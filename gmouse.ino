#include <Mouse.h>
#include <Wire.h>
#include <Keyboard.h>
#include <math.h>

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
#define MICROSECONDS_BETWEEN_SAMPLES 1000.0
#define SECONDS_ACROSS_SAMPLES ((MICROSECONDS_BETWEEN_SAMPLES/1000000.0)*(NUMBER_OF_SAMPLES-1))

// Full Scale Range sensitivity (see page 31 of register spec)
#define GYRO_LSB_SENSITIVITY 16.4
#define ACCEL_LSB_SENSITIVITY 16384

/* ################## Custom Types ################## */

typedef struct sensor_data_s {
  int gx; 
  int gy;
  int gz;
  int ax; 
  int ay;
  int az;
} sensor_data;

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
mouse_state get_mouse_state(mouse_state mstate, sensor_data gdegree) {

  // Log changes in gstate if over noise threshold
  mstate.degree_x += gdegree.gx;
  mstate.degree_y -= gdegree.gy;

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

void print_gyro_samples(sensor_data *gs) {

  for (int i=0;i<NUMBER_OF_SAMPLES;i++) {
    Serial.print(gs[i].gx); Serial.print(' ');
    Serial.print(gs[i].gy); Serial.print(' ');
    Serial.print(gs[i].gz); Serial.println(' ');
  }

}

/* Aproximates the area under a graph of points X/Y/Z in g1 and g2, with
   time_elapsed as the x difference. Uses simpson's method */
sensor_data area_under_curve(sensor_data gstates[]) {

  sensor_data change;

  if ((NUMBER_OF_SAMPLES % 2) != 0) {
    Serial.println("Cannot find area under curve, need even NUMBER_OF_SAMPLES!");
    return change;
  }

  // Conventional variable names for integration
  float a = 0;
  float b = SECONDS_ACROSS_SAMPLES;
  int n = NUMBER_OF_SAMPLES;
  float x = (b - a) / n;

  //Serial.print("x: "); Serial.println(x, 8);
  //Serial.print("ax "); Serial.println(gstates[0].ax, 8);
  //Serial.print("ax "); Serial.println(gstates[n-1].ax, 8);

  // Accumulate Xo and Xn
  change.gx = gstates[0].gx + gstates[n-1].gx;
  change.gy = gstates[0].gy + gstates[n-1].gy;
  change.gz = gstates[0].gz + gstates[n-1].gz;

  change.ax = gstates[0].ax + gstates[n-1].ax;
  change.ay = gstates[0].ay + gstates[n-1].ay;
  change.az = gstates[0].az + gstates[n-1].az;
  

  // Apply Simpson's rule for middle terms
  for (int i=1;i<NUMBER_OF_SAMPLES-2; i+=2) {
    //Serial.print("ax "); Serial.println(gstates[i].ax, 8);
    change.gx += (4 * gstates[i].gx) + (2 * gstates[i+1].gx);
    change.gy += (4 * gstates[i].gy) + (2 * gstates[i+1].gy);
    change.gz += (4 * gstates[i].gz) + (2 * gstates[i+1].gz);

    change.ax += (4 * gstates[i].ax) + (2 * gstates[i+1].ax);
    change.ay += (4 * gstates[i].ay) + (2 * gstates[i+1].ay);
    change.az += (4 * gstates[i].az) + (2 * gstates[i+1].az);
  }

  change.gx = (x/3)*change.gx;
  change.gy = (x/3)*change.gy;
  change.gz = (x/3)*change.gz;

  change.ax = (x/3)*change.ax;
  change.ay = (x/3)*change.ay;
  change.az = (x/3)*change.az;
  

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

/* Get X/Y/Z values from gyroscope (degrees/s) and accelerometer */
sensor_data get_gyro_accel_xyz() {

  Wire.beginTransmission(MPU_6050_ADDRESS);
  Wire.write(0x3B);  // starting with register 3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_6050_ADDRESS, 14, true);  // request a total of 14 registers
  int Ax = (Wire.read() << 8) | Wire.read();  // 0x43 (ACCEL_XOUT_H) & 0x44 (ACCEL_XOUT_L)
  int Ay = (Wire.read() << 8) | Wire.read();  // 0x45 (ACCEL_YOUT_H) & 0x46 (ACCEL_YOUT_L)
  int Az = (Wire.read() << 8) | Wire.read();  // 0x47 (ACCEL_ZOUT_H) & 0x48 (ACCEL_ZOUT_L)
  Wire.read() ; Wire.read();                  // Ignore temperature readings
  int Gx = (Wire.read() << 8) | Wire.read();  // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  int Gy = (Wire.read() << 8) | Wire.read();  // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  int Gz = (Wire.read() << 8) | Wire.read();  // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)

  struct sensor_data_s state;

  state.ax = (Ax / ACCEL_LSB_SENSITIVITY);
  state.ay = (Ay / ACCEL_LSB_SENSITIVITY);
  state.az = (Az / ACCEL_LSB_SENSITIVITY);

  state.gx = (Gx / GYRO_LSB_SENSITIVITY);
  state.gy = (Gy / GYRO_LSB_SENSITIVITY);
  state.gz = (Gz / GYRO_LSB_SENSITIVITY);

  return state;
}


/* Print the state of the gyroscope */
void print_sensor_data(sensor_data state) {
  Serial.print("Ax = "); Serial.print(state.ax);
  Serial.print(" | Ay = "); Serial.print(state.ay);
  Serial.print(" | Az = "); Serial.print(state.az);
  Serial.println("");
  Serial.print("Gx = "); Serial.print(state.gx);
  Serial.print(" | Gy = "); Serial.print(state.gy);
  Serial.print(" | Gz = "); Serial.print(state.gz);
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

  // Set FS_SEL to +- 2000 degree/s (maximum it can detect)
  set_mpu_register(REG_GYRO_CONFIG, B11000); 

  // Set AFS_SEL to +- 2g 
  set_mpu_register(REG_ACCEL_CONFIG, B00000); 

  // Set SMPRT_DIV (sample rate divider) 1000HZ/(9 + 1)
  set_mpu_register(REG_SMPRT_DIV, B1001); 

  // Set DLPF_CFG in CONFIG 
  set_mpu_register(REG_CONFIG, B001); 
}

void loop() {

  sensor_data gstates[NUMBER_OF_SAMPLES];

  // Sample gyroscope X/Y/Z readings, with a pause each time 
  for (int i=0;i<NUMBER_OF_SAMPLES;i++) {
    gstates[i] = get_gyro_accel_xyz();
    if (i == NUMBER_OF_SAMPLES-1) 
      break;
    delayMicroseconds(MICROSECONDS_BETWEEN_SAMPLES);
  }

  //print_gyro_samples(gstates);
  //Serial.println(' ');

  //return;

  sensor_data change = area_under_curve(gstates);

  //int force_magnitude_approx = abs(gyro_degree_change.gx) + abs(gyro_degree_change.gy) + abs(gyro_degree_change.gz);
  //Serial.println(force_magnitude_approx);

  //X/Y
  float yAcc = atan2f((float)change.gy, (float)change.gz) * 180 / M_PI;
  //Serial.print("before: "); Serial.println(gyro_degree_change.gy);
  change.gy = change.gy * 0.98 + yAcc * 0.02;

  float xAcc = atan2f((float)change.gx, (float)change.gz) * 180 / M_PI;
  Serial.print("before: "); Serial.println(change.gy);
  change.gx = change.gx * 0.98 + xAcc * 0.02;
  Serial.print("yacc: "); Serial.println(yAcc);
  Serial.print("after: "); Serial.println(change.gy);

  //return;

  print_sensor_data(change);

  global_mouse_state = get_mouse_state(global_mouse_state, change);

  //print_sensor_data(gstate);

  print_mouse_state(global_mouse_state);

  set_cursor_state(global_mouse_state);

  //delay(1);  
}
