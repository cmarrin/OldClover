//
//  Interpreter.h
//  CompileArly
//
//  Created by Chris Marrin on 1/9/22.
//
// Arly Interpreter base class
//
// Interpret Arly executable
//

#pragma once

#include "Opcodes.h"

#include <stdlib.h>
#include <string.h>

#ifdef ARDUINO
    #include <Arduino.h>
#else
    static inline void randomSeed(uint32_t s) { srand(s); }
    static inline int32_t random(int32_t min, int32_t max)
    {
        if (min >= max) {
            return max;
        }
        int r = rand() % (max - min);
        return r + min;
    }
    
    template<class T> 
    const T& min(const T& a, const T& b)
    {
        return (b < a) ? b : a;
    }

    template<class T> 
    const T& max(const T& a, const T& b)
    {
        return (b > a) ? b : a;
    }
#endif

namespace arly {

static constexpr uint8_t MaxStackSize = 128;    // Could be 255 but let's avoid excessive 
                                                // memory usage
static constexpr uint8_t StackOverhead = 64;    // Amount added to var mem high water mark
                                                // for stack growth.
static constexpr uint8_t MaxTempSize = 32;      // Allocator uses a uint32_t map. That would 
                                                // need to be changed to increase this.
static constexpr uint8_t ParamsSize = 16;       // Constrained by the 4 bit field with the index

static inline float intToFloat(uint32_t i)
{
    float f;
    memcpy(&f, &i, sizeof(float));
    return f;
}

static inline uint32_t floatToInt(float f)
{
    uint32_t i;
    memcpy(&i, &f, sizeof(float));
    return i;
}


//
// Core Native Functions
//

class Interpreter;

class CompileEngine;

class NativeModule
{
public:
    virtual ~NativeModule() { }
    
    virtual bool hasId(uint8_t id) const = 0;
    virtual uint8_t numParams(uint8_t id) const = 0;
    virtual int32_t call(Interpreter*, uint8_t id) = 0;
    
#ifndef ARDUINO
    virtual void addFunctions(CompileEngine*) = 0;
#endif
};

class Interpreter
{
public:
    enum class Error {
        None,
        CmdNotFound,
        NestedForEachNotAllowed,
        UnexpectedOpInIf,
		InvalidOp,
        OnlyMemAddressesAllowed,
        AddressOutOfRange,
        InvalidColorComp,
        ExpectedSetFrame,
        InvalidNativeFunction,
        NotEnoughArgs,
        WrongNumberOfArgs,
        StackOverrun,
        StackUnderrun,
        StackOutOfRange,
    };

    Interpreter(NativeModule** mod, uint32_t modSize);
    ~Interpreter();
    
    bool init(uint8_t cmd, const uint8_t* buf, uint8_t size);
    int32_t loop();

    Error error() const { return _error; }
    
    // Returns -1 if error was not at any pc addr
    int16_t errorAddr() const { return _errorAddr; }
    
	// Return a float with a random number between min and max.
	// The random function takes ints. Multiply inputs by 1000 then divide the result
    // by the same to get a floating point result. That effectively makes the range
    // +/-2,000,000.
	static float random(float min, float max)
	{
		return float(::random(int32_t(min * 1000), int32_t(max * 1000))) / 1000;
	}
	
	static int32_t random(int32_t min, int32_t max)
	{
		int32_t r = ::random(min, max);
		return r;
	}
 
     uint32_t stackLocal(uint16_t addr) { return _stack.local(addr); }
     void stackPush(uint32_t v) { _stack.push(v); }

    int32_t animate(uint32_t index);
    uint8_t param(uint32_t i) const { return (i >= ParamsSize) ? 0 : _params[i]; }
    int16_t pc() const { return _pc; }
    void setError(Error error) { _error = error; }
    void initArray(uint32_t addr, uint32_t value, uint32_t count);

    virtual uint8_t rom(uint16_t i) const = 0;
    virtual void logInt(uint16_t addr, int8_t i, int32_t v) const = 0;
    virtual void logFloat(uint16_t addr, int8_t i, float v) const = 0;
    virtual void logHex(uint16_t addr, int8_t i, uint32_t v) const = 0;

private:
    class Stack
    {
    public:
        void alloc(uint16_t size)
        {
            if (_stack) {
                delete [ ] _stack;
            }
            _size = size;
            _stack = new uint32_t [size];
            
            _sp = 0;
            _bp = 0;
            _error = Error::None;
        }
            
        void push(uint32_t v) { ensurePush(); _stack[_sp++] = v; }
        uint32_t pop() { ensureCount(1); return _stack[--_sp]; }
        void swap()
        {
            ensureCount(2); 
            uint32_t t = _stack[_sp - 1];
            _stack[_sp - 1] = _stack[_sp - 2];
            _stack[_sp - 2] = t;
        }
        
        const uint32_t& top(uint8_t rel = 0) const { ensureRel(rel); return _stack[_sp - rel - 1]; }
        uint32_t& top(uint8_t rel = 0) { ensureRel(rel); return _stack[_sp - rel - 1]; }
        const uint32_t& local(uint16_t addr) const { ensureLocal(addr); return get(addr + _bp); }
        uint32_t& local(uint16_t addr) { ensureLocal(addr); return get(addr + _bp); }

        bool empty() const { return _sp == 0; }
        Error error() const { return _error; }
                
        bool setFrame(uint8_t params, uint8_t locals)
        {
            int16_t savedPC = pop();
            _sp += locals;
            push(savedPC);
            push(_bp);
            int16_t newBP = _sp - params - locals - 2;
            if (newBP < 0) {
                _error = Error::NotEnoughArgs;
                return false;
            }
            _bp = newBP;
            return true;
        }

        int16_t restoreFrame(uint32_t returnValue)
        {
            int16_t savedBP = pop();
            int16_t pc = pop();
            _sp = _bp;
            _bp = savedBP;
            push(returnValue);
            return pc;
        }

    private:
        void ensureCount(uint8_t n) const
        {
            if (_sp < n) {
                // Set the error, but let the exec catch it
                _error = Error::StackUnderrun;
            }
        }
        
        void ensureRel(uint8_t rel) const
        {
            int16_t addr = _sp - rel - 1;
            if (addr < 0 || addr >= int16_t(_size)) {
                // Set the error, but let the exec catch it
                _error = Error::StackOutOfRange;
            }
        }
        
        void ensureLocal(uint8_t i) const
        {
            int16_t addr = _bp + i;
            if (addr < 0 || addr >= _sp) {
                // Set the error, but let the exec catch it
                _error = Error::StackOutOfRange;
            }
        }
        
        void ensurePush() const
        {
            if (_sp >= _size) {
                // Set the error, but let the exec catch it
                _error = Error::StackOverrun;
            }
        }
        
        uint32_t& get(uint16_t addr) const
        {
            if (int16_t(addr) >= _size) {
                // Set the error, but let the exec catch it
                _error = Error::StackOverrun;
            }
            return _stack[addr];
        }
        
        uint32_t* _stack = nullptr;
        int16_t _size = 0;
        int16_t _sp = 0;
        int16_t _bp = 0;
        mutable Error _error = Error::None;
    };

    int32_t execute(uint16_t addr);
    
    // Index is in bytes
    uint8_t getUInt8ROM(uint16_t index)
    {
        return rom(index);
    }
    
    uint16_t getUInt16ROM(uint16_t index)
    {
        // Little endian
        return uint32_t(getUInt8ROM(index)) | 
               (uint32_t(getUInt8ROM(index + 1)) << 8);
    }

    uint8_t getId() { return getUInt8ROM(_pc++); }
    int8_t getConst() { return getUInt8ROM(_pc++); }
    uint8_t getSz() { return getUInt8ROM(_pc++); }
    void getPL(uint8_t& p, uint8_t& l)
    {
        uint8_t pl = getUInt8ROM(_pc++);
        p = pl >> 4;
        l = pl & 0x0f;
    }

    uint8_t getI()
    {
        uint8_t b = getUInt8ROM(_pc++);
        return b & 0x0f;
    }

    float loadFloat(uint8_t id, uint8_t index = 0)
    {
        uint32_t i = loadInt(id, index);
        float f;
        memcpy(&f, &i, sizeof(float));
        return f;
    }
    
    void storeFloat(uint8_t id, float v) { storeFloat(id, 0, v); }
    
    void storeFloat(uint8_t id, uint8_t index, float v) { storeInt(id, index, floatToInt(v)); }
    
    uint32_t loadInt(uint8_t id, uint8_t index = 0)
    {
        if (id < GlobalStart) {
            // ROM address
            uint16_t addr = ((id + index) * 4) + _constOffset;

            // Little endian
            uint32_t u = uint32_t(getUInt8ROM(addr)) | 
                        (uint32_t(getUInt8ROM(addr + 1)) << 8) | 
                        (uint32_t(getUInt8ROM(addr + 2)) << 16) | 
                        (uint32_t(getUInt8ROM(addr + 3)) << 24);

            return u;
        }
        
        if (id < LocalStart) {
            // Global address
            return _global[id - GlobalStart + index];
        }

        // Local address. Relative to current bp.
        return _stack.local(id - LocalStart + index);        
    }
    
    void storeInt(uint8_t id, uint32_t v) { storeInt(id, 0, v); }
    
    void storeInt(uint8_t id, uint8_t index, uint32_t v)
    {
        // Only Global or Local
        if (id < GlobalStart) {
            return;
        }

        if (id < LocalStart) {
            // Global address
            uint32_t addr = uint32_t(id) - GlobalStart + uint32_t(index);
            _global[addr] = v;
            return;
        }
            
        // Local address. Relative to current bp.
        uint32_t addr = uint32_t(id) - LocalStart + uint32_t(index);
        _stack.local(addr) = v;
    }
    
    Error _error = Error::None;
    int16_t _errorAddr = -1;
    
    uint8_t _params[ParamsSize];
    uint8_t _paramsSize = 0;

    uint32_t* _global = nullptr;
    uint16_t _globalSize = 0;
    
    int16_t _pc = 0;
    Stack _stack;
    
    NativeModule** _nativeModules = nullptr;
    uint8_t _nativeModulesSize = 0;
    
    uint16_t _constOffset = 0; // In bytes
    uint8_t _numParams = 0;
    uint16_t _initStart = 0;
    uint16_t _loopStart = 0;
    uint16_t _codeOffset = 0; // Used by Call to go to the correct absolute address
};

}
