#include <Mouse.h>
#include <Wire.h>
#include <Keyboard.h>
#include <math.h>

#include "mpu6050.h"

#define MOUSE_LEFT_CLICK_PIN 4
#define MOUSE_RIGHT_CLICK_PIN 5
#define MOUSE_BACK_CLICK_PIN 6
#define MOUSE_SWITCH_PWR_PIN 7

#define LED_PWR_PIN 9
#define LED_RED_PIN 10
#define LED_GREEN_PIN 11
#define LED_BLUE_PIN 12

/* ################## Custom Types ################## */

typedef struct xyz_s {
  int x; 
  int y;
  int z;
} xyz;

typedef struct mouse_state_s {
  int velocity_x;           // How fast/slow cursor is moving
  int velocity_y; 
  int mouse_left;           // Left click
  int mouse_right;          // Right click
  int mouse_back;           // back page
} mouse_state;

/* ################## Global Variables ################## */

int all_led_color_pins[] = {LED_RED_PIN, LED_GREEN_PIN, LED_BLUE_PIN};
mouse_state global_mouse_state;
boolean mouse_left_down = false;
boolean mouse_right_down = false;
boolean mouse_back_down = false;

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

  state.velocity_x = 0;
  state.velocity_y = 0;
  state.mouse_left = LOW;
  state.mouse_right = LOW;
  state.mouse_back = LOW;

  return state;
}

void update_led(mouse_state state) {

  if (state.velocity_x != 0)      // X must be moving
    enable_led_pin(LED_GREEN_PIN);
  else if (state.velocity_y != 0) // Y must be moving
    enable_led_pin(LED_BLUE_PIN);
  else                            // Either both, or neither are moving
    enable_led_pin(LED_RED_PIN); 
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

void enable_led_pin(int pin) {
  for (int i=0;i<3;i++)
    digitalWrite(all_led_color_pins[i], LOW);
  digitalWrite(pin, HIGH);
}

/* Given a raw accelerometer value v, returns a scaled down version that we can
   use as a velocity for the mouse cursor */
xyz mouse_velocity_from_accel(xyz accel) {

  xyz velocity; 
  velocity.z = -1; 

  // TODO: define these values, don't hard-code!
  int velrange = 6;
  int gyrrange = 16000;

  // Y velocity
  velocity.y = map(map(accel.y, -gyrrange+200, gyrrange-200, 0, 5000), 0, 5000, -velrange, velrange);

  // X velocity
  velocity.x = map(map(accel.x, 0, gyrrange, 0, 5000), 5000, 0, 0, velrange);
  velocity.x *= sign(accel.z);

  return velocity;
}

/* Sets the current state of the mouse. Checks to see if buttons are
   pressed, what the accelerometer values are, etc*/
mouse_state get_mouse_state(mouse_state mstate, xyz raw_accel) {

  // Update velocity
  xyz velocity = mouse_velocity_from_accel(raw_accel);
  int old_vx = mstate.velocity_x;
  int old_vy = mstate.velocity_y;

  // Implement velocity axis lock
  if (old_vx == 0 && old_vy == 0) {
    if (velocity.x != 0) 
      mstate.velocity_x = velocity.x;
    else 
      mstate.velocity_y = velocity.y;
  }
  else if (old_vx != 0)
    mstate.velocity_x = velocity.x;
  else if (old_vy != 0)
    mstate.velocity_y = velocity.y;

  mstate.mouse_left  = digitalRead(MOUSE_LEFT_CLICK_PIN);  //left
  mstate.mouse_right = digitalRead(MOUSE_RIGHT_CLICK_PIN); //right
  mstate.mouse_back  = digitalRead(MOUSE_BACK_CLICK_PIN);  //dedicated back page key

  print_xyz("raw_accel", raw_accel);
  print_xyz("velocity", velocity);

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

  update_led(state);

  // Parameters passed to Mouse.move()
  boolean scroll_mode = scroll_buttons_pressed(state);
  long velocity_x = state.velocity_x;
  long velocity_y = state.velocity_y;
 
  if (state.mouse_left == HIGH && mouse_left_down == false) { 
    Mouse.press(MOUSE_LEFT); 
    mouse_left_down = true;
  }
  else if (state.mouse_left == LOW && mouse_left_down == true)  {
    Mouse.release(MOUSE_LEFT); 
    mouse_left_down = false;
  }

  if (state.mouse_right == HIGH && mouse_right_down == false) {
    Mouse.press(MOUSE_RIGHT); 
    mouse_right_down = true;
  }
  else if (state.mouse_right == LOW && mouse_right_down == true) {
    Mouse.release(MOUSE_RIGHT); 
    mouse_right_down = false;
  }

  if (state.mouse_back == HIGH && (mouse_back_down) == false)
  {
    Keyboard.press(KEY_LEFT_ALT);
    Keyboard.press(KEY_LEFT_ARROW);
    Keyboard.releaseAll();
    mouse_back_down = true;
  }
  else if (state.mouse_back == LOW && mouse_back_down == true) {
    mouse_back_down = false;
  }

  Mouse.move(velocity_x, velocity_y, scroll_mode);
}

/* Get X/Y/Z values from the accelerometer */
xyz get_accel_xyz() {

  Wire.beginTransmission(MPU_6050_ADDRESS);
  Wire.write(0x3B);  // starting with register 3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_6050_ADDRESS, 6, true);  // request a total of 14 registers
  int Ax_raw = (Wire.read() << 8) | Wire.read();  // 0x43 (ACCEL_XOUT_H) & 0x44 (ACCEL_XOUT_L)
  int Ay_raw = (Wire.read() << 8) | Wire.read();  // 0x45 (ACCEL_YOUT_H) & 0x46 (ACCEL_YOUT_L)
  int Az_raw = (Wire.read() << 8) | Wire.read();  // 0x47 (ACCEL_ZOUT_H) & 0x48 (ACCEL_ZOUT_L)

  xyz accel_state;

  // Relative to how we orient it, see orientations.txt
  accel_state.x = Az_raw;
  accel_state.y = Ax_raw;
  accel_state.z = Ay_raw;

  return accel_state;
}

/* Print the state of the accelerometer */
void print_xyz(String what, xyz state) {
  Serial.print(what); Serial.print(" | ");
  Serial.print("x = "); Serial.print(state.x);
  Serial.print(" | y = "); Serial.print(state.y);
  Serial.print(" | z = "); Serial.print(state.z);
  Serial.println("");
}

void print_mouse_state(mouse_state state) {
  Serial.println("");
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

  // Set which pins are input/output
  set_pins_mode(INPUT, in_pins, 3);
  set_pins_mode(OUTPUT, all_led_color_pins, 3);

  // Enable pwr to mouse switches
  pinMode(MOUSE_SWITCH_PWR_PIN, OUTPUT);
  digitalWrite(MOUSE_SWITCH_PWR_PIN, HIGH);

  // Enable pwr to LED
  pinMode(LED_PWR_PIN, OUTPUT);
  digitalWrite(LED_PWR_PIN, HIGH);

  // Setup global variables
  global_mouse_state = initial_mouse_state();

  // Disable temperature sensor 
  set_mpu_register(REG_PWR_MGMT_1, B1000);

  // Set FS_SEL to +- 2000 degree/s (maximum it can detect)
  set_mpu_register(REG_GYRO_CONFIG, B11000); 

  // Set AFS_SEL to +- 16g 
  set_mpu_register(REG_ACCEL_CONFIG, B00000); 

  // Set SMPRT_DIV (sample rate divider) 1000HZ/(9 + 1)
  set_mpu_register(REG_SMPRT_DIV, B1001); 

  // Set DLPF_CFG in CONFIG 
  set_mpu_register(REG_CONFIG, B001); 
  
}

void loop() {

  // Get raw XYZ values from accelerometer
  xyz accel_state = get_accel_xyz();

  //print_xyz("accelero", accel_state);
  //degree_xy degrees_now = degree_from_accel(accel_state);

  global_mouse_state = get_mouse_state(global_mouse_state, accel_state);

  print_mouse_state(global_mouse_state);

  set_cursor_state(global_mouse_state);

  delayMicroseconds(2000);  
}
