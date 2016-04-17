# gmouse
This repo contains the Arduino code for a highschool computer engineering
project. The goal is to create a USB mouse controlled via gyroscopic rotation
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

The MPU-6050 seems to return values corresponding to the orientation of the
device relative to it's previous sampling state (can this be configured?). When
motionless, it returns values for x/y/z ranging between about negative 500 to
positive 500. When rotated sharply, it returns much larger values (~10-100
times greater). We need to find the range of values we can safely ignore due
to, for example, natural disturbances or slight wrist/arm shaking.

The main loop will involve fetching and processing the gyroscope's values,
updating the x/y internal velocity variables, and finally updating the mouse's
position with Mouse.move().

### Ergonomics

 * The mouse should be comfortable to hold in the air
  * We should consider making a brace / wrist mount to reduce tension from holding the mouse
 * The buttons should be comfortable to reach from rest position (left/right and top buttons)

### Button Input

The mouse shell will need slots to support our switches. I have 3 of the
thick/clicky push switches, and a bunch of slide-switches. We can probably
harvest any extra parts we need from scrap in the compeng room.


### Misc

  * I guess we need an obnoxious logo?
  * LED at bottom of the shell 

##### Anything else?

