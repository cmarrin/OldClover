//
//  CompileEngine.cpp
//  CompileArly
//
//  Created by Chris Marrin on 1/9/22.
//

#include "CompileEngine.h"

#include "Interpreter.h"
#include <cmath>
#include <map>

using namespace arly;

static std::vector<OpData> _opcodes = {
    { "Push",           Op::Push            , OpParams::Id },
    { "Pop",            Op::Pop             , OpParams::Id },
    { "PushIntConst",   Op::PushIntConst    , OpParams::Const },
    { "PushIntConstS",  Op::PushIntConstS   , OpParams::Index },

    { "PushRef",        Op::PushRef         , OpParams::Id },
    { "PushDeref",      Op::PushDeref       , OpParams::None },
    { "PopDeref",       Op::PopDeref        , OpParams::None },

    { "Dup",            Op::Dup             , OpParams::None },
    { "Drop",           Op::Drop            , OpParams::None },
    { "Swap",           Op::Swap            , OpParams::None },
    
    { "if",             Op::If              , OpParams::Sz },
    { "else",           Op::Else            , OpParams::Sz },

    { "Call",           Op::Call            , OpParams::Target },
    { "CallNative",     Op::CallNative      , OpParams::Const },
    { "Return",         Op::Return          , OpParams::None },
    { "SetFrame",       Op::SetFrame        , OpParams::P_L },

    { "Jump",           Op::Jump            , OpParams::Sz },
    { "Loop",           Op::Loop            , OpParams::Sz },
    
    { "Or",             Op::Or              , OpParams::None },
    { "Xor",            Op::Xor             , OpParams::None },
    { "And",            Op::And             , OpParams::None },
    { "Not",            Op::Not             , OpParams::None },
    { "LOr",            Op::LOr             , OpParams::None },
    { "LAnd",           Op::LAnd            , OpParams::None },
    { "LNot",           Op::LNot            , OpParams::None },
    { "LTInt",          Op::LTInt           , OpParams::None },
    { "LTFloat",        Op::LTFloat         , OpParams::None },
    { "LEInt",          Op::LEInt           , OpParams::None },
    { "LEFloat",        Op::LEFloat         , OpParams::None },
    { "EQInt",          Op::EQInt           , OpParams::None },
    { "EQFloat",        Op::EQFloat         , OpParams::None },
    { "NEInt",          Op::NEInt           , OpParams::None },
    { "NEFloat",        Op::NEFloat         , OpParams::None },
    { "GEInt",          Op::GEInt           , OpParams::None },
    { "GEFloat",        Op::GEFloat         , OpParams::None },
    { "GTInt",          Op::GTInt           , OpParams::None },
    { "GTFloat",        Op::GTFloat         , OpParams::None },
    { "AddInt",         Op::AddInt          , OpParams::None },
    { "AddFloat",       Op::AddFloat        , OpParams::None },
    { "SubInt",         Op::SubInt          , OpParams::None },
    { "SubFloat",       Op::SubFloat        , OpParams::None },
    { "MulInt",         Op::MulInt          , OpParams::None },
    { "MulFloat",       Op::MulFloat        , OpParams::None },
    { "DivInt",         Op::DivInt          , OpParams::None },
    { "DivFloat",       Op::DivFloat        , OpParams::None },
    { "NegInt",         Op::NegInt          , OpParams::None },
    { "NegFloat",       Op::NegFloat        , OpParams::None },

    { "PreIncInt",      Op::PreIncInt       , OpParams::None },
    { "PreIncFloat",    Op::PreIncFloat     , OpParams::None },
    { "PreDecInt",      Op::PreDecInt       , OpParams::None },
    { "PreDecFloat",    Op::PreDecFloat     , OpParams::None },
    { "PostIncInt",     Op::PostIncInt      , OpParams::None },
    { "PostIncFloat",   Op::PostIncFloat    , OpParams::None },
    { "PostDecInt",     Op::PostDecInt      , OpParams::None },
    { "PostDecFloat",   Op::PostDecFloat    , OpParams::None },
    
    { "Offset",         Op::Offset          , OpParams::Index },
    { "Index",          Op::Index           , OpParams::Index },
};

CompileEngine::CompileEngine(std::istream* stream, std::vector<std::pair<int32_t, std::string>>* annotations)
    : _scanner(stream, annotations)
{
}

void
CompileEngine::emit(std::vector<uint8_t>& executable)
{
    // Set the stack to be whatever the high water was for
    // function variable allocation plue StackOverhead.
    // If this is more than MaxStackSize, then we have a 
    // problem.
    uint32_t stackSize = uint32_t(_localHighWaterMark) + StackOverhead;
    expect(stackSize <= MaxStackSize, Compiler::Error::StackTooBig);
    
    executable.push_back('a');
    executable.push_back('r');
    executable.push_back('l');
    executable.push_back('y');
    executable.push_back(uint8_t(_rom32.size()));
    executable.push_back(uint8_t(_globalSize));
    executable.push_back(uint8_t(stackSize));
    executable.push_back(0);
    
    char* buf = reinterpret_cast<char*>(&(_rom32[0]));
    executable.insert(executable.end(), buf, buf + _rom32.size() * 4);

    for (int i = 0; i < _effects.size(); ++i) {
        executable.push_back(_effects[i]._cmd);
        executable.push_back(_effects[i]._count);
        executable.push_back(uint8_t(_effects[i]._initAddr));
        executable.push_back(uint8_t(_effects[i]._initAddr >> 8));
        executable.push_back(uint8_t(_effects[i]._loopAddr));
        executable.push_back(uint8_t(_effects[i]._loopAddr >> 8));
    }
    executable.push_back(0);
    
    buf = reinterpret_cast<char*>(&(_rom8[0]));
    executable.insert(executable.end(), buf, buf + _rom8.size());
}

bool
CompileEngine::def()
{
     if (!match(Reserved::Def)) {
        return false;
    }
    
    std::string id;
    int32_t val;
    
    expect(identifier(id), Compiler::Error::ExpectedIdentifier);
    expect(integerValue(val), Compiler::Error::ExpectedValue);
    
    // A def is always a positive integer less than 256
    expect(val >= 0 && val < 256, Compiler::Error::DefOutOfRange);
    
    _defs.emplace_back(id, val);
    return true;
}

bool
CompileEngine::constant()
{
    if (!match(Reserved::Const)) {
        return false;
    }
    
    Type t;
    std::string id;
    int32_t val;
    
    expect(type(t), Compiler::Error::ExpectedType);
    expect(identifier(id), Compiler::Error::ExpectedIdentifier);
    expect(value(val, t), Compiler::Error::ExpectedValue);
    
    // There is only enough room for 128 constant values
    expect(_rom32.size() < 128, Compiler::Error::TooManyConstants);

    // Save constant
    _globals.emplace_back(id, _rom32.size(), t, Symbol::Storage::Const);
    _rom32.push_back(val);
    
    return true;
}

bool
CompileEngine::effect()
{
    if (!match(Reserved::Effect)) {
        return false;
    }

    std::string id;

    expect(identifier(id), Compiler::Error::ExpectedIdentifier);
    
    // Effect Identifier must be a single char from 'a' to 'p'
    if (id.size() != 1 || id[0] < 'a' || id[0] > 'p') {
        _error = Compiler::Error::ExpectedCommandId;
        throw true;
    }

    int32_t paramCount;
    expect(integerValue(paramCount), Compiler::Error::ExpectedValue);
    
    // paramCount must be 0 - 15
    if (paramCount < 0 || paramCount > 15) {
        _error = Compiler::Error::InvalidParamCount;
        throw true;
    }
    
    _effects.emplace_back(id[0], paramCount, handleFunctionName().addr(), handleFunctionName().addr());
    return true;
}

bool
CompileEngine::type(Type& t)
{
    if (match(Reserved::Float)) {
        t = Type::Float;
        return true;
    }
    if (match(Reserved::Int)) {
        t = Type::Int;
        return true;
    }
    
    // See if it's a struct
    
    return false;
}

bool
CompileEngine::values(Type t)
{
    bool haveValues = false;
    while(1) {
        int32_t val;
        if (!value(val, t)) {
            break;
        }
        haveValues = true;
        
        _rom32.push_back(val);
    }
    return haveValues;
}


bool
CompileEngine::value(int32_t& i, Type t)
{
    bool neg = false;
    if (match(Token::Minus)) {
        neg = true;
    }
    
    float f;
    if (floatValue(f)) {
        if (neg) {
            f = -f;
        }

        // If we're expecting an Integer, convert it
        if (t == Type::Int) {
            i = roundf(f);
        } else {
            i = *(reinterpret_cast<int32_t*>(&f));
        }
        return true;
    }
    
    if (integerValue(i)) {
        if (neg) {
            i = -i;
        }
        
        // If we're expecting a float, convert it
        if (t == Type::Float) {
            f = float(i);
            i = *(reinterpret_cast<int32_t*>(&f));
        }
        return true;
    }
    return false;
}

void
CompileEngine::expect(Token token, const char* str)
{
    bool err = false;
    if (_scanner.getToken() != token) {
        _error = Compiler::Error::ExpectedToken;
        err = true;
    }
    
    if (str && _scanner.getTokenString() != str) {
        _error = Compiler::Error::ExpectedToken;
        err = true;
    }
    
    if (err) {
        _expectedToken = token;
        if (str) {
            _expectedString = str;
        } else if (uint8_t(token) < 0x80) {
            _expectedString = char(token);
        } else {
            _expectedString = "";
        }
        throw true;
    }

    _scanner.retireToken();
}

void
CompileEngine::expect(bool passed, Compiler::Error error)
{
    if (!passed) {
        _error = error;
        throw true;
    }
}

void
CompileEngine::expectWithoutRetire(Token token)
{
    if (_scanner.getToken() != token) {
        _expectedToken = token;
        _expectedString = "";
        _error = Compiler::Error::ExpectedToken;
        throw true;
    }
}

bool
CompileEngine::match(Reserved r)
{
    Reserved rr;
    if (!isReserved(_scanner.getToken(), _scanner.getTokenString(), rr)) {
        return false;
    }
    if (r != rr) {
        return false;
    }
    _scanner.retireToken();
    return true;
}

bool
CompileEngine::match(Token t)
{
    if (_scanner.getToken() != t) {
        return false;
    }
    _scanner.retireToken();
    return true;
}

bool
CompileEngine::identifier(std::string& id, bool retire)
{
    if (_scanner.getToken() != Token::Identifier) {
        return false;
    }
    
    if (reserved()) {
        return false;
    }
    
    id = _scanner.getTokenString();
    
    if (retire) {
        _scanner.retireToken();
    }
    return true;
}

bool
CompileEngine::integerValue(int32_t& i)
{
    if (_scanner.getToken() != Token::Integer) {
        return false;
    }
    
    i = _scanner.getTokenValue().integer;
    _scanner.retireToken();
    return true;
}

bool
CompileEngine::floatValue(float& f)
{
    if (_scanner.getToken() != Token::Float) {
        return false;
    }
    
    f = _scanner.getTokenValue().number;
    _scanner.retireToken();
    return true;
}

bool
CompileEngine::reserved()
{
    Reserved r;
    return isReserved(_scanner.getToken(), _scanner.getTokenString(), r);
}

bool
CompileEngine::reserved(Reserved &r)
{
    return isReserved(_scanner.getToken(), _scanner.getTokenString(), r);
}

const CompileEngine::Function&
CompileEngine::handleFunctionName()
{
    std::string targ;
    expect(identifier(targ), Compiler::Error::ExpectedIdentifier);
    
    auto it = find_if(_functions.begin(), _functions.end(),
                    [targ](const Function& fun) { return fun.name() == targ; });
    expect(it != _functions.end(), Compiler::Error::UndefinedIdentifier);

    return *it;
}

bool
CompileEngine::opDataFromString(const std::string str, OpData& data)
{
    auto it = find_if(_opcodes.begin(), _opcodes.end(),
                    [str](const OpData& opData) { return opData._str == str; });
    if (it != _opcodes.end()) {
        data = *it;
        return true;
    }
    return false;
}

bool
CompileEngine::opDataFromOp(const Op op, OpData& data)
{
    auto it = find_if(_opcodes.begin(), _opcodes.end(),
                    [op](const OpData& opData) { return opData._op == op; });
    if (it != _opcodes.end()) {
        data = *it;
        return true;
    }
    return false;
}

bool
CompileEngine::isReserved(Token token, const std::string str, Reserved& r)
{
    static std::map<std::string, Reserved> reserved = {
        { "def",        Reserved::Def },
        { "const",      Reserved::Const },
        { "table",      Reserved::Table },
        { "var",        Reserved::Var },
        { "function",   Reserved::Function },
        { "effect",     Reserved::Effect },
        { "foreach",    Reserved::ForEach },
        { "if",         Reserved::If },
        { "else",       Reserved::Else },
        { "float",      Reserved::Float },
        { "int",        Reserved::Int },
    };

    if (token != Token::Identifier) {
        return false;
    }
    
    auto it = reserved.find(str);
    if (it != reserved.end()) {
        r = it->second;
        return true;
    }
    return false;
}

bool
CompileEngine::findSymbol(const std::string& s, Symbol& sym)
{
    auto it = find_if(_globals.begin(), _globals.end(),
                    [s](const Symbol& sym) { return sym.name() == s; });

    if (it == _globals.end()) {    
        // Not found. See if it's a local to the current function (param or var)
        it = find_if(currentLocals().begin(), currentLocals().end(),
                        [s](const Symbol& p) { return p.name() == s; });

        if (it == currentLocals().end()) {
            return false;
        }
    }
    sym = *it;
    return true;
}

bool
CompileEngine::findFunction(const std::string& s, Function& fun)
{
    auto it = find_if(_functions.begin(), _functions.end(),
                    [s](const Function& fun) { return fun.name() == s; });

    if (it != _functions.end()) {
        fun = *it;
        return true;
    }
    
    return false;
}

uint8_t
CompileEngine::Symbol::addr() const
{
    switch(_storage) {
        case CompileEngine::Symbol::Storage::None:
            return 0;
        case CompileEngine::Symbol::Storage::Const:
            return _addr + ConstStart;
        case CompileEngine::Symbol::Storage::Global:
            return _addr + GlobalStart;
        case CompileEngine::Symbol::Storage::Local:
            return _addr + LocalStart;
    }
}

