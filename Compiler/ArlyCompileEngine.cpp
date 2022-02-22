//
//  ArlyCompileEngine.cpp
//  CompileArly
//
//  Created by Chris Marrin on 1/9/22.
//

#include "ArlyCompileEngine.h"

#include <map>

using namespace arly;

bool
ArlyCompileEngine::program()
{
    try {
        defs();
        constants();
        tables();
        vars();
        functions();
        effects();
        
        // Must be at the EOF
        ignoreNewLines();
        expect(Token::EndOfFile);
    }
    catch(...) {
        return false;
    }
    return true;
}

void
ArlyCompileEngine::defs()
{
    while(1) {
        ignoreNewLines();
        if (!def()) {
            return;
        }
        expect(Token::NewLine);
    }
}

void
ArlyCompileEngine::constants()
{
    while(1) {
        ignoreNewLines();
        if (!constant()) {
            return;
        }
        expect(Token::NewLine);
    }
}

void
ArlyCompileEngine::tables()
{
    while(1) {
        ignoreNewLines();
        if (!table()) {
            return;
        }
        expect(Token::NewLine);
    }
}

void
ArlyCompileEngine::functions()
{
    while(1) {
        ignoreNewLines();
        if (!function()) {
            return;
        }
        expect(Token::NewLine);
    }
}

void
ArlyCompileEngine::effects()
{
    while(1) {
        ignoreNewLines();
        if (!effect()) {
            return;
        }
        expect(Token::NewLine);
    }
}

void
ArlyCompileEngine::vars()
{
    while(1) {
        ignoreNewLines();
        if (!var()) {
            return;
        }
        expect(Token::NewLine);
    }
}

void
ArlyCompileEngine::statements()
{
    while(1) {
        ignoreNewLines();
        if (!statement()) {
            return;
        }
        expect(Token::NewLine);
    }
}

bool
ArlyCompileEngine::table()
{
    if (!match(Reserved::Table)) {
        return false;
    }
    
    Type t;
    std::string id;
    expect(type(t), Compiler::Error::ExpectedType);
    expect(identifier(id), Compiler::Error::ExpectedIdentifier);
    expect(Token::NewLine);
    
    // Set the start address of the table. tableEntries() will fill them in
    _globals.emplace_back(id, _rom32.size(), t, Symbol::Storage::Const);
    
    ignoreNewLines();
    tableEntries(t);
    expect(Token::Identifier, "end");
    return true;
}

void
ArlyCompileEngine::tableEntries(Type t)
{
    while (1) {
        ignoreNewLines();
        if (!values(t)) {
            break;
        }
        expect(Token::NewLine);
    }
}

bool
ArlyCompileEngine::var()
{
    if (!match(Reserved::Var)) {
        return false;
    }

    Type t;
    std::string id;
    
    expect(type(t), Compiler::Error::ExpectedType);
    
    expect(identifier(id), Compiler::Error::ExpectedIdentifier);
    
    int32_t size;
    expect(integerValue(size), Compiler::Error::ExpectedInt);

    // FIXME: deal with locals
    _globals.emplace_back(id, _nextMem, t, Symbol::Storage::Global);
    _nextMem += size;
    _globalSize = _nextMem;

    // There is only enough room for 128 var values
    expect(_nextMem <= 128, Compiler::Error::TooManyVars);

    return true;
}


bool
ArlyCompileEngine::opcode(Op op, std::string& str, OpParams& par)
{
    OpData data;
    if (!opDataFromOp(op, data)) {
        return false;
    }
    str = data._str;
    par = data._par;
    return true;
}

bool
ArlyCompileEngine::function()
{
    if (!match(Reserved::Function)) {
        return false;
    }
    
    std::string id;

    expect(identifier(id), Compiler::Error::ExpectedIdentifier);
    
    // Remember the function
    _functions.emplace_back(id, uint16_t(_rom8.size()));
    
    ignoreNewLines();
    
    statements();
    
    expect(Token::Identifier, "end");
    
    // Insert a return at the end of the function to make sure it returns
    addOpSingleByteIndex(Op::PushIntConstS, 0);
    _rom8.push_back(uint8_t(Op::Return));
    return true;
}

bool
ArlyCompileEngine::statement()
{
    if (forStatement()) {
        return true;
    }
    if (ifStatement()) {
        return true;
    }
    if (opStatement()) {
        return true;
    }
    return false;
}

uint8_t
ArlyCompileEngine::handleI()
{
    uint8_t i = handleConst();
    expect(i <= 15, Compiler::Error::ParamOutOfRange);
    return uint8_t(i);
}

uint8_t
ArlyCompileEngine::handleConst()
{
    int32_t i;
    std::string id;
    
    if (identifier(id)) {
        // See if this is a def
        auto it = find_if(_defs.begin(), _defs.end(),
                    [id](const Def& d) { return d._name == id; });
        if (it != _defs.end()) {
            return it->_value;
        }
        
        // See if it's a native function
        Function fun;
        expect(findFunction(id, fun), Compiler::Error::UndefinedIdentifier);
        expect(fun.isNative(), Compiler::Error::ExpectedDef);
        i = uint8_t(fun.native());
    } else {
        expect(integerValue(i), Compiler::Error::ExpectedInt);
    }

    expect(i >= 0 && i < 256, Compiler::Error::ParamOutOfRange);
    return uint8_t(i);
}

uint8_t
ArlyCompileEngine::handleId(Symbol::Storage& storage)
{
    std::string id;
    Symbol sym;
    expect(identifier(id), Compiler::Error::ExpectedIdentifier);
    expect(findSymbol(id, sym), Compiler::Error::UndefinedIdentifier);
    return sym.addr();
}

void
ArlyCompileEngine::handleOpParams(uint8_t a)
{
    _rom8.push_back(a);
    expectWithoutRetire(Token::NewLine);
}

void
ArlyCompileEngine::handleOpParams(uint8_t a, uint8_t b)
{
    _rom8.push_back(a);
    _rom8.push_back(b);
    expectWithoutRetire(Token::NewLine);
}

void
ArlyCompileEngine::handleOpParamsReverse(uint8_t a, uint8_t b)
{
    _rom8.push_back(b);
    _rom8.push_back(a);
    expectWithoutRetire(Token::NewLine);
}

void
ArlyCompileEngine::handleOpParamsRdRs(Op op, uint8_t rd, uint8_t rs)
{
    _rom8.push_back(uint8_t(op));
    _rom8.push_back((rd << 6) | (rs << 4));
    expectWithoutRetire(Token::NewLine);
}

void
ArlyCompileEngine::handleOpParamsRdRsI(Op op, uint8_t rd, uint8_t rs, uint8_t i)
{
    _rom8.push_back(uint8_t(op));
    _rom8.push_back((rd << 6) | (rs << 4) | (i & 0x0f));
    expectWithoutRetire(Token::NewLine);
}

void
ArlyCompileEngine::handleOpParamsRdIRs(Op op, uint8_t rd, uint8_t i, uint8_t rs)
{
    _rom8.push_back(uint8_t(op));
    _rom8.push_back((rd << 6) | (rs << 4) | (i & 0x0f));
    expectWithoutRetire(Token::NewLine);
}

void
ArlyCompileEngine::handleOpParamsRdRs(Op op, uint8_t id, uint8_t rd, uint8_t i, uint8_t rs)
{
    _rom8.push_back(uint8_t(op));
    _rom8.push_back(id);
    _rom8.push_back((rd << 6) | (rs << 4) | (i & 0x0f));
    expectWithoutRetire(Token::NewLine);
}

bool
ArlyCompileEngine::opStatement()
{
    Op op;
    OpParams par;
    if (!opcode(_scanner.getToken(), _scanner.getTokenString(), op, par)) {
        return false;
    }
    
    // Don't handle Else here
    if (op == Op::Else) {
        return false;
    }
    
    _scanner.retireToken();
    
    // Get the params in the sequence specified in OpParams
    switch(par) {
        case OpParams::None:  addOp(op); break;
        case OpParams::Id:    addOpId(op, handleId()); break;
        case OpParams::I:     addOpI(op, handleI()); break;
        case OpParams::Index: addOpSingleByteIndex(op, handleI()); break;
        case OpParams::Const: addOpConst(op, handleConst()); break;
        case OpParams::Target: {
            uint16_t targ = handleFunctionName().addr();
            _rom8.push_back(uint8_t(op) | uint8_t((targ >> 8) & 0x0f));
            _rom8.push_back(uint8_t(targ));
            break;
        }
        case OpParams::P_L:
            _rom8.push_back(uint8_t(op));
            _rom8.push_back((handleI() << 4) | handleI());
            break;
        case OpParams::Id_Sz:
        case OpParams::Sz:
            // Should never get here
            break;
    }

    expectWithoutRetire(Token::NewLine);
    return true;
}

bool
ArlyCompileEngine::forStatement()
{
    // FIXME: foreach is completely broken in arly.
    // No more foreach op. It's handled with Loop now
    if (!match(Reserved::ForEach)) {
        return false;
    }
    
    expect(Token::NewLine);
    
    // Output a placeholder for sz and rember where it is
    auto szIndex = _rom8.size();
    _rom8.push_back(0);
    
    statements();
    expect(match(Reserved::End), Compiler::Error::ExpectedEnd);
    
    // Update sz
    // rom is pointing at inst past 'end', we want to point at end
    auto offset = _rom8.size() - szIndex - 1;
    expect(offset < 256, Compiler::Error::JumpTooBig);
    
    _rom8[szIndex] = uint8_t(offset);
    
    return true;
}

bool
ArlyCompileEngine::ifStatement()
{
    if (!match(Reserved::If)) {
        return false;
    }
    expect(Token::NewLine);

    _rom8.push_back(uint8_t(Op::If));
    
    // Output a placeholder for sz and rember where it is
    auto szIndex = _rom8.size();
    _rom8.push_back(0);
    
    statements();
    
    // Update sz
    auto offset = _rom8.size() - szIndex - 1;
    expect(offset < 256, Compiler::Error::JumpTooBig);
    
    _rom8[szIndex] = uint8_t(offset);
    
    if (match(Reserved::Else)) {
        expect(Token::NewLine);

        _rom8.push_back(uint8_t(Op::Else));
    
        // Output a placeholder for sz and rember where it is
        auto szIndex = _rom8.size();
        _rom8.push_back(0);
        statements();

        // Update sz
        // rom is pointing at inst past 'end', we want to point at end
        auto offset = _rom8.size() - szIndex - 1;
        expect(offset < 256, Compiler::Error::JumpTooBig);
    
        _rom8[szIndex] = uint8_t(offset);
    }
    
    expect(match(Reserved::End), Compiler::Error::ExpectedEnd);

    // Finally output and EndIf. This lets us distinguish
    // Between an if and an if-else. If we skip an If we
    // will either see an Else of an EndIf instruction.
    // If we see anything else it's an error. If we see
    // an Else, it means this is the else clause of an
    // if statement we've skipped, so we execute its
    // statements. If we see an EndIf it means this If
    // doesn't have an Else.
    _rom8.push_back(uint8_t(Op::EndIf));

    return true;
}

void
ArlyCompileEngine::ignoreNewLines()
{
    while (1) {
        if (_scanner.getToken() != Token::NewLine) {
            break;
        }
        _scanner.retireToken();
    }
}

bool
ArlyCompileEngine::opcode()
{
    return opcode(_scanner.getToken(), _scanner.getTokenString());
}

bool
ArlyCompileEngine::opcode(Token token, const std::string str)
{
    Op op;
    OpParams par;
    return opcode(token, str, op, par);
}

bool
ArlyCompileEngine::opcode(Token token, const std::string str, Op& op, OpParams& par)
{
    if (token != Token::Identifier) {
        return false;
    }
    
    OpData data;
    if (!opDataFromString(str, data)) {
        return false;
    }
    op = data._op;
    par = data._par;
    return true;
}

bool
ArlyCompileEngine::isReserved(Token token, const std::string str, Reserved& r)
{
    static std::map<std::string, Reserved> reserved = {
        { "end",        Reserved::End },
        { "r0",         Reserved::R0 },
        { "r1",         Reserved::R1 },
        { "r2",         Reserved::R2 },
        { "r3",         Reserved::R3 },
        { "c0",         Reserved::C0 },
        { "c1",         Reserved::C1 },
        { "c2",         Reserved::C2 },
        { "c3",         Reserved::C3 },
    };
    
    if (CompileEngine::isReserved(token, str, r)) {
        return true;
    }

    if (token != Token::Identifier) {
        return false;
    }
    
    // For Arly we want to also check opcodes
    if (opcode()) {
        return true;
    }
    
    auto it = reserved.find(str);
    if (it != reserved.end()) {
        r = it->second;
        return true;
    }
    return false;
}
