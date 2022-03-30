/*-------------------------------------------------------------------------
    This source file is a part of Clover
    For the latest info, see https://github.com/cmarrin/Clover
    Copyright (c) 2021-2022, Chris Marrin
    All rights reserved.
    Use of this source code is governed by the MIT license that can be
    found in the LICENSE file.
-------------------------------------------------------------------------*/

#pragma once

#include <stdint.h>

namespace clvr {

/*

Machine Code for compiled Clover code

The machine code and assembly language for the Clover system is called Arly.

See README.md for a description of Clover and the Arly machine language.

Engine

There are 4 color registers which contain 3 float (h, s, v) and 4 registers of
4 bytes which can hold an integer or a float.

Opcodes:
    Param nomenclature:
        
        id          - Byte after opcode. Address of a const, global or local
        i           - Lower 4 bits of opcode. Offset in struct
        x           - Lower 4 bits of opcode. Size of each member of an array
        const       - Byte after opcode. Int constant (-128 to 127)
        constS      - Lower 4 bits of opcode. Int constant (0 to 15)
        absTarg     - Lower 4 bits of opcode (bits 11:8) | byte after opcode
                      (bits 7:0). 12 bit absolute address (0 to 4095).
        relTarg     - Lower 4 bits of opcode (bits 11:8) | byte after opcode
                      (bits 7:0). 12 bit relative address (-2048 to 2047).
        nativeId    - Byte after opcode. Id of function in NativeModule.
        p           - Lower 4 bits of opcode. Num params passed to function.
        l           - Byte after opcode. Num locals in function.
        n           - Lower 4 bits of opcode. Num params passed to Log.
        len         - Byte after opcode. Num chars in string.
        <str>       - Starts at byte after len. Format string for Log.

    // The opcodes deal with variable references. LoadRef simply places
    // the address of the passed id in the passed register. It can later
    // be used by LoadDeref and StoreDeref. LoadRefX is passed a source
    // register and numeric value. The value is the number of words per
    // entry in the array. The register value is multiplied by the passed
    // value and the result is added to the address of the passed id which
    // is stored in the destination register.
    //
    // LoadDeref is passed the register with the address previous set above
    // along with a numeric value to access the given element of the entry.
    // The source register address is added to the passed value, then the
    // memory at that address is loaded into the destination register.
    // StoreDeref does the same but the value in the source register is
    // stored at the address.
    
    PushRef id              - stack[sp++] = id
    PushDeref               - t = stack[--sp], stack[sp++] = mem[t]
    PopDeref                - v = stack[--sp], mem[stack[--sp]] = v
    
    Offset i                - stack[sp-1] += i
    Index x                 - i = stack[--sp], stack[sp-1] += i * n
    
    Push id                 - stack[sp++] = const or global or local variable
    Pop id                  - global or local variable = stack[--sp]
    
    PushIntConst const      - stack[sp++] = const
    PushIntConstS constS    - stack[sp++] = constS

    Dup                     - stack[sp++] = stack[sp]
    Drop                    - --sp
    Swap                    - t = stack[sp]; stack[sp] = stack[sp-1]; stack[sp-1[ = t;

    If relTarg              - If stack[--sp] is non-zero execute statements in first clause. 
                              If zero skip the statements. Number of bytes to skip is 
                              in sz.
    
    Jump relTarg            - Jump size bytes (target is -2048 to 2047)
    
    Call absTarg            - Call function [target], params on stack
    CallNative nativeId     - Call native function
    Return                  - Return from function, return value on TOS
    SetFrame p l            - Set the local frame with number of formal
                              params (p) and locals (l). This must be the 
                              first instruction of every function.
                              
    Log n len <str>         - n is the number of args passed on the stack.
                              Opcode is followed by len character format
                              string. Output formatted string to console.

The following opcodes expect 1 value on stack (a = tos). Value
is popped, the operation is performed and the result is pushed.

    Not                     - stack[sp++] = ~a (assumes int32_t)
    LNot                    - stack[sp++] = !a (assumes int32_t)
    NegInt                  - stack[sp++] = -a (assumes int32_t, result is int32_t)
    NegFloat                - stack[sp++] = -a (assumes float, result is float)

The following opcodes expect 2 values on stack (a = tos-1, b = tos). Values
are popped, the operation is performed and the result is pushed.

    Or                      - stack[sp++] = a | b (assumes int32_t)
    XOr                     - stack[sp++] = a ^ b (assumes int32_t)
    And                     - stack[sp++] = a & b (assumes int32_t)
    
    LOr                     - stack[sp++] = a || b (assumes int32_t)
    LAnd                    - stack[sp++] = a && b (assumes int32_t)
    LTInt                   - stack[sp++] = a < b (assumes int32_t, result is int32_t)
    LTFloat                 - stack[sp++] = a < b (assumes float, result is int32_t)
    LEInt                   - stack[sp++] = a <= b (assumes int32_t, result is int32_t)
    LEFloat                 - stack[sp++] = a <= b (assumes float, result is int32_t)
    EQInt                   - stack[sp++] = a == b (assumes int32_t, result is int32_t)
    EQFloat                 - stack[sp++] = a == b (assumes float, result is int32_t)
    NEInt                   - stack[sp++] = a != b (assumes int32_t, result is int32_t)
    NEFloat                 - stack[sp++] = a != b (assumes float, result is int32_t)
    GEInt                   - stack[sp++] = a >= b (assumes int32_t, result is int32_t)
    GEFloat                 - stack[sp++] = a >= b (assumes float, result is int32_t)
    GTInt                   - stack[sp++] = a > b (assumes int32_t, result is int32_t)
    GTFloat                 - stack[sp++] = a > b (assumes float, result is int32_t)
    
    AddInt                  - stack[sp++] = a + b (assumes int32_t, result is int32_t)
    AddFloat                - stack[sp++] = a + b (assumes float, result is float)
    SubInt                  - stack[sp++] = a - b (assumes int32_t, result is int32_t)
    SubFloat                - stack[sp++] = a - b (assumes float, result is float)
    MulInt                  - stack[sp++] = a * b (assumes int32_t, result is int32_t)
    MulFloat                - stack[sp++] = a * b (assumes float, result is float)
    DivInt                  - stack[sp++] = a / b (assumes int32_t, result is int32_t)
    DivFloat                - stack[sp++] = a / b (assumes float, result is float)

Increment and decrement ops expect a ref on TOS which is popped. The "pre" 
versions will load the value, increment or decrement, store the new value 
and push that value. The "post" versions will load the value, push that value, 
then increment or decrement and store the result. There are Int and Float 
versions for all. The end result is one value left on the stack, which is 
either incremented or decremented or not and one value at the location of the 
ref which is either incremented or decremented.
     
    PreIncInt
    PreIncFloat
    PreDecInt
    PreDecFloat
    PostIncInt
    PostIncFloat
    PostDecInt
    PostDecFloat
    
    
    Executable format
    
    Format Id           - 4 bytes: 'arly'
    Constants size      - 2 bytes: size in 4 byte units of Constants area
    Global size         - 2 bytes: size in 4 byte units needed for global vars
    Stack size          - 2 bytes: size in 4 byte units needed for the stack
    Constants area      - n 4 byte entries: ends after size 4 byte units
    Command entries     - Each entry has:
                            7 byte command (Unused trailing bytes are set to '\0')
                            1 byte number of param bytes
                            2 bytes start of init instructions, in bytes
                            2 bytes start of loop instructions, in bytes
                            
                          entries end when a byte of 0 is seen
                          
    Commands            - List of init and loop instructions for each command
*/

static constexpr uint16_t MaxIdSize = 4096;
static constexpr uint16_t ConstStart = 0x00;
static constexpr uint16_t ConstSize = 2048; // Max possible size
static constexpr uint16_t GlobalStart = ConstStart + ConstSize;
static constexpr uint16_t GlobalSize = 1024;
static constexpr uint16_t LocalStart = GlobalStart + GlobalSize;
static constexpr uint16_t LocalSize = MaxIdSize - LocalStart;
static constexpr uint8_t ExtOpcodeStart = 0x40;

enum class Op: uint8_t {

    None            = 0x00,

    PushIntConst    = 0x01,
    PushDeref       = 0x02,
    PopDeref        = 0x03,
    
    Dup             = 0x04,
    Drop            = 0x05,
    Swap            = 0x06,
    
    CallNative      = 0x0a,
    Return          = 0x0b,
    
// 0x0c to 0x0f unused

    Or              = 0x10,
    Xor             = 0x11,
    And             = 0x12,
    Not             = 0x13,

    LOr             = 0x14,
    LAnd            = 0x15,
    LNot            = 0x16,

    LTInt           = 0x17,
    LTFloat         = 0x18,
    LEInt           = 0x19,
    LEFloat         = 0x1a,
    EQInt           = 0x1b,
    EQFloat         = 0x1c,
    NEInt           = 0x1d,
    NEFloat         = 0x1e,
    GEInt           = 0x1f,
    GEFloat         = 0x20,
    GTInt           = 0x21,
    GTFloat         = 0x22,

    AddInt          = 0x23,
    AddFloat        = 0x24,
    SubInt          = 0x25,
    SubFloat        = 0x26,
    MulInt          = 0x27,
    MulFloat        = 0x28,
    DivInt          = 0x29,
    DivFloat        = 0x2a,

    NegInt          = 0x2b,
    NegFloat        = 0x2c,
    
    PreIncInt       = 0x2d,
    PreIncFloat     = 0x2e,
    PreDecInt       = 0x2f,
    PreDecFloat     = 0x30,
    PostIncInt      = 0x31,
    PostIncFloat    = 0x32,
    PostDecInt      = 0x33,
    PostDecFloat    = 0x34,
    
    // 0x40 - 0xf0 ops use lower 4 bits for data value

    PushRef         = ExtOpcodeStart + 0x00,
    Push            = ExtOpcodeStart + 0x10,
    Pop             = ExtOpcodeStart + 0x20,

    Call            = ExtOpcodeStart + 0x30,
    
    Offset          = ExtOpcodeStart + 0x40,
    Index           = ExtOpcodeStart + 0x50,
    PushIntConstS   = ExtOpcodeStart + 0x60,
    Log             = ExtOpcodeStart + 0x70,
    SetFrame        = ExtOpcodeStart + 0x80,
    Jump            = ExtOpcodeStart + 0x90,
    If              = ExtOpcodeStart + 0xa0,
};

enum class OpParams : uint8_t {
    None,       // No params
    Id,         // b+1 = <id>
    I,          // b+1[3:0] = <int> (0-3)
    Index,      // b[3:0] = <int> (0-15)
    Const,      // b+1 = 0-255
    AbsTarg,    // Lower 4 bits of opcode (bits 11:8) | byte after opcode
                // (bits 7:0). 12 bit absolute address (0 to 4095).
    RelTarg,    // Lower 4 bits of opcode (bits 11:8) | byte after opcode
                // (bits 7:0). 12 bit relative address (-2048 to 2047).
    P_L,        // b[3:0] = num params (0-15), b+1 = num locals (0-255)
    Idx_Len_S, // b[3:0] = <int> (0-15), b+1 = <int>, followed by Sz string bytes
};

}
