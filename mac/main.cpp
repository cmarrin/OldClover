/*-------------------------------------------------------------------------
    This source file is a part of Clover
    For the latest info, see https://github.com/cmarrin/Clover
    Copyright (c) 2021-2022, Chris Marrin
    All rights reserved.
    Use of this source code is governed by the MIT license that can be
    found in the LICENSE file.
-------------------------------------------------------------------------*/

#include "Compiler.h"
#include "Decompiler.h"
#include "Interpreter.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <getopt.h>
#include <cstdio>

static constexpr uint32_t MaxExecutableSize = 1024;
static constexpr int NumLoops = 0;

// Simulator
//
// Subclass of Interpreter that outputs device info to consolee
class Simulator : public clvr::Interpreter
{
public:
    Simulator(clvr::NativeModule** mod = nullptr, uint32_t modSize = 0) : Interpreter(mod, modSize) { }
    virtual ~Simulator() { }
    
    virtual uint8_t rom(uint16_t i) const override
    {
        return (i < MaxExecutableSize) ? _rom[i] : 0;
    }
    
    virtual void log(const char* s) const override
    {
        std::cout << s << std::flush;
    }

    void setROM(const std::vector<uint8_t>& buf)
    {
        size_t size = buf.size();
        if (size > 1024){
            size = 1024;
        }
        memcpy(_rom, &buf[0], size);
    }
    
private:
    uint8_t _rom[1024];
};

// compile [-xdsh] <input file>...
//
//      -s      output binary in 64 byte segments (named <root name>00.{clvr,arly}, etc.
//      -h      output in include file format. Output file is <root name>.h
//      -d      decompile and print result
//      -x      simulate resulting binary
//
// Multiple input files accepted. Output file(s) are placed in the same dir as input
// files with extension .arlx or .h. If segmented (-s), filename has 2 digit suffix
// added before the .arlx.

// Include file format
//
// This file can be included in an Arduino sketch to upload to EEPROM. The file
// contains 'static const uint8_t PROGMEM EEPROM_Upload = {', followed by each
// byte of the binary data as a hex value. It also has a
// 'static constexpr uint16_t EEPROM_Upload_Size = ' with the number of bytes.

struct Test
{
    const char* _cmd;
    std::vector<uint8_t> _buf;
};

static std::vector<Test> Tests = {
    { "test", { 4, 7, 11 } },
};

static void showError(clvr::Compiler::Error error, clvr::Token token, const std::string& str, uint32_t lineno, uint32_t charno)
{
    const char* err = "unknown";
    switch(error) {
        case clvr::Compiler::Error::None: err = "internal error"; break;
        case clvr::Compiler::Error::UnrecognizedLanguage: err = "unrecognized language"; break;
        case clvr::Compiler::Error::ExpectedToken: err = "expected token"; break;
        case clvr::Compiler::Error::ExpectedType: err = "expected type"; break;
        case clvr::Compiler::Error::ExpectedValue: err = "expected value"; break;
        case clvr::Compiler::Error::ExpectedString: err = "expected string"; break;
        case clvr::Compiler::Error::ExpectedRef: err = "expected ref"; break;
        case clvr::Compiler::Error::ExpectedOpcode: err = "expected opcode"; break;
        case clvr::Compiler::Error::ExpectedEnd: err = "expected 'end'"; break;
        case clvr::Compiler::Error::ExpectedIdentifier: err = "expected identifier"; break;
        case clvr::Compiler::Error::ExpectedExpr: err = "expected expression"; break;
        case clvr::Compiler::Error::ExpectedLHSExpr: err = "expected left-hand side expression"; break;
        case clvr::Compiler::Error::ExpectedArgList: err = "expected arg list"; break;
        case clvr::Compiler::Error::ExpectedFormalParams: err = "expected formal params"; break;
        case clvr::Compiler::Error::ExpectedFunction: err = "expected function name"; break;
        case clvr::Compiler::Error::ExpectedStructType: err = "expected Struct type"; break;
        case clvr::Compiler::Error::ExpectedVar: err = "expected var"; break;
        case clvr::Compiler::Error::AssignmentNotAllowedHere: err = "assignment not allowed here"; break;
        case clvr::Compiler::Error::InvalidStructId: err = "invalid Struct identifier"; break;
        case clvr::Compiler::Error::InvalidParamCount: err = "invalid param count"; break;
        case clvr::Compiler::Error::UndefinedIdentifier: err = "undefined identifier"; break;
        case clvr::Compiler::Error::ParamOutOfRange: err = "param must be 0..15"; break;
        case clvr::Compiler::Error::JumpTooBig: err = "tried to jump too far"; break;
        case clvr::Compiler::Error::IfTooBig: err = "too many instructions in if"; break;
        case clvr::Compiler::Error::ElseTooBig: err = "too many instructions in else"; break;
        case clvr::Compiler::Error::StringTooLong: err = "string too long"; break;
        case clvr::Compiler::Error::TooManyConstants: err = "too many constants"; break;
        case clvr::Compiler::Error::TooManyVars: err = "too many vars"; break;
        case clvr::Compiler::Error::DefOutOfRange: err = "def out of range"; break;
        case clvr::Compiler::Error::ExpectedDef: err = "expected def"; break;
        case clvr::Compiler::Error::NoMoreTemps: err = "no more temp variables available"; break;
        case clvr::Compiler::Error::TempNotAllocated: err = "temp not allocated"; break;
        case clvr::Compiler::Error::InternalError: err = "internal error"; break;
        case clvr::Compiler::Error::StackTooBig: err = "stack too big"; break;
        case clvr::Compiler::Error::MismatchedType: err = "mismatched type"; break;
        case clvr::Compiler::Error::WrongType: err = "wrong type"; break;
        case clvr::Compiler::Error::WrongNumberOfArgs: err = "wrong number of args"; break;
        case clvr::Compiler::Error::OnlyAllowedInLoop: err = "break/continue only allowed in loop"; break;
        case clvr::Compiler::Error::DuplicateCmd: err = "duplicate command"; break;
        case clvr::Compiler::Error::ExecutableTooBig: err = "executable too big"; break;
    }
    
    if (token == clvr::Token::EndOfFile) {
        err = "unexpected tokens after EOF";
    }
    
    std::cout << "Compile failed: " << err;
    if (!str.empty()) {
        std::cout << " ('" << str << "')";
    }
    std::cout << " on line " << lineno << ":" << charno << "\n";
}

int main(int argc, char * const argv[])
{
    std::cout << "Clover Compiler v0.2\n\n";
    
    int c;
    bool execute = false;
    bool decompile = false;
    bool segmented = false;
    bool headerFile = false;
    
    while ((c = getopt(argc, argv, "dxsh")) != -1) {
        switch(c) {
            case 'd': decompile = true; break;
            case 'x': execute = true; break;
            case 's': segmented = true; break;
            case 'h': headerFile = true; break;
            default: break;
        }
    }
    
    // If headerFile is true, segmented is ignored.
    if (headerFile) {
        segmented = false;
    }
    
    if (optind >= argc) {
        std::cout << "No input file given\n";
        return 0;
    }
    
    std::vector<std::string> inputFiles;
    
    for (int i = 0; ; ++i) {
        inputFiles.push_back(argv[optind + i]);
        if (optind + i >= argc - 1) {
            break;
        }
    }
    
    std::vector<std::pair<int32_t, std::string>> annotations;

    for (const auto& it : inputFiles) {
        clvr::Compiler compiler;
        std::fstream stream;
        stream.open(it.c_str(), std::fstream::in);
        if (stream.fail()) {
            std::cout << "Can't open '" << it << "'\n";
            return 0;
        }
        
        std::cout << "Compiling '" << it << "'\n";
        
        std::vector<uint8_t> executable;
        
        clvr::Compiler::Language lang;
        std::string suffix = it.substr(it.find_last_of('.'));
        if (suffix == ".clvr") {
            lang = clvr::Compiler::Language::Clover;
        } else if (suffix == ".arly") {
            lang = clvr::Compiler::Language::Arly;
        } else {
            std::cout << "*** suffix '" << suffix << "' not recognized\n";
            return -1;
        }
        
        randomSeed(uint32_t(clock()));

        compiler.compile(&stream, lang, executable, MaxExecutableSize, { }, &annotations);
        if (compiler.error() != clvr::Compiler::Error::None) {
            showError(compiler.error(), compiler.expectedToken(), compiler.expectedString(), compiler.lineno(), compiler.charno());
            std::cout << "          Executable size=" << std::to_string(executable.size()) << "\n";
            return -1;
        }

        std::cout << "Compile succeeded. Executable size=" << std::to_string(executable.size()) << "\n";
        
        // Write executable
        // Use the same name as the input file for the output
        std::string path = it.substr(0, it.find_last_of('.'));
        
        // Delete any old copies
        std::string name = path + ".h";
        remove(name.c_str());

        name = path + ".arlx";
        remove(name.c_str());

        for (int i = 0; ; ++i) {
            char buf[3];
            sprintf(buf, "%02u", i);
            name = path + buf + ".arlx";
            if (remove(name.c_str()) != 0) {
                break;
            }
        }
        
        std::cout << "\nEmitting executable to '" << path << "'\n";
        std::fstream outStream;
        
        // If segmented break it up into 64 byte chunks, prefix each file with start addr byte
        size_t sizeRemaining = executable.size();
        
        for (uint8_t i = 0; ; i++) {
            if (segmented) {
                char buf[3];
                sprintf(buf, "%02u", i);
                name = path + buf + ".arlx";
            } else if (headerFile) {
                name = path + ".h";
            } else {
                name = path + ".arlx";
            }
        
            std::ios_base::openmode mode = std::fstream::out;
            if (!headerFile) {
                mode|= std::fstream::binary;
            }
            
            outStream.open(name.c_str(), mode);
            if (outStream.fail()) {
                std::cout << "Can't open '" << name << "'\n";
                return 0;
            } else {
                char* buf = reinterpret_cast<char*>(&(executable[i * 64]));
                size_t sizeToWrite = sizeRemaining;
                
                if (segmented && sizeRemaining > 64) {
                    sizeToWrite = 64;
                }
                
                if (segmented) {
                    // Write the 2 byte offset
                    uint16_t addr = uint16_t(i) * 64;
                    outStream.put(uint8_t(addr & 0xff));
                    outStream.put(uint8_t(addr >> 8));
                }
                
                if (!headerFile) {
                    // Write the buffer
                    outStream.write(buf, sizeToWrite);
                    if (outStream.fail()) {
                        std::cout << "Save failed\n";
                        return 0;
                    } else {
                        sizeRemaining -= sizeToWrite;
                        outStream.close();
                        std::cout << "    Saved " << name << "\n";
                        if (sizeRemaining == 0) {
                            break;
                        }
                    }
                } else {
                    std::string name = path.substr(path.find_last_of('/') + 1);
                    outStream << "static const uint8_t PROGMEM EEPROM_Upload_" << name << "[ ] = {\n";
                    
                    for (size_t i = 0; i < sizeRemaining; ++i) {
                        char hexbuf[5];
                        sprintf(hexbuf, "0x%02x", executable[i]);
                        outStream << hexbuf << ", ";
                        if (i % 8 == 7) {
                            outStream << std::endl;
                        }
                    }

                    outStream << "};\n";
                    
                    break;
                }
            }
        }
        std::cout << "Executables saved\n";

        // decompile if needed
        if (decompile) {
            std::string out;
            clvr::Decompiler decompiler(&executable, &out, annotations);
            bool success = decompiler.decompile();
            std::cout << "\nDecompiled executable:\n" << out << "\nEnd decompilation\n\n";
            if (!success) {
                const char* err = "unknown";
                switch(decompiler.error()) {
                    case clvr::Decompiler::Error::None: err = "internal error"; break;
                    case clvr::Decompiler::Error::InvalidSignature: err = "invalid signature"; break;
                    case clvr::Decompiler::Error::InvalidOp: err = "invalid op"; break;
                    case clvr::Decompiler::Error::PrematureEOF: err = "premature EOF"; break;
                }
                std::cout << "Decompile failed: " << err << "\n\n";
                return 0;
            }
        }
        
        // Execute if needed
        if (execute) {
            Simulator sim;
            
            sim.setROM(executable);
            
            for (const Test& test : Tests) {
                std::cout << "Running '" << test._cmd << "' command...\n";
            
                bool success = sim.init(test._cmd, &test._buf[0], test._buf.size());
                if (success && NumLoops > 0) {
                    for (int i = 0; i < NumLoops; ++i) {
                        int32_t delay = sim.loop();
                        if (delay < 0) {
                            success = false;
                            break;
                        }
                        std::cout << "[" << i << "]: delay = " << delay << "\n";
                    }
                
                    if (success) {
                        std::cout << "Complete\n\n";
                    }
                }
                
                if (!success) {
                    const char* err = "unknown";
                    switch(sim.error()) {
                        case clvr::Interpreter::Error::None: err = "internal error"; break;
                        case clvr::Interpreter::Error::CmdNotFound: err = "command not found"; break;
                        case clvr::Interpreter::Error::UnexpectedOpInIf: err = "unexpected op in if (internal error)"; break;
                        case clvr::Interpreter::Error::InvalidOp: err = "invalid opcode"; break;
                        case clvr::Interpreter::Error::InvalidNativeFunction: err = "invalid native function"; break;
                        case clvr::Interpreter::Error::OnlyMemAddressesAllowed: err = "only Mem addresses allowed"; break;
                        case clvr::Interpreter::Error::StackOverrun: err = "can't call, stack full"; break;
                        case clvr::Interpreter::Error::StackUnderrun: err = "stack underrun"; break;
                        case clvr::Interpreter::Error::StackOutOfRange: err = "stack access out of range"; break;
                        case clvr::Interpreter::Error::AddressOutOfRange: err = "address out of range"; break;
                        case clvr::Interpreter::Error::InvalidModuleOp: err = "invalid operation in module"; break;
                        case clvr::Interpreter::Error::ExpectedSetFrame: err = "expected SetFrame as first function op"; break;
                        case clvr::Interpreter::Error::NotEnoughArgs: err = "not enough args on stack"; break;
                        case clvr::Interpreter::Error::WrongNumberOfArgs: err = "wrong number of args"; break;
                    }
                    std::cout << "Interpreter failed: " << err;
                    
                    int16_t errorAddr = sim.errorAddr();
                    if (errorAddr >= 0) {
                        std::cout << " at addr " << errorAddr;
                    }
                    
                    std::cout << "\n\n";
                }
            }
        }
    }

    return 1;
}
