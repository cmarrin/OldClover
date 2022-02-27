//
//  Compiler.cpp
//  CompileArly
//
//  Created by Chris Marrin on 1/9/22.
//

#include "Interpreter.h"

#include "NativeCore.h"

using namespace arly;

Interpreter::Interpreter(NativeModule** mod, uint32_t modSize)
{
    _nativeModules = new NativeModule*[modSize + 1];
    _nativeModulesSize = modSize + 1;
    
    _nativeModules[0] = new NativeCore();
    for (int i = 0; i < modSize; ++i) {
        _nativeModules[i + 1] = mod[i];
    }
}

Interpreter::~Interpreter()
{
    // We own the NativeCore, which is the first module
    if (!_nativeModules) {
        delete _nativeModules[0];
    }
    delete [ ] _nativeModules;
}

void
Interpreter::initArray(uint32_t addr, uint32_t value, uint32_t count)
{
    // Only global or local
    if (addr < GlobalStart) {
        _error = Error::OnlyMemAddressesAllowed;
        return;
    }

    if (addr < LocalStart) {
        memset(_global + (addr - GlobalStart), value, count * sizeof(uint32_t));
    } else {
        memset(&_stack.local(addr - LocalStart), value, count * sizeof(uint32_t));
    }
}

bool
Interpreter::init(uint8_t cmd, const uint8_t* buf, uint8_t size)
{
    memcpy(_params, buf, size);
    _paramsSize = size;
	_error = Error::None;
 
    if (_global) {
        delete [ ]_global;
        _global = nullptr;
        _globalSize = 0;
    }
    
    _constOffset = 8;
    
    uint32_t constSize = uint32_t(getUInt8ROM(4)) * 4;
    bool found = false;
    _codeOffset = _constOffset + constSize;
    
    // Alloc globals
    _globalSize = getUInt8ROM(5);
    
    if (_globalSize) {
        _global = new uint32_t[_globalSize];
    }
    
    // Alloc stack
    _stack.alloc(getUInt8ROM(6));

    // Find command
    while (1) {
        uint8_t c = rom(_codeOffset);
        if (c == 0) {
            _codeOffset++;
            break;
        }
        if (c == cmd) {
            // found cmd
            _numParams = getUInt8ROM(_codeOffset + 1);
            _initStart = getUInt16ROM(_codeOffset + 2);
            _loopStart = getUInt16ROM(_codeOffset + 4);
            
            found = true;
            
            //Need to keep looping through all effects
            // to find where the code starts
        }
        
        _codeOffset += 6;
    }
    
    if (!found) {
        _error = Error::CmdNotFound;
        _errorAddr = -1;
        return false;
    }

    _initStart += _codeOffset;
    _loopStart += _codeOffset;
    
    if (_numParams != size) {
        _error = Error::WrongNumberOfArgs;
        return false;
    }
    
    // Execute init();
    if (Op(getUInt8ROM(_initStart)) != Op::SetFrame) {
        _error = Error::ExpectedSetFrame;
        return false;
    }

    // Push a dummy pc, for the return
    _stack.push(uint32_t(-1));
    execute(_initStart);
    if (_error == Error::None) {
        _error = _stack.error();
    }
    return _error == Error::None;
}

int32_t
Interpreter::loop()
{
    if (Op(getUInt8ROM(_loopStart)) != Op::SetFrame) {
        _error = Error::ExpectedSetFrame;
        return false;
    }

    // Push a dummy pc, for the return
    _stack.push(uint32_t(-1));
    return execute(_loopStart);
}

int32_t
Interpreter::execute(uint16_t addr)
{
    _pc = addr;
    
    while(1) {
        if (_stack.error() != Error::None) {
            _error = _stack.error();
        }
        if (_error != Error::None) {
            _errorAddr = _pc - 1;
            return -1;
        }
        
        uint8_t cmd = getUInt8ROM(_pc++);
        uint8_t index = 0;
        if (cmd >= 0x80) {
            index = cmd & 0x0f;
            cmd &= 0xf0;
        }

        uint8_t id;
        uint16_t targ;
        uint8_t numParams;
        uint8_t numLocals;
        uint32_t value;
        
        switch(Op(cmd)) {
			default:
				_error = Error::InvalidOp;
				return -1;
            case Op::Push:
                id = getId();
                _stack.push(loadInt(id));
                break;
            case Op::Pop:
                id = getId();
                storeInt(id, _stack.pop());
                break;
            case Op::PushIntConst:
                _stack.push(getConst());
                break;
            case Op::PushIntConstS: {
                _stack.push(index);
                break;
            }    
            case Op::PushRef:
                _stack.push(getId());
                break;
            case Op::PushDeref:
                value = _stack.pop();
                _stack.push(loadInt(value));
                break;
            case Op::PopDeref:
                value = _stack.pop();
                index = _stack.pop();
                storeInt(index, value);
                break;

            case Op::Offset:
                _stack.top() += index;
                break;
            case Op::Index:
                value = _stack.pop();
                _stack.top() += value * index;
                break;

            case Op::Dup:
                _stack.push(_stack.top());
                break;
            case Op::Drop:
                _stack.pop();
                break;
            case Op::Swap:
                _stack.swap();
                break;

            case Op::If:
                id = getSz();
                if (_stack.pop() == 0) {
                    // Skip if
                    _pc += id;
                    
                    // Next instruction must be EndIf or Else
                    cmd = getUInt8ROM(_pc++);
                    if (Op(cmd) == Op::EndIf) {
                        // We hit the end of the if, just continue
                    } else if (Op(cmd) == Op::Else) {
                        // We have an Else, execute it
                        getSz(); // Ignore Sz
                    } else {
                        _error = Error::UnexpectedOpInIf;
                        return -1;
                    }
                }
                break;
            case Op::Else:
                // If we get here the corresponding If succeeded so ignore this
                _pc += getSz();
                break;
            case Op::EndIf:
                // This is the end of an if, always ignore it
                break;

            case Op::Jump:
                _pc += getSz();
                break;
            case Op::Loop:
                _pc -= getSz();
                break;

            case Op::Log: {
                if (_stringBuf) {
                    delete [ ] _stringBuf;
                }
                _stringSize = getSz();
                _stringBuf = new char[_stringSize + 1];
                
                for (int i = 0; i < _stringSize; ++i) {
                    _stringBuf[i] = getConst();
                }
                
                _stringBuf[_stringSize] = '\0';
                log(_stringBuf, index);
                break;
            }
            
            case Op::Call:
                targ = uint16_t(getId()) | (uint16_t(index)  << 8);
                _stack.push(_pc);
                _pc = targ + _codeOffset;
                
                if (Op(getUInt8ROM(_pc)) != Op::SetFrame) {
                    _error = Error::ExpectedSetFrame;
                    return -1;
                }
                break;
            case Op::CallNative: {
                id = getConst();
                
                // Find the function
                bool found = false;
                for (int i = 0; i < _nativeModulesSize; ++i) {
                    if (_nativeModules[i]->hasId(id)) {
                        found = true;
                        
                        // Save the _pc just to make setFrame work
                        _stack.push(_pc);
                        
                        if (!_stack.setFrame(_nativeModules[i]->numParams(id), 0)) {
                            return -1;
                        }

                        int32_t returnVal = _nativeModules[i]->call(this, id);
 
                        _pc = _stack.restoreFrame(returnVal);
                        break;
                    }
                }
                
                if (!found) {
                    _error = Error::InvalidNativeFunction;
                    return -1;
                }
                break;
            }
            case Op::Return: {
                uint32_t retVal = _stack.empty() ? 0 : _stack.pop();
                
                if (_stack.empty()) {
                    // Returning from top level
                    return 0;
                }
                
                // TOS has return value. Pop it and push it back after restore
                _pc = _stack.restoreFrame(retVal);
                
                // A _pc of -1 returns from top level
                if (_pc < 0) {
                    // retVal was pushed, get rid of it
                    _stack.pop();
                    return retVal;
                }
                break;
            }
            case Op::SetFrame:
                getPL(numParams, numLocals);
                if (!_stack.setFrame(numParams, numLocals)) {
                    return -1;
                }
                break;

            case Op::Or:    _stack.top() |= _stack.pop(); break;
            case Op::Xor:   _stack.top() ^= _stack.pop(); break;
            case Op::And:   _stack.top() &= _stack.pop(); break;
            case Op::Not:   _stack.top() = ~_stack.top(); break;
            case Op::LOr:   _stack.push(_stack.pop() || _stack.pop()); break;
            case Op::LAnd:   _stack.push(_stack.pop() && _stack.pop()); break;
            case Op::LNot:   _stack.top() = !_stack.top(); break;

            case Op::LTInt:
                value = _stack.pop();
                _stack.top() = int32_t(_stack.top()) < int32_t(value);
                break;
            case Op::LTFloat:
                value = _stack.pop();
                _stack.top() = intToFloat(_stack.top()) < intToFloat(value);
                break;
            case Op::LEInt:
                value = _stack.pop();
                _stack.top() = int32_t(_stack.top()) <= int32_t(value);
                break;
            case Op::LEFloat:
                value = _stack.pop();
                _stack.top() = intToFloat(_stack.top()) <= intToFloat(value);
                break;
            case Op::EQInt:
                value = _stack.pop();
                _stack.top() = int32_t(_stack.top()) == int32_t(value);
                break;
            case Op::EQFloat:
                value = _stack.pop();
                _stack.top() = intToFloat(_stack.top()) == intToFloat(value);
                break;
            case Op::NEInt:
                value = _stack.pop();
                _stack.top() = int32_t(_stack.top()) != int32_t(value);
                break;
            case Op::NEFloat:
                value = _stack.pop();
                _stack.top() = intToFloat(_stack.top()) != intToFloat(value);
                break;
            case Op::GEInt:
                value = _stack.pop();
                _stack.top() = int32_t(_stack.top()) >= int32_t(value);
                break;
            case Op::GEFloat:
                value = _stack.pop();
                _stack.top() = intToFloat(_stack.top()) >= intToFloat(value);
                break;
            case Op::GTInt:
                value = _stack.pop();
                _stack.top() = int32_t(_stack.top()) > int32_t(value);
                break;
            case Op::GTFloat:
                value = _stack.pop();
                _stack.top() = intToFloat(_stack.top()) > intToFloat(value);
                break;

            case Op::AddInt:
                value = _stack.pop();
                _stack.top() = int32_t(_stack.top()) + int32_t(value);
                break;
            case Op::AddFloat:
                value = _stack.pop();
                _stack.top() = floatToInt(intToFloat(_stack.top()) + intToFloat(value));
                break;
            case Op::SubInt:
                value = _stack.pop();
                _stack.top() = int32_t(_stack.top()) - int32_t(value);
                break;
            case Op::SubFloat:
                value = _stack.pop();
                _stack.top() = floatToInt(intToFloat(_stack.top()) - intToFloat(value));
                break;
            case Op::MulInt:
                value = _stack.pop();
                _stack.top() = int32_t(_stack.top()) * int32_t(value);
                break;
            case Op::MulFloat:
                value = _stack.pop();
                _stack.top() = floatToInt(intToFloat(_stack.top()) * intToFloat(value));
                break;
            case Op::DivInt:
                value = _stack.pop();
                _stack.top() = int32_t(_stack.top()) / int32_t(value);
                break;
            case Op::DivFloat:
                value = _stack.pop();
                _stack.top() = floatToInt(intToFloat(_stack.top()) / intToFloat(value));
                break;
            case Op::NegInt:
                _stack.top() = -int32_t(_stack.top());
                break;
            case Op::NegFloat:
                _stack.top() = floatToInt(-intToFloat(_stack.top()));
                break;

            case Op::PreIncInt:
            case Op::PreDecInt:
            case Op::PostIncInt:
            case Op::PostDecInt: {
                uint32_t addr = _stack.pop();
                int32_t value = int32_t(loadInt(addr));
                int32_t valueAfter = (Op(cmd) == Op::PreIncInt || Op(cmd) == Op::PostIncInt) ? (value + 1) : (value - 1);
                storeInt(addr, valueAfter);
                _stack.push((Op(cmd) == Op::PreIncInt || Op(cmd) == Op::PreDecInt) ? valueAfter : value);
                break;
            }
            case Op::PreIncFloat:
            case Op::PreDecFloat:
            case Op::PostIncFloat:
            case Op::PostDecFloat: {
                uint32_t addr = _stack.pop();
                float value = loadFloat(addr);
                float valueAfter = (Op(cmd) == Op::PreIncFloat || Op(cmd) == Op::PostIncFloat) ? (value + 1) : (value - 1);
                storeFloat(addr, valueAfter);
                _stack.push((Op(cmd) == Op::PreIncFloat || Op(cmd) == Op::PreDecFloat) ? floatToInt(valueAfter) : floatToInt(value));
                break;
            }
        }
    }
}

// Return -1 if we just finished going down, or 1 
// if we just finished going up. Otherwise return 0.
int32_t
Interpreter::animate(uint32_t index)
{
    float cur = loadFloat(index, 0);
    float inc = loadFloat(index, 1);
    float min = loadFloat(index, 2);
    float max = loadFloat(index, 3);

    cur += inc;
    storeFloat(index, 0, cur);

    if (0 < inc) {
        if (cur >= max) {
            cur = max;
            inc = -inc;
            storeFloat(index, 0, cur);
            storeFloat(index, 1, inc);
            return 1;
        }
    } else {
        if (cur <= min) {
            cur = min;
            inc = -inc;
            storeFloat(index, 0, cur);
            storeFloat(index, 1, inc);
            return -1;
        }
    }
    return 0;
}

bool
Interpreter::log(const char* fmt, uint8_t numArgs)
{
    // This is a very simplified version of printf. It
    // handles '%i' and '%f'
    uint8_t arg = numArgs;
    
    for (int i = 0; ; ) {
        char c[2];;
        c[1] = '\0';
        c[0] = fmt[i++];
        if (c[0] == '\0') {
            break;
        }
        
        if (c[0] == '%') {
            c[0] = fmt[i++];
            if (c[0] == '%') {
                logString(c);
            } else if (c[0] == 'i') {
                if (arg == 0) {
                    return false;
                }
                String v = to_string(int32_t(_stack.top(arg - 1)));
                --arg;
                logString(v.c_str());
            } else if (c[0] == 'f') {
                if (arg == 0) {
                    return false;
                }
                String v = to_string(intToFloat(_stack.top(arg - 1)));
                --arg;
                logString(v.c_str());
            } else {
                return false;
            }
        } else {
            logString(c);
        }
    }
    
    _stack.pop(numArgs);
    return true;
}
