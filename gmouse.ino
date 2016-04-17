#include <Mouse.h>
#include <Wire.h>

#include <gyroscope_registers.h>

/*
 * A note on setup and how communicating with the gyroscope works:
 * 
 * We can configure how the gyroscope functions by setting a value to its various "registers",
 * which are basically memory cells that reside on the chip itself. We can read/write
 * to these registers by sending the right signals to the chip over I2C (with Arduino's Wire library).
 * It's important to note that mpu6050 AUTO INCREMENTS the register of the values
 * we write, so we can theoretically setup or read from the entire chip at once with a single transmission.
 * 
 * Here is an annotated example of setting up the chip.
 * 
 * Wire.beginTransmission(SLAVE_ADDRESS); 
 * // beginTransmission sends the I2C "START" signal. 
 * // The SLAVE_ADDRESS is a value that uniquely identifies a component. 
 * // The documentation says it is always 110100X (104 or 105) for mpu6050
 *
 * Wire.write(REGISTER_START_ADDRESS); 
 * // Select the register at REGISTER_START_ADDRESS. On the mpu6050, registers 13 through 117 are available
 * // Each of these registers has a unique purpose. See the register documentation for a description what it does
 * 
 * Wire.write(VALUE1); 
 * // Set the value for REGISTER_START_ADDRESS
 * // What each value does depends on the register, so see the documentation
 * 
 * Wire.write(VALUE2); 
 * // Since the mpu6050 auto-increments the selected register, subsequent writes  
 * // set the the value for the next register. So here, VALUE2 becomes the value of REGISTER_START_ADDRESS+1
 * 
 * ...
 * Wire.endTransmission(true); 
 * // Tell the gyroscope  we are done reading/writing values 
 * // by sending the I2C "STOP" message (this is why we need (true) as the paramater)
 * 
 */


#define MPU_6050_ADDRESS 104

void set_register(byte reg, byte value) {
  Wire.beginTransmission(MPU_6050_ADDRESS); 
  Wire.write(reg);
  Wire.write(value); 
  Wire.endTransmission(true);
}

void setup() {
  
  Wire.begin();

  set_register(REG_PWR_MGMT_1, B1000); // Disable temperature sensor
  
  Serial.begin(9600);
}

void loop() {
  int MPU_addr = 104;
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_addr,14,true);  // request a total of 14 registers
  int AcX=Wire.read()<<8|Wire.read();  // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)    
  int AcY=Wire.read()<<8|Wire.read();  // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  int AcZ=Wire.read()<<8|Wire.read();  // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
  int Tmp=Wire.read()<<8|Wire.read();  // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
  int GyX=Wire.read()<<8|Wire.read();  // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  int GyY=Wire.read()<<8|Wire.read();  // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  int GyZ=Wire.read()<<8|Wire.read();  // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)
  Serial.print("AcX = "); Serial.print(AcX);
  Serial.print(" | AcY = "); Serial.print(AcY);
  Serial.print(" | AcZ = "); Serial.print(AcZ);
  Serial.print(" | Tmp = "); Serial.print(Tmp/340.00+36.53);  //equation for temperature in degrees C from datasheet
  Serial.print(" | GyX = "); Serial.print(GyX);
  Serial.print(" | GyY = "); Serial.print(GyY);
  Serial.print(" | GyZ = "); Serial.println(GyZ);
  delay(333);  
}
