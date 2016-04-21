#include <Mouse.h>
#include <Wire.h>

#include "mpu6050.h"

// We only consider values from the gyroscope larger than GYRO_THRESHOLD
#define GYRO_THRESHOLD 1000
#define MAX_X_MOUSE_VELOCITY 10
#define MAX_Y_MOUSE_VELOCITY 10

/* ################## Custom Types ################## */

enum click_state {
  pressed,
  released
};

typedef struct gyro_state_s {
  int x; 
  int y;
  int z;
} gyro_state;

typedef struct mouse_state_s {
  short velocity_x; // Left/Right speed
  short velocity_y; // Up/Down speed
  bool scroll;    // Toggles scroll mode 
  click_state mouse1;  // Left click
  click_state mouse2;  // Right click
} mouse_state;


/* ################## Global Variables ################## */

bool first_run = true;
mouse_state global_mouse_state;

/* ################## Functions ################## */

/* A simple function to set register values. See setup() */
void set_register(byte reg, byte value) {
  Wire.beginTransmission(MPU_6050_ADDRESS); 
  Wire.write(reg);
  Wire.write(value); 
  Wire.endTransmission(true);
}

/* Return a mouse state with default values */
mouse_state initial_mouse_state() {

  mouse_state state;

  state.velocity_x = 0;
  state.velocity_y = 0;
  state.scroll = false;
  state.mouse1 = released;
  state.mouse2 = released;

  return state;
}

int sign(int number) {
  if (number >= 0)
    return 1;
  else
    return -1;
}

int gyro_change(int value) {
  if (abs(value) > GYRO_THRESHOLD) 
    return (1 * sign(value));
  return 0;
}

/* Sets the current state of the mouse. Checks to see if buttons are
 * pressed, what the gyroscope values are, etc*/
mouse_state get_mouse_state(mouse_state mstate, gyro_state gstate) {

  /*mouse_state mstate = initial_mouse_state();*/

  int change_in_x = gyro_change(gstate.x);
  mstate.velocity_x += change_in_x;

  int change_in_y = gyro_change(gstate.y);
  mstate.velocity_y -= change_in_y;

  constrain(mstate.velocity_x, 0, MAX_X_MOUSE_VELOCITY);
  constrain(mstate.velocity_y, 0, MAX_Y_MOUSE_VELOCITY);

  /*TODO: 
    - Figure out which pins do what (ie, clicking)*/

  return mstate;

}

/* This function is called whenever we update the cursor on the screen 
   This is where we actually move the mouse */
void set_cursor_state(mouse_state state) {

  Mouse.move(state.velocity_x, state.velocity_y, false);

  /*TODO: 
    - Set Mouse.whatever() based on mouse_state */

}

/* Get X/Y/Z values from the gyroscope */
gyro_state get_gyro_xyz() {

  Wire.beginTransmission(MPU_6050_ADDRESS);
  Wire.write(0x43);  // starting with register 0x43 (GYRO_XOUT)
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_6050_ADDRESS, 6, true);  // request a total of 6 registers
  int GyX=Wire.read()<<8|Wire.read();  // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  int GyY=Wire.read()<<8|Wire.read();  // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  int GyZ=Wire.read()<<8|Wire.read();  // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)

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

String click_to_string(click_state s) {
  if (s == pressed)
    return "pressed";
  else if (s == released)
    return "released";
  return "?";
}

char bool_to_char(bool b) {
  if (b)
    return 'T';
  else
    return 'F';
}

void print_mouse_state(mouse_state state) {
  Serial.println("");
  Serial.print("velocity_x = "); Serial.println(state.velocity_x);
  Serial.print("velocity_y = "); Serial.println(state.velocity_y);
  Serial.print("scroll_on  = "); Serial.println(bool_to_char(state.scroll));
  Serial.print("mouse1     = "); Serial.println(click_to_string(state.mouse1));
  Serial.print("mouse2     = "); Serial.println(click_to_string(state.mouse2));
  Serial.println("");
}


/* ################## Main Program ################## */

void setup() {
  
  Wire.begin();
  Serial.begin(9600);
  Mouse.begin();

  global_mouse_state = initial_mouse_state();

  // Disable temperature sensor 
  set_register(REG_PWR_MGMT_1, B1000);

  // Set FS_SEL to +- 2000 (this seems to reduce sensitivity)
  set_register(REG_GYRO_CONFIG, B11000); 
}

void loop() {

  gyro_state gstate = get_gyro_xyz();
  global_mouse_state = get_mouse_state(global_mouse_state, gstate);

  //print_gyro_state(gstate);
  print_mouse_state(global_mouse_state);

  set_cursor_state(global_mouse_state);

  delay(10);  
}
