//
//  Compiler.h
//  CompileArly
//
//  Created by Chris Marrin on 1/9/22.
//
// Arly compiler
//
// Compile Arly statements into a binary stream which can be loaded and executed
//

#pragma once

#include "Interpreter.h"
#include "Opcodes.h"
#include "Scanner.h"
#include <cstdint>
#include <istream>
#include <vector>

namespace arly {

struct OpData
{
    OpData() { }
    OpData(std::string str, Op op, OpParams par) : _str(str), _op(op), _par(par) { }
    std::string _str;
    Op _op = Op::Return;
    OpParams _par = OpParams::None;
};

class Compiler
{
public:
    enum class Error {
        None,
        UnrecognizedLanguage,
        ExpectedToken,
        ExpectedType,
        ExpectedValue,
        ExpectedString,
        ExpectedRef,
        ExpectedOpcode,
        ExpectedEnd,
        ExpectedIdentifier,
        ExpectedCommandId,
        ExpectedExpr,
        ExpectedArgList,
        ExpectedFormalParams,
        ExpectedFunction,
        ExpectedLHSExpr,
        ExpectedStructType,
        AssignmentNotAllowedHere,
        InvalidStructId,
        InvalidParamCount,
        UndefinedIdentifier,
        ParamOutOfRange,
        JumpTooBig,
        IfTooBig,
        ElseTooBig,
        StringTooLong,
        TooManyConstants,
        TooManyVars,
        DefOutOfRange,
        ExpectedDef,
        NoMoreTemps,
        TempNotAllocated,
        InternalError,
        StackTooBig,
        MismatchedType,
        WrongNumberOfArgs,
        WrongType,
        OnlyAllowedInLoop,
    };
    
    Compiler() { }
    
    enum class Language { Arly, Clover };
    
    bool compile(std::istream*, Language, std::vector<uint8_t>& executable,
                 const std::vector<NativeModule*>&,
                 std::vector<std::pair<int32_t, std::string>>* annotations = nullptr);

    Error error() const { return _error; }
    Token expectedToken() const { return _expectedToken; }
    const std::string& expectedString() const { return _expectedString; }
    uint32_t lineno() const { return _lineno; }
    uint32_t charno() const { return _charno; }        

private:
    Error _error = Error::None;
    Token _expectedToken = Token::None;
    std::string _expectedString;
    uint32_t _lineno;
    uint32_t _charno;
};

}
