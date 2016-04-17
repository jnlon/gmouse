#include <Mouse.h>
#include <Wire.h>

#include "mpu6050_registers.h"

#define MPU_6050_ADDRESS 104

//ints are two bytes
struct gyro_state_s {
  int x; 
  int y;
  int z;
};

typedef gyro_state_s gyro_state;

void set_register(byte reg, byte value) {
  Wire.beginTransmission(MPU_6050_ADDRESS); 
  Wire.write(reg);
  Wire.write(value); 
  Wire.endTransmission(true);
}

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

void setup() {
  
  Wire.begin();

  set_register(REG_PWR_MGMT_1, B1000); // Disable temperature sensor
  
  Serial.begin(9600);
}

void loop() {
  int MPU_addr = 104;

  gyro_state_s state = get_gyro_xyz();
  
  Serial.print("GyX = "); Serial.print(state.x);
  Serial.print(" | GyY = "); Serial.print(state.y);
  Serial.print(" | GyZ = "); Serial.println(state.z);
  delay(333);  
}
