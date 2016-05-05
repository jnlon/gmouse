#!/usr/bin/env python3

import serial, sys

with serial.Serial(port=sys.argv[1], baudrate=sys.argv[2]) as s:
    while s.isOpen():
        print(ser.readline().decode("utf-8").strip())
