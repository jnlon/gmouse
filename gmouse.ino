#include <Mouse.h>
#include <Wire.h>
#include <Keyboard.h>

#include "mpu6050.h"

// We only consider values from the gyroscope larger than GYRO_THRESHOLD
#define GYRO_IGNORE_UNDER 50

#define VELOCITY_NOTCHES 10L 
#define GYRO_LEVEL_RANGE 200L 

#define SENSITIVITY 1
#define STOP_THRESHOLD 0

#define MOUSE_SWITCH_PWR_PIN 7
#define MOUSE_LEFT_CLICK_PIN 4
#define MOUSE_RIGHT_CLICK_PIN 5
#define MOUSE_BACK_CLICK_PIN 6

/* ################## Custom Types ################## */

typedef struct gyro_state_s {
  int x; 
  int y;
  int z;
} gyro_state;

typedef struct mouse_state_s {
  long gyro_x;              // Accumulate significant gyroscope values
  long gyro_y; 
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

  state.gyro_x = 0;
  state.gyro_y = 0;
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

int gyro_change(int value) {
  if (abs(value) > GYRO_IGNORE_UNDER) 
    return (SENSITIVITY * sign(value));
  return 0;
}

long velocity_now(long gyro_value) {
  long s = sign(gyro_value);
  long v = abs(gyro_value);
  return ((v / (GYRO_LEVEL_RANGE / VELOCITY_NOTCHES))*s);
}

/* Sets the current state of the mouse. Checks to see if buttons are
 * pressed, what the gyroscope values are, etc*/
mouse_state get_mouse_state(mouse_state mstate, gyro_state gstate) {

  /*
  int change_in_x = gyro_change(gstate.x);
  mstate.velocity_x += change_in_x;

  int change_in_y = gyro_change(gstate.y);
  mstate.velocity_y -= change_in_y;

  mstate.velocity_x = constrain(mstate.velocity_x, (MAX_X_MOUSE_VELOCITY*(-1)), MAX_X_MOUSE_VELOCITY);
  mstate.velocity_y = constrain(mstate.velocity_y, (MAX_Y_MOUSE_VELOCITY*(-1)), MAX_Y_MOUSE_VELOCITY);
  */

  // Log changes in gstate if over noise threshold
  mstate.gyro_x += gyro_change(gstate.x);
  mstate.gyro_y += gyro_change(gstate.y);

  // Update the velocity
  mstate.velocity_x = velocity_now(mstate.gyro_x);
  mstate.velocity_y = velocity_now(mstate.gyro_y);

  Serial.println(mstate.velocity_x);
  Serial.println(mstate.velocity_y);

  mstate.mouse_left  = digitalRead(MOUSE_LEFT_CLICK_PIN);  //left
  mstate.mouse_right = digitalRead(MOUSE_RIGHT_CLICK_PIN); //right
  mstate.mouse_back  = digitalRead(MOUSE_BACK_CLICK_PIN);  //dedicated back page key

  return mstate;

}

/* This function is called whenever we update the cursor on the screen 
   This is where we actually move/click the mouse */
void set_cursor_state(mouse_state state) {

  int velocity_x = state.velocity_x-(STOP_THRESHOLD*sign(state.velocity_x));
  int velocity_y = state.velocity_y-(STOP_THRESHOLD*sign(state.velocity_y));

  if (abs(state.velocity_x) < STOP_THRESHOLD)
    velocity_x = 0;
  
  if (abs(state.velocity_y) < STOP_THRESHOLD)
    velocity_y = 0;
 
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

  Mouse.move(velocity_x, velocity_y, false);
}

/* Get X/Y/Z values from the gyroscope */
gyro_state get_gyro_xyz() {

  Wire.beginTransmission(MPU_6050_ADDRESS);
  Wire.write(0x43);  // starting with register 0x43 (GYRO_XOUT)
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_6050_ADDRESS, 6, true);  // request a total of 6 registers
  int GyX = (Wire.read() << 8) | Wire.read();  // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  int GyY = (Wire.read() << 8) | Wire.read();  // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  int GyZ = (Wire.read() << 8) | Wire.read();  // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)

  struct gyro_state_s state;

  state.x = GyX;
  state.y = GyY;
  state.z = GyZ;

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
  Serial.print("gyro_x             = "); Serial.println(state.gyro_x);
  Serial.print("gyro_y             = "); Serial.println(state.gyro_y);
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

  // TODO: properly store these pin values, don't just hard code
  int in_pins[] = {4, 5, 6};
  set_pins_mode(INPUT, in_pins, 3);

  pinMode(MOUSE_SWITCH_PWR_PIN, OUTPUT);
  digitalWrite(MOUSE_SWITCH_PWR_PIN, HIGH);

  global_mouse_state = initial_mouse_state();

  // Disable temperature sensor 
  set_mpu_register(REG_PWR_MGMT_1, B1000);

  // Set FS_SEL to +- 2000 (this seems to reduce sensitivity)
  set_mpu_register(REG_GYRO_CONFIG, B11000); 
}

void loop() {
  gyro_state gstate = get_gyro_xyz();

  global_mouse_state = get_mouse_state(global_mouse_state, gstate);

  //print_gyro_state(gstate);

  print_mouse_state(global_mouse_state);

  set_cursor_state(global_mouse_state);

  delay(10);  
}
