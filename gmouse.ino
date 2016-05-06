#include <Mouse.h>
#include <Wire.h>
#include <Keyboard.h>


#include "mpu6050.h"
#include "i2cdevlib_mpu6050/I2Cdev.h"
#include "i2cdevlib_mpu6050/MPU6050_6Axis_MotionApps20.h"

#define INTERRUPT_PIN 8

// The minimum/maximum stored gyro value in mouse_state
#define MAX_GYRO_COUNT 500000L 

// What multiple of MAX_GYRO_COUNT counts as a velocity increase
#define GYRO_VELOCITY_MULTIPLE 100000L 

// At velocity zero, how much extra gyro count we need to add to overcome ut
#define GYRO_STOP_BUMP 1000L

// We ignore gyroscope values that are too too small or too big
#define GYRO_IGNORE_UNDER 250
#define GYRO_IGNORE_OVER (MAX_GYRO_COUNT*5) 

// Bit-shift to lower the gyroscopes raw input
#define GYRO_DAMPEN 5

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
int16_t gx, gy, gz;
MPU6050 i2cdev_gyro;
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)

volatile bool mpuInterrupt = false;     // indicates whether MPU interrupt pin has gone high
void dmpDataReady() {
    mpuInterrupt = true;
}

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
  if (abs(value) > GYRO_IGNORE_UNDER && abs(value) < GYRO_IGNORE_OVER) 
    return (value);
  return 0;
}

boolean between(long value, long low, long high) {
  if (value > low && value < high)
    return true;
  return false;
}

long velocity_now(long gyro_value) {
  if (between(gyro_value, (-1)*GYRO_VELOCITY_MULTIPLE, GYRO_VELOCITY_MULTIPLE)) { // apply speed bump if velocity = 0
    int direction = sign(gyro_value);
    gyro_value -= GYRO_STOP_BUMP*direction;
  }

  return ((gyro_value / GYRO_VELOCITY_MULTIPLE));
}

/* Sets the current state of the mouse. Checks to see if buttons are
 * pressed, what the gyroscope values are, etc*/
mouse_state get_mouse_state(mouse_state mstate, gyro_state gstate) {

  // Log changes in gstate if over noise threshold
  mstate.gyro_x += gyro_change(gstate.x);
  mstate.gyro_y -= gyro_change(gstate.y);

  mstate.gyro_x = constrain(mstate.gyro_x, (-1)*MAX_GYRO_COUNT, MAX_GYRO_COUNT);
  mstate.gyro_y = constrain(mstate.gyro_y, (-1)*MAX_GYRO_COUNT, MAX_GYRO_COUNT);

  // Update the velocity
  mstate.velocity_x = velocity_now(mstate.gyro_x);
  mstate.velocity_y = velocity_now(mstate.gyro_y);

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

  state.x = GyX >> GYRO_DAMPEN;
  state.y = GyY >> GYRO_DAMPEN;
  state.z = GyZ >> GYRO_DAMPEN;

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

void print_gyro_i2cdev_state(int x, int y, int z) {

  /*Serial.print(x); Serial.print("\t");
  Serial.print(y); Serial.print("\t");
  Serial.println(z);*/

  /*mpuInterrupt = false;
  uint16_t mpuIntStatus = i2cdev_gyro.getIntStatus();

  if  (!(mpuIntStatus & 0x02))
    return;
    */

  uint16_t fifoCount = i2cdev_gyro.getFIFOCount();

  while (fifoCount < packetSize) fifoCount = i2cdev_gyro.getFIFOCount();

  uint8_t fifoBuffer[64]; // FIFO storage buffer
  i2cdev_gyro.getFIFOBytes(fifoBuffer, packetSize);

  // display Euler angles in degrees
  Quaternion q;           // [w, x, y, z]         quaternion container
  float euler[3];         // [psi, theta, phi]    Euler angle container
  i2cdev_gyro.dmpGetQuaternion(&q, fifoBuffer);
  i2cdev_gyro.dmpGetEuler(euler, &q);

  Serial.print("euler\t");
  Serial.print(euler[0] * 180/M_PI);
  Serial.print("\t");
  Serial.print(euler[1] * 180/M_PI);
  Serial.print("\t");
  Serial.println(euler[2] * 180/M_PI);

}

/* ################## Main Program ################## */

void setup() {
  
  Wire.begin();
  Serial.begin(9600);
  while (!Serial);
  Mouse.begin();
  Keyboard.begin();

  //i2cdev gyro init
  i2cdev_gyro.initialize();
  Serial.println(i2cdev_gyro.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");
  i2cdev_gyro.setDMPEnabled(true);

  uint8_t devstatus = i2cdev_gyro.dmpInitialize();

  i2cdev_gyro.setXGyroOffset(220);
  i2cdev_gyro.setYGyroOffset(76);
  i2cdev_gyro.setZGyroOffset(-85);
  i2cdev_gyro.setZAccelOffset(1788); // 1688 factory default for my test chip

  //pinMode(INTERRUPT_PIN, INPUT);
  //attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), dmpDataReady, RISING);

  if (devstatus != 0)
    Serial.println("Could not init DMP!");

  Serial.print("status: ");
  Serial.println(i2cdev_gyro.getIntStatus());
  packetSize = i2cdev_gyro.dmpGetFIFOPacketSize();

  // TODO: properly store these pin values, don't just hard code
  int in_pins[] = {4, 5, 6};
  set_pins_mode(INPUT, in_pins, 3);

  pinMode(MOUSE_SWITCH_PWR_PIN, OUTPUT);
  digitalWrite(MOUSE_SWITCH_PWR_PIN, HIGH);

  global_mouse_state = initial_mouse_state();

  // Disable temperature sensor 
  /*set_mpu_register(REG_PWR_MGMT_1, B1000);

  // Set FS_SEL to +- 2000 (this seems to reduce sensitivity)
  //set_mpu_register(REG_GYRO_CONFIG, B11000); 
  set_mpu_register(REG_GYRO_CONFIG, B00000); 

  // Set SMPRT_DIV (sample rate dividor) to 0 to achive a faster sample rate
  set_mpu_register(REG_SMPRT_DIV, B0); */

  // Enable DATA_RDY_EN, so that the MPU can notify use when there's new data
  // set_mpu_register(REG_INT_ENABLE, B1);
}

void loop() {
  gyro_state gstate = get_gyro_xyz();

  global_mouse_state = get_mouse_state(global_mouse_state, gstate);

  // wait for MPU interrupt or extra packet(s) available
  //while (!mpuInterrupt) { }

  print_gyro_i2cdev_state(0, 0, 0);

  return;

  //print_gyro_state(gstate);

  print_mouse_state(global_mouse_state);

  set_cursor_state(global_mouse_state);

  //delay(1);  
}
