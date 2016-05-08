#include <Mouse.h>
#include <Wire.h>
#include <Keyboard.h>

#include "mpu6050.h"


// The minimum/maximum stored gyro value in mouse_state
#define DEGREE_MAX_RANGE 360

// What multiple of DEGREE_MAX_RANGE counts as a velocity increase
#define DEGREE_VELOCITY_MULTIPLE 6

// At velocity zero, how much extra gyro count we need to add to overcome it
#define DEGREE_STOP_BUMP 0 

#define MOUSE_LEFT_CLICK_PIN 4
#define MOUSE_RIGHT_CLICK_PIN 5
#define MOUSE_BACK_CLICK_PIN 6
#define MOUSE_SWITCH_PWR_PIN 7

// Number of gyroscope queries before calculating change (must be even!)
#define NUMBER_OF_SAMPLES 10

// Artificial delay between samples (microseconds, keep < 16383) 
#define MICROSECONDS_BETWEEN_SAMPLES 12000.0
#define SECONDS_ACROSS_SAMPLES ((MICROSECONDS_BETWEEN_SAMPLES/1000000.0)*(NUMBER_OF_SAMPLES-1))

// Full Scale Range sensitivity (see page 31 of register spec)
#define LSB_SENSITIVITY 131

/* ################## Custom Types ################## */

typedef struct gyro_state_s {
  int x; 
  int y;
  int z;
} gyro_state;

typedef struct mouse_state_s {
  int degree_x;             // Tilt in degrees
  int degree_y; 
  int velocity_x;           // How fast/slow cursor is moving
  int velocity_y; 
  int mouse_left;           // Left click
  int mouse_right;          // Right click
  int mouse_back;           // back page
} mouse_state;


/* ################## Global Variables ################## */

mouse_state global_mouse_state;

/* ################## Functions ################## */

/* A simple function to set register values. See setup() */
void set_mpu_register(byte reg, byte value) {
  Wire.beginTransmission(MPU_6050_ADDRESS); 
  Wire.write(reg);
  Wire.write(value); 
  Wire.endTransmission(true);
}

/* Return a mouse state with default values */
mouse_state initial_mouse_state() {

  mouse_state state;

  state.degree_x = 0;
  state.degree_y = 0;
  state.velocity_x = 0;
  state.velocity_y = 0;
  state.mouse_left = LOW;
  state.mouse_right = LOW;
  state.mouse_back = LOW;

  return state;
}

int sign(int number) {
  if (number >= 0)
    return 1;
  else
    return -1;
}

boolean between(long value, long low, long high) {
  if (value > low && value < high)
    return true;
  return false;
}

long velocity_now(long value) {
  // Apply speed bump 
  if (between(value, (-1)*DEGREE_VELOCITY_MULTIPLE, DEGREE_VELOCITY_MULTIPLE)) {
    int direction = sign(value);
    value -= DEGREE_STOP_BUMP*direction;
  }

  return ((value / DEGREE_VELOCITY_MULTIPLE));
}

/* Sets the current state of the mouse. Checks to see if buttons are
   pressed, what the gyroscope values are, etc*/
mouse_state get_mouse_state(mouse_state mstate, gyro_state gdegree) {

  // Log changes in gstate if over noise threshold
  mstate.degree_x += gdegree.x;
  mstate.degree_y -= gdegree.y;

  mstate.degree_x = constrain(mstate.degree_x, (-1)*DEGREE_MAX_RANGE, DEGREE_MAX_RANGE);
  mstate.degree_y = constrain(mstate.degree_y, (-1)*DEGREE_MAX_RANGE, DEGREE_MAX_RANGE);

  // Update the velocity
  mstate.velocity_x = velocity_now(mstate.degree_x);
  mstate.velocity_y = velocity_now(mstate.degree_y);

  /*
  // Slows down (and eventually stops) the mouse when we are moving slowly
  // TODO: Clean this up
  if (abs(mstate.velocity_x) <= 1 && mstate.velocity_x != 0 && mstate.degree_x != 0)
    mstate.degree_x -= 1*sign(mstate.velocity_x);

  if (abs(mstate.velocity_y) <= 1 && mstate.velocity_y != 0 && mstate.degree_y != 0)
    mstate.degree_y -= 1*sign(mstate.velocity_y);
  */

  mstate.mouse_left  = digitalRead(MOUSE_LEFT_CLICK_PIN);  //left
  mstate.mouse_right = digitalRead(MOUSE_RIGHT_CLICK_PIN); //right
  mstate.mouse_back  = digitalRead(MOUSE_BACK_CLICK_PIN);  //dedicated back page key

  return mstate;

}

boolean reset_buttons_pressed(mouse_state state) {
  if (state.mouse_left == HIGH && 
      state.mouse_right == HIGH && 
      state.mouse_back == HIGH)
    return true;
  else 
    return  false;
}

boolean scroll_buttons_pressed(mouse_state state) {
  if (state.mouse_left == HIGH && 
      state.mouse_right == HIGH)
    return true;
  else 
    return  false;
}

float average(long x1, long x2)  {
  return ((x1 + x2) / 2);
}

void print_samples(gyro_state *gs) {

  for (int i=0;i<NUMBER_OF_SAMPLES;i++) {
    Serial.print(gs[i].x); Serial.print(' ');
    Serial.print(gs[i].y); Serial.print(' ');
    Serial.print(gs[i].z); Serial.println(' ');
  }

}

/* Aproximates the area under a graph of points X/Y/Z in g1 and g2, with
   time_elapsed as the x difference. Uses simpson's method */
gyro_state area_under_curve(gyro_state gstates[]) {

  gyro_state change;

  if ((NUMBER_OF_SAMPLES % 2) != 0) {
    Serial.println("Cannot find area under curve, need even NUMBER_OF_SAMPLES!");
    return change;
  }

  // Conventional variable names for integration
  float a = 0;
  float b = SECONDS_ACROSS_SAMPLES;
  int n = NUMBER_OF_SAMPLES;
  float x = (b - a) / n;

  Serial.print("x: "); Serial.println(x, 8);
  Serial.print("x/3.0: "); Serial.println(x/3.0f, 8);

  // Accumulate Xo and Xn
  change.x = gstates[0].x + gstates[n].x;
  change.y = gstates[0].y + gstates[n].y;
  change.z = gstates[0].z + gstates[n].z;

  // Apply Simpson's rule for middle terms
  for (int i=1;i<NUMBER_OF_SAMPLES-1; i+=2) {
    change.x += (4 * gstates[i].x) + (2 * gstates[i+1].x);
    change.y += (4 * gstates[i].y) + (2 * gstates[i+1].y);
    change.z += (4 * gstates[i].z) + (2 * gstates[i+1].z);
  }

  change.x = (x/3)*change.x;
  change.y = (x/3)*change.y;
  change.z = (x/3)*change.z;

  return change;

}

/* This function is called whenever we update the cursor on the screen 
   This is where we actually move/click the mouse */
void set_cursor_state(mouse_state state) {

  if (reset_buttons_pressed(state)) {
    global_mouse_state = initial_mouse_state();
    return;
  }

  // Paramaters passed to Mouse.move()
  boolean scroll_mode = scroll_buttons_pressed(state);
  long velocity_x = state.velocity_x;
  long velocity_y = state.velocity_y;
 
  if (state.mouse_left == HIGH) 
    Mouse.press(MOUSE_LEFT); 
  else if (state.mouse_left == LOW) 
    Mouse.release(MOUSE_LEFT); 

  else if (state.mouse_right == HIGH)
    Mouse.press(MOUSE_RIGHT); 
  else if (state.mouse_right == LOW) 
    Mouse.release(MOUSE_RIGHT); 

  else if (state.mouse_back == HIGH)
  {
    Keyboard.press(KEY_LEFT_ALT);
    Keyboard.press(KEY_LEFT_ARROW);
    Keyboard.releaseAll();
  }

  Mouse.move(velocity_x, velocity_y, scroll_mode);
}

/* Get X/Y/Z values from gyroscope in degrees/s */
gyro_state get_gyro_xyz() {

  Wire.beginTransmission(MPU_6050_ADDRESS);
  Wire.write(0x43);  // starting with register 0x43 (GYRO_XOUT)
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_6050_ADDRESS, 6, true);  // request a total of 6 registers
  int GyX = (Wire.read() << 8) | Wire.read();  // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  int GyY = (Wire.read() << 8) | Wire.read();  // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  int GyZ = (Wire.read() << 8) | Wire.read();  // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)

  struct gyro_state_s state;

  state.x = (GyX / LSB_SENSITIVITY);
  state.y = (GyY / LSB_SENSITIVITY);
  state.z = (GyZ / LSB_SENSITIVITY);

  return state;
}


/* Print the state of the gyroscope */
void print_gyro_state(gyro_state state) {
  Serial.print("GyX = "); Serial.print(state.x);
  Serial.print(" | GyY = "); Serial.print(state.y);
  Serial.print(" | GyZ = "); Serial.print(state.z);
  Serial.println("");
}

void print_mouse_state(mouse_state state) {
  Serial.println("");
  Serial.print("degree_x             = "); Serial.println(state.degree_x);
  Serial.print("degree_y             = "); Serial.println(state.degree_y);
  Serial.print("velocity_x         = "); Serial.println(state.velocity_x);
  Serial.print("velocity_y         = "); Serial.println(state.velocity_y);
  Serial.print("mouse_left         = "); Serial.println(state.mouse_left);
  Serial.print("mouse_right        = "); Serial.println(state.mouse_right);
  Serial.print("mouse_back         = "); Serial.println(state.mouse_back);
  Serial.println("");
}

void set_pins_mode(int mode, int pins[], int len) {
  for (int i=0;i<len;i++)
    pinMode(pins[i], mode);
}

void set_pins_output(int mode, int pins[], int len) {
  for (int i=0;i<len;i++)
    digitalWrite(pins[i], mode);
}

/* ################## Main Program ################## */

void setup() {
  
  Wire.begin();
  Serial.begin(9600);
  Mouse.begin();
  Keyboard.begin();

  int in_pins[] = { 
    MOUSE_LEFT_CLICK_PIN, 
    MOUSE_RIGHT_CLICK_PIN, 
    MOUSE_BACK_CLICK_PIN 
  };

  set_pins_mode(INPUT, in_pins, 3);

  pinMode(MOUSE_SWITCH_PWR_PIN, OUTPUT);
  digitalWrite(MOUSE_SWITCH_PWR_PIN, HIGH);

  global_mouse_state = initial_mouse_state();

  // Disable temperature sensor 
  set_mpu_register(REG_PWR_MGMT_1, B1000);

  // Set FS_SEL to +- 250 (most accurate for small readings)
  set_mpu_register(REG_GYRO_CONFIG, B00000); 

  // Set SMPRT_DIV (sample rate divider) to 0 (8KHz)
  set_mpu_register(REG_SMPRT_DIV, B0); 

  // Set DLPF_CFG in CONFIG 
  set_mpu_register(REG_CONFIG, B111); 

}

void loop() {

  gyro_state gstates[NUMBER_OF_SAMPLES];

  // Sample gyroscope X/Y/Z readings, with a pause each time 
  for (int i=0;i<NUMBER_OF_SAMPLES;i++) {
    gstates[i] = get_gyro_xyz();
    if (i == NUMBER_OF_SAMPLES-1) 
      break;
    delayMicroseconds(MICROSECONDS_BETWEEN_SAMPLES);
  }

  //print_samples(gstates);
  //Serial.println(' ');

  return;

  gyro_state gyro_degree_change = area_under_curve(gstates);

  //print_gyro_state(change);

  global_mouse_state = get_mouse_state(global_mouse_state, gyro_degree_change);

  //print_gyro_state(gstate);

  print_mouse_state(global_mouse_state);

  set_cursor_state(global_mouse_state);

  //delay(1);  
}
