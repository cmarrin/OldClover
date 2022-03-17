# Clover
Clover - A tiny C-like language for Arduino

## History

Clover grew out my need to upload code to an Arduino (Nano in this case) without having to connect to it via USB. It was for a Project where I was daisy chaining a series of Neopixel LED lights on top of fence posts. Each ring of pixels is connected to an Arduino Nano and do effects like flicker, strobe and a rainbow effect. I didn't want to have disassemble all 9 light posts, plug into usb and upload new Arduino code just to add or change an effect. So I created Clover, a simple C-like language. You compile the source code, producing bytecodees which are interpreted by the Clover runtime. SoftSerial is used to talk to the Arduinos from a Raspberry Pi running Node-Red. The RPi is connected to the SoftSerial input of the first Nana, and its output is connected to the SoftSerial input of the next, etc. From a web page you can upload new interpreted code and send commands to select the effect, color, rate, etc. All without ever touching the Nanos.

## Overview

Clover is a standalone package with a Compiler and Runtime. There is a Mac project to run the compiler, which takes Clover source an turns it into Arly, the interpreted bytecodes which are executed by the Runtime. The runtime works both on Mac and Arduino. You can test your compiled code on the Mac then transfer it to the Arduino to execute in the live hardware environment.

## Limitations

The Clover virtual machine is very small and designed to run on AVR processors with 32K of ROM, 2K of RAM, and 1K of EEPROM. So many of the limits are based on memory size. Also Arly, the Clover machine language is designed to be very compact. So it adds it's own limits. All memory in Clover is addressed as a 4 byte value, either integer, float or pointer. The machine code itself is made up of single byte entries, so jumps in the code are on byte boundaries as well. When describing limits I will describe what would have to be done to expand these limits.

### Variables and constants

Clover can address constants in EEPROM, as well as global and local variables in RAM. The machine code represents these as a single byte. A value between 0 and 0x7f is a constant value in EEPROM, starting at 0. This allows for 128 values, so constants must be in the first 512 bytes of EEPROM. In reality constant memory takes up as much space as needed and the rest of EEPROM is used for code. At compile time if more than 128 words of constants are created the compile fails.

A value from 0x80 to 0xbf is in global memory, so the maximum global size is 64 words or 256 bytes. The address is (value - 0x80). The global size is determined at compile time and allocated at runtime. A value from 0xc0 to 0xff is a local variable. The address is (value - 0xc0). Locals are allocated on the stack with the bp register as the base. At function entry, enough space is reserved for locals and at function exit it is returned to the stack.

More flexibility would be possible if the ranges of the 3 value type were not fixed. You might have a compilation that needs more globals than constants, so you might want the start of globals to be before 0x80. Since we know the size required by globals at runtime, the base of each value type could be set for each compilations unit. This is a possible future change, It's also possible to allow 256 values of each type if opcodes were added to handle each value types. Alternatively the opcodes taking a value id (PushRef, Push and Pop) could be made into extended opcodes, which have 4 extra data bits in the lower 4 bits of the opcode. This would allow an id of 12 bits, which would be distributed to the 3 value types, for a total access of 4K words or 16K bytes.

### Jumps

The Loop, Jump, If and Else opcodes take a sz byte, which allows for a jump of 256 bytes (backward in the case of Loop and forward for the others). So it's possible for a jump to be too big. This is detected at compile time and the compilation fails when it happens. There are two possibilities to resolve this. First, these opcodes could be made extended, which would allow for a 4K jump distance. Or a jump instruction with a 2 byte sz could be created and used when a long jump is required. It would have a signed sz, so it could take the place of Jump and Loop directly. There would also be a long If and Else with a 2 byte sz value. This would allow for a jump of 32KB.

### Structs and Arrays

The Offset opcode has a 4 bit offset value. It's used with the dot notation, allowing access into structs. Each entry in a struct is a one word value (structs with struct or array entries are not allowed). So each struct is limited to 16 values. The Index opcode also has a 4 bit size value. It's used to access elements of an array. This value is the size of each entry in the array, so it's multiplied by the array index. That means each array entry can be a maxumum of 16 words in size. An array of floats of Ints has an entry size of 1, so the only time this happens is for arrays of structs. Since a struct is already limited to 16 entries, this limit matches that. Array indexes are integers, so theoretically they could be 64K words in length.

A long Index and Offset opcode could be added to extend this to 12 bit entries, allowing 4K word structs. This could allow for structs containing fixed size arrays, as long as the array size doesn't make the struct more than 4K words. This is currently no syntax to do this, so that would have to be added.

### Commands

The Clover runtime uses the Arduino mechanism of init and loop functions. Furthermore, a Clover compilation unit can contain multiple "commands". The command statement defines the command name (a string), number of params passed in and the name of an init and loop function. The interpreter init function is called with the command name and an array of up to 16 param bytes. The interpreter finds the command and then performs initialization to execute. Then it calls the init function specified in the command. A Param native function in the Core module is used to access command bytes as ints from 0 to 255. These params are available to both the init and loop functions. The init function can be void. It's return value is not used. But the loop functions should return an int, which is the number of milliseconds to wait before calling loop again.

The param limit of 16 bytes is arbitrary. It could be increased by simply increasing the buffer size in the interpreter. And an accessor method could be added to the Core module to access params as floats or 32 bit ints.

### Functions

Functions must start with a SetFrame opcode. This has a 4 bit param value and an 8 bit local value. Formal parameters can only be int, float or pointer, so functions are limited to 16 params. Local variables can be structs or arrays as well and there can be a total of 256 words of local variables. 16 formal params is not a big restriction. But 256 words of locals restricts the size of local arrays and the number of possible structs. So this limit could be solved with a long SetFrame form, taking a 2 byte local size, allowing for 64K words of local variables.

The target for a function call is an absolute byte address in RAM. The Call instruction is extended and takes a subsequent byte allowing for an address 4KB in size. A long Call could be created which would allow for a 64KB address range.

Clover also supports native functions. They are kept in modules added to the runtime to add function calls for specific applications. There is a Core set included with Clover which does things like converting float to int, generating random numbers, etc.

## Future Work

- Make base of each value type (constant, global, local) set at runtime to allow more or less of each type depending on needs.
- Rearrange the opcode values to make more space for extended opcodes
- Make opcodes taking an id into extended opcodes, allowing for 12 bit ids for access to 16KB of memory
- 
