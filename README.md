# gmouse
This repo contains the Arduino code for a highschool computer engineering
project. The goal is to create a USB mouse controlled via angular displacement 
with an [Arduino Leonardo](https://www.arduino.cc/en/Main/arduinoBoardLeonardo)
and an [MPU 6050
gyroscope/accelerometer](http://www.invensense.com/products/motion-tracking/6-axis/mpu-6050/)

# Project Notes

## Docs

[MPU-6050 Datasheet](https://cdn.sparkfun.com/datasheets/Components/General%20IC/PS-MPU-6000A.pdf)

[MPU-6050 Registers](http://43zrtwysvxb2gf29r5o0athu.wpengine.netdna-cdn.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf)

[Arduino Leonardo Overview](https://www.arduino.cc/en/Main/ArduinoBoardLeonardo)

[Arduino Mouse/Keyboard Reference](https://www.arduino.cc/en/Reference/MouseKeyboard)

[Arduino Wire Library (for i2c)](https://www.arduino.cc/en/reference/wire)

## Project Overview

The goal is to create a USB mouse whose pointer has a velocity that is
controlled via the tilt of the device. So, when the mouse is held flat in the
air, the cursor should be stationary. If it is tilted to the left, the cursor
should start to move the left of the screen proportional to the tilt.

We aim to include 2 left/right mouse buttons, a "scroll button" that, when
depressed, causes the orientation of mouse to control scrolling the mouse wheel
up/down (basically a tilt scroll wheel), and potentially another button for
calibration, setting what physical orientation of the mouse has a zero x/y
pointer velocity.

The mouse shell will be 3D-printed, and the device will be attached to the
computer via a USB cable.

## Considerations

### Gyroscope

**Update**: The accelerometer works much better for our use than the gyroscope, see the section below

The MPU-6050 seems to return values corresponding to the orientation of the
device relative to it's previous sampling state (can this be configured?). When
motionless, it returns values for x/y/z ranging between about negative 500 to
positive 500. When rotated sharply, it returns much larger values (~10-100
times greater). We need to find the range of values we can safely ignore due
to, for example, natural disturbances or slight wrist/arm shaking.

The main loop will involve fetching and processing the gyroscope's values,
updating the x/y internal velocity variables, and finally updating the mouse's
position with Mouse.move().

### Accelerometer

The MPU-6050 has a built in accelerometer that is accessible from register
0x3B. It returns values that consistantly correspond to the orientation of the
device relative to the ground (due to the force of gravity and its components).
This discovery saves us from having to apply something a Kalman filter to our
gyroscope values or integrate them over time.

### Ergonomics

 * The mouse should be comfortable to hold in the air
  * We should consider making a brace / wrist mount to reduce tension from holding the mouse
 * The buttons should be comfortable to reach from rest position (left/right and top buttons)

### Button Input

The mouse has 3 physical buttons:
  * Front left and right button
  * Left side button
 
The following is the functions:
  * Front left + right = scroll mode
  * Side button = back button
  * All three buttons = calibration

### Misc

  * As a last minute addition, we've added an RGB LED that changes colour
    depending on the current velocity of the cursor
    *Red = stop
    *Blue = cursor moving up/down
    *Green = cursor moving left/right

## Technical Notes

### I2C with Gyroscope/Accelerometer

We can configure how the gyroscope functions by setting the values of its
various "registers". Registers are 8-bit memory cells that reside on the chip itself.
We can read from/write to these registers by sending the right signals 
over I2C (with Arduino's Wire library). It's important to note that the
mpu6050 **auto increments** the register of the values we read/write, so we can
theoretically setup or read from the entire chip at once with a single
transmission. See page 38 of the datasheet for a better description of how this
works.

Here is an annotated example of setting up the chip.

```
Wire.beginTransmission(SLAVE_ADDRESS); 
// beginTransmission sends the I2C "START" signal. 
// The SLAVE_ADDRESS is a value that uniquely identifies a component. 
// The documentation for the mpu6050 says it is 110100X (104 or 105)

Wire.write(REGISTER_START_ADDRESS); 
// Select the register at REGISTER_START_ADDRESS. 
// On the mpu6050, registers 13 through 117 are available
// Each of these registers has a unique purpose. See the register documentation

Wire.write(VALUE1); 
// Set the value for REGISTER_START_ADDRESS
// What each value does depends on the register, see the documentation

Wire.write(VALUE2); 
// Since the mpu6050 auto-increments the selected register, subsequent writes  
// set the the value for the next register. So here, VALUE2 becomes the value of REGISTER_START_ADDRESS+1

...
Wire.endTransmission(true); 
// Tell the gyroscope we are done reading/writing values 
// by sending the I2C "STOP" message (this is why we need (true) as the paramater)
```

### Code Guidelines

* Register values should be placed in `mpu6050.h` and should be prefixed with REG_

