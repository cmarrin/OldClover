# Clover
Clover - A tiny C-like language for Arduino

## History

Clover grew out my need to upload code to an Arduino (Nano in this case) without having to connect to it via USB. It was for a Project where I was daisy chaining a series of Neopixel LED lights on top of fence posts. Each ring of pixels is connected to an Arduino Nano and do effects like flicker, strobe and a rainbow effect. I didn't want to have disassemble all 9 light posts, plug into usb and upload new Arduino code just to add or change an effect. So I created Clover, a simple C-like language. You compile the source code, producing bytecodees which are interpreted by the Clover runtime. SoftSerial is used to talk to the Arduinos from a Raspberry Pi running Node-Red. The RPi is connected to the SoftSerial input of the first Nana, and its output is connected to the SoftSerial input of the next, etc. From a web page you can upload new interpreted code and send commands to select the effect, color, rate, etc. All without ever touching the Nanos.

## Overview

Clover is a standalone package with a Compiler and Runtime. There is a Mac project to run the compiler, which takes Clover source an turns it into Arly, the interpreted bytecodes which are executed by the Runtime. The runtime works both on Mac and Arduino. You can test your compiled code on the Mac then transfer it to the Arduino to execute in the live hardware environment.
