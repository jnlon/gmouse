#include <Mouse.h>
#include <Wire.h>
#include <Keyboard.h>
#include <math.h>

#include "mpu6050.h"

// The minimum/maximum stored gyro value in mouse_state
#define DEGREE_MAX_RANGE 360

// What multiple of DEGREE_MAX_RANGE counts as a velocity increase
#define DEGREE_VELOCITY_MULTIPLE 30

// How much we have to go past a velocity multiple, in that direction, to
// increment velocity. This prevents jitter from hand-shaking
#define EXTRA_DEGREE_BUMP 5

#define MOUSE_LEFT_CLICK_PIN 4
#define MOUSE_RIGHT_CLICK_PIN 5
#define MOUSE_BACK_CLICK_PIN 6
#define MOUSE_SWITCH_PWR_PIN 7

/* ################## Custom Types ################## */

typedef struct sensor_data_s {
  int ax; 
  int ay;
  int az;
} sensor_data;

typedef struct degree_xy_s {
  int x; 
  int y;
} degree_xy;

typedef struct mouse_state_s {
  int degree_x;              // Tilt in degrees
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

int velocity_from_degree(int degree, int direction) {
  return ((degree + (EXTRA_DEGREE_BUMP*direction)) / (DEGREE_VELOCITY_MULTIPLE));
}

/* Sets the current state of the mouse. Checks to see if buttons are
   pressed, what the gyroscope values are, etc*/
mouse_state get_mouse_state(mouse_state mstate, degree_xy degree) {

  // What orientation we were at last time we were here
  int old_degree_x = mstate.degree_x;
  int old_degree_y = mstate.degree_y;

  mstate.degree_x = constrain(degree.x, (-1)*DEGREE_MAX_RANGE, DEGREE_MAX_RANGE);
  mstate.degree_y = constrain(degree.y, (-1)*DEGREE_MAX_RANGE, DEGREE_MAX_RANGE);

  int direction_x = old_degree_x - mstate.degree_x;
  int direction_y = old_degree_y - mstate.degree_y;

  // Update the velocity
  mstate.velocity_x = velocity_from_degree(mstate.degree_x, sign(direction_x));
  mstate.velocity_y = velocity_from_degree(mstate.degree_y, sign(direction_y));

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

/* Get X/Y/Z values from the accelerometer */
sensor_data get_degree_xyz() {

  Wire.beginTransmission(MPU_6050_ADDRESS);
  Wire.write(0x3B);  // starting with register 3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_6050_ADDRESS, 6, true);  // request a total of 14 registers
  int Ax_raw = (Wire.read() << 8) | Wire.read();  // 0x43 (ACCEL_XOUT_H) & 0x44 (ACCEL_XOUT_L)
  int Ay_raw = (Wire.read() << 8) | Wire.read();  // 0x45 (ACCEL_YOUT_H) & 0x46 (ACCEL_YOUT_L)
  int Az_raw = (Wire.read() << 8) | Wire.read();  // 0x47 (ACCEL_ZOUT_H) & 0x48 (ACCEL_ZOUT_L)

  struct sensor_data_s state;

  // Relative to how we orient it, see orienttions.txt
  state.ax = Az_raw;
  state.ay = Ax_raw;
  state.az = Ay_raw;

  return state;
}

/* Convert raw accelerometer data to degree */
degree_xy degree_from_accel(sensor_data data) {

  degree_xy degree;

  // TODO: Make this simpler
  degree.x = ((((data.ax / 100)*10)/2)-90)*((-1)*sign(data.az));
  degree.y = ((data.ay / 100)*10)/2;

  return degree;
}

/* Print the state of the accelerometer */
void print_sensor_data(sensor_data state) {
  Serial.print("Ax = "); Serial.print(state.ax);
  Serial.print(" | Ay = "); Serial.print(state.ay);
  Serial.print(" | Az = "); Serial.print(state.az);
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
  
  // Initialize libraries
  Wire.begin();
  Serial.begin(9600);
  Mouse.begin();
  Keyboard.begin();

  // Setup pins
  int in_pins[] = { 
    MOUSE_LEFT_CLICK_PIN, 
    MOUSE_RIGHT_CLICK_PIN, 
    MOUSE_BACK_CLICK_PIN 
  };

  set_pins_mode(INPUT, in_pins, 3);
  pinMode(MOUSE_SWITCH_PWR_PIN, OUTPUT);
  digitalWrite(MOUSE_SWITCH_PWR_PIN, HIGH);

  // Setup global variables
  global_mouse_state = initial_mouse_state();

  // Disable temperature sensor 
  set_mpu_register(REG_PWR_MGMT_1, B1000);

  // Set FS_SEL to +- 2000 degree/s (maximum it can detect)
  set_mpu_register(REG_GYRO_CONFIG, B11000); 

  // Set AFS_SEL to +- 2g 
  set_mpu_register(REG_ACCEL_CONFIG, B11000); 

  // Set SMPRT_DIV (sample rate divider) 1000HZ/(9 + 1)
  set_mpu_register(REG_SMPRT_DIV, B1001); 

  // Set DLPF_CFG in CONFIG 
  set_mpu_register(REG_CONFIG, B001); 

  
}

void loop() {

  // Get raw XYZ values from accelerometer
  sensor_data accel_state = get_degree_xyz();

  degree_xy degrees_now = degree_from_accel(accel_state);

  // print_sensor_data(change);

  global_mouse_state = get_mouse_state(global_mouse_state, degrees_now);

  // print_sensor_data(gstate);

  print_mouse_state(global_mouse_state);

  set_cursor_state(global_mouse_state);

  //delay(1);  
}
