/*-------------------------------------------------------------------------
    This source file is a part of Clover
    For the latest info, see https://github.com/cmarrin/Clover
    Copyright (c) 2021-2022, Chris Marrin
    All rights reserved.
    Use of this source code is governed by the MIT license that can be
    found in the LICENSE file.
-------------------------------------------------------------------------*/

#include "CloverCompileEngine.h"

#include <map>
#include <vector>

using namespace clvr;

bool
CloverCompileEngine::opInfo(Token token, OpInfo& op) const
{
    static std::vector<OpInfo> opInfo = {
        { Token::Equal,     1, Op::Pop,     Op::Pop,      OpInfo::Assign::Only , Type::None },
        { Token::AddSto,    1, Op::AddInt,  Op::AddFloat, OpInfo::Assign::Op   , Type::None },
        { Token::SubSto,    1, Op::SubInt,  Op::SubFloat, OpInfo::Assign::Op   , Type::None },
        { Token::MulSto,    1, Op::MulInt,  Op::MulFloat, OpInfo::Assign::Op   , Type::None },
        { Token::DivSto,    1, Op::DivInt,  Op::DivFloat, OpInfo::Assign::Op   , Type::None },
        { Token::AndSto,    1, Op::And,     Op::None,     OpInfo::Assign::Op   , Type::Int },
        { Token::OrSto,     1, Op::Or,      Op::None,     OpInfo::Assign::Op   , Type::Int },
        { Token::XorSto,    1, Op::Xor,     Op::None,     OpInfo::Assign::Op   , Type::Int },
        { Token::LOr,       6, Op::LOr,     Op::None,     OpInfo::Assign::None , Type::Int },
        { Token::LAnd,      7, Op::LAnd,    Op::None,     OpInfo::Assign::None , Type::Int },
        { Token::Or,        8, Op::Or,      Op::None,     OpInfo::Assign::None , Type::Int },
        { Token::Xor,       9, Op::Xor,     Op::None,     OpInfo::Assign::None , Type::Int },
        { Token::And,      10, Op::And,     Op::None,     OpInfo::Assign::None , Type::Int },
        { Token::EQ,       11, Op::EQInt,   Op::EQFloat,  OpInfo::Assign::None , Type::Int },
        { Token::NE,       11, Op::NEInt,   Op::NEFloat,  OpInfo::Assign::None , Type::Int },
        { Token::LT,       12, Op::LTInt,   Op::LTFloat,  OpInfo::Assign::None , Type::Int },
        { Token::GT,       12, Op::GTInt,   Op::GTFloat,  OpInfo::Assign::None , Type::Int },
        { Token::GE,       12, Op::GEInt,   Op::GEFloat,  OpInfo::Assign::None , Type::Int },
        { Token::LE,       12, Op::LEInt,   Op::LEFloat,  OpInfo::Assign::None , Type::Int },
        { Token::Plus,     14, Op::AddInt,  Op::AddFloat, OpInfo::Assign::None , Type::None },
        { Token::Minus,    14, Op::SubInt,  Op::SubFloat, OpInfo::Assign::None , Type::None },
        { Token::Mul,      15, Op::MulInt,  Op::MulFloat, OpInfo::Assign::None , Type::None },
        { Token::Div,      15, Op::DivInt,  Op::DivFloat, OpInfo::Assign::None , Type::None },
    };

    auto it = find_if(opInfo.begin(), opInfo.end(),
                    [token](const OpInfo& op) { return op.token() == token; });
    if (it != opInfo.end()) {
        op = *it;
        return true;
    }
    return false;
}

bool
CloverCompileEngine::program()
{
    _scanner.setIgnoreNewlines(true);
    
    try {
        while(element()) { }
        expect(Token::EndOfFile);
    }
    catch(...) {
        return false;
    }
    
    return _error == Compiler::Error::None;
}

bool
CloverCompileEngine::element()
{
    if (def()) {
        expect(Token::Semicolon);
        return true;
    }
    
    if (constant()) {
        expect(Token::Semicolon);
        return true;
    }
    
    if (var()) return true;    
    if (table()) return true;
    if (strucT()) return true;
    if (function()) return true;
    
    if (command()) {
        expect(Token::Semicolon);
        return true;
    }
    return false;
}

bool
CloverCompileEngine::table()
{
    if (!match(Reserved::Table)) {
        return false;
    }
    
    Type t;
    std::string id;
    expect(type(t), Compiler::Error::ExpectedType);
    expect(identifier(id), Compiler::Error::ExpectedIdentifier);
    expect(Token::OpenBrace);
    
    // Set the start address of the table. tableEntries() will fill them in
    _globals.emplace_back(id, _rom32.size(), t, Symbol::Storage::Const);
    
    values(t);
    expect(Token::CloseBrace);
    return true;
}

bool
CloverCompileEngine::strucT()
{
    if (!match(Reserved::Struct)) {
        return false;
    }
    
    std::string id;
    expect(identifier(id), Compiler::Error::ExpectedIdentifier);

    // Add a struct entry
    _structs.emplace_back(id);
    
    expect(Token::OpenBrace);
    
    while(structEntry()) { }
    
    expect(Token::CloseBrace);
    return true;
}

bool
CloverCompileEngine::var()
{
    if (!match(Reserved::Var)) {
        return false;
    }

    Type t;
    std::string id;
    
    expect(type(t), Compiler::Error::ExpectedType);
    
    bool isPointer = false;
    if (match(Token::Mul)) {
        isPointer = true;
    }
    
    expect(identifier(id), Compiler::Error::ExpectedIdentifier);
    
    int32_t size;
    if (!integerValue(size)) {
        size = 1;
    }
    
    size *= elementSize(t);

    expect(Token::Semicolon);

    // Put the var in _globals unless we're in a function, then put it in _locals
    if (inFunction) {
        currentLocals().emplace_back(id, currentLocals().size(), t, Symbol::Storage::Local, isPointer);
    } else {
        _globals.emplace_back(id, _nextMem, t, Symbol::Storage::Global, isPointer);

        // There is only enough room for GlobalSize values
        expect(_nextMem + size <= GlobalSize, Compiler::Error::TooManyVars);
        _globalSize = _nextMem + size;
    }
    
    _nextMem += size;
    return true;
}

bool
CloverCompileEngine::type(Type& t)
{
    if (CompileEngine::type(t)) {
        return true;
    }
    
    // See if it's a struct
    std::string id;
    if (!identifier(id, false)) {
        return false;
    }
    
    auto it = find_if(_structs.begin(), _structs.end(),
                    [id](const Struct s) { return s.name() == id; });
    if (it != _structs.end()) {
        // Types from 0x80 - 0xff are structs. Make the enum the struct
        // index + 0x80
        t = Type(0x80 + (it - _structs.begin()));
        _scanner.retireToken();
        return true;
    }
    return false;
}

bool
CloverCompileEngine::function()
{
    if (!match(Reserved::Function)) {
        return false;
    }
    
    _nextMem = 0;
    
    // Type is optional
    Type t = Type::None;
    type(t);
    
    std::string id;
    expect(identifier(id), Compiler::Error::ExpectedIdentifier);

    // Remember the function
    _functions.emplace_back(id, uint16_t(_rom8.size()), t);
    inFunction = true;
    
    expect(Token::OpenParen);
    
    expect(formalParameterList(), Compiler::Error::ExpectedFormalParams);
    
    // Remember how many formal params we have
    currentFunction().args() = currentFunction().locals().size();
    
    expect(Token::CloseParen);
    expect(Token::OpenBrace);

    // Remember the rom addr so we can check to see if we've emitted any code
    uint16_t size = romSize();
    
    while(var()) { }
    
    // SetFrame has to be the first instruction in the Function. Pass Params and Locals
    addOpPL(Op::SetFrame, currentFunction().args(), currentFunction().locals().size() - currentFunction().args());
    
    while(statement()) { }

    expect(Token::CloseBrace);

    // Set the high water mark
    if (_nextMem > _localHighWaterMark) {
        _localHighWaterMark = _nextMem;
    }
    
    // Emit Return at the end if there's not already one
    if (size == romSize() || lastOp() != Op::Return) {
        addOpSingleByteIndex(Op::PushIntConstS, 0);
        addOp(Op::Return);
    }
    
    inFunction = false;
    return true;
}

bool
CloverCompileEngine::structEntry()
{
    Type t;
    std::string id;
    
    if (!type(t)) {
        return false;
    }
    
    expect(identifier(id), Compiler::Error::ExpectedIdentifier);
    
    expect(Token::Semicolon);
    
    _structs.back().addEntry(id, t);
    return true;
}

bool
CloverCompileEngine::statement()
{
    if (compoundStatement()) return true;
    if (ifStatement()) return true;
    if (forStatement()) return true;
    if (whileStatement()) return true;
    if (loopStatement()) return true;
    if (returnStatement()) return true;
    if (jumpStatement()) return true;
    if (logStatement()) return true;
    if (expressionStatement()) return true;
    return false;
}

bool
CloverCompileEngine::compoundStatement()
{
    if (!match(Token::OpenBrace)) {
        return false;
    }

    while(statement()) { }

    expect(Token::CloseBrace);
    return true;
}

bool
CloverCompileEngine::ifStatement()
{
    if (!match(Reserved::If)) {
        return false;
    }
    
    expect(Token::OpenParen);
    
    arithmeticExpression();
    expect(bakeExpr(ExprAction::Right) == Type::Int, Compiler::Error::WrongType);
    expect(Token::CloseParen);

    // At this point the expresssion has been executed and the result is on TOS
    
    // Emit the if test
    addOp(Op::If);

    // Output a placeholder for sz and rember where it is
    auto szIndex = _rom8.size();
    addInt(0);
    
    statement();

    // Update sz
    auto offset = _rom8.size() - szIndex - 1;
    expect(offset < 256, Compiler::Error::JumpTooBig);
    _rom8[szIndex] = uint8_t(offset);
    
    if (match(Reserved::Else)) {
        addOp(Op::Else);

        // Output a placeholder for sz and rember where it is
        auto szIndex = _rom8.size();
        addInt(0);

        statement();

        // Update sz
        // rom is pointing at inst past 'end', we want to point at end
        auto offset = _rom8.size() - szIndex - 1;
        expect(offset < 256, Compiler::Error::JumpTooBig);
        _rom8[szIndex] = uint8_t(offset);
    }
    
    // Finally output and EndIf. This lets us distinguish
    // Between an if and an if-else. If we skip an If we
    // will either see an Else of an EndIf instruction.
    // If we see anything else it's an error. If we see
    // an Else, it means this is the else clause of an
    // if statement we've skipped, so we execute its
    // statements. If we see an EndIf it means this If
    // doesn't have an Else.
    addOp(Op::EndIf);

    return true;
}

bool
CloverCompileEngine::forStatement()
{
    if (!match(Reserved::ForEach)) {
        return false;
    }

    enterJumpContext();
    
    expect(Token::OpenParen);

    std::string id;
    expect(identifier(id), Compiler::Error::ExpectedIdentifier);

    expect(Token::Colon);

    // Loop starts with an if test of id < expr
    uint16_t startAddr = _rom8.size();
    
    Symbol sym;
    expect(findSymbol(id, sym), Compiler::Error::UndefinedIdentifier);
    expect(sym.storage() == Symbol::Storage::Local || 
           sym.storage() == Symbol::Storage::Global, Compiler::Error::ExpectedVar);
    
    addOpId(Op::Push, sym.addr());
    
    arithmeticExpression();
    expect(bakeExpr(ExprAction::Right) == Type::Int, Compiler::Error::WrongType);
    
    // We do the inverse test and break if true
    addOp(Op::GEInt);
    addOpInt(Op::If, 2);
    
    addJumpEntry(JumpEntry::Type::Break);
    
    addOp(Op::EndIf);
    
    expect(Token::CloseParen);
    
    statement();

    // Add the increment of the iterator
    uint16_t loopAddr = _rom8.size();
    addOpId(Op::PushRef, sym.addr());
    addOp(Op::PreIncInt);
    addOp(Op::Drop);
    
    // Loop back to the beginning
    addOp(Op::Loop);

    uint16_t offset = uint16_t(_rom8.size()) - startAddr + 1;
    expect(offset < 256, Compiler::Error::JumpTooBig);
    addInt(uint8_t(offset));
    
    // Now resolve all the jumps
    exitJumpContext(loopAddr);

    return true;
}

bool
CloverCompileEngine::whileStatement()
{
    if (!match(Reserved::While)) {
        return false;
    }

    enterJumpContext();

    expect(Token::OpenParen);

    // Loop starts with an if test of expr
    uint16_t startAddr = _rom8.size();

    arithmeticExpression();
    expect(bakeExpr(ExprAction::Right) == Type::Int, Compiler::Error::WrongType);

    // Invert the result so we can break if the result is false
    addOp(Op::LNot);
    addOpInt(Op::If, 2);

    addJumpEntry(JumpEntry::Type::Break);

    addOp(Op::EndIf);
    
    expect(Token::CloseParen);
    
    statement();

    uint16_t loopAddr = _rom8.size();
    
    // Loop back to the beginning
    addOp(Op::Loop);

    uint16_t offset = uint16_t(_rom8.size()) - startAddr + 1;
    expect(offset < 256, Compiler::Error::JumpTooBig);
    addInt(uint8_t(offset));
    
    // Now resolve all the jumps
    exitJumpContext(loopAddr);

    return true;
}

bool
CloverCompileEngine::loopStatement()
{
    if (!match(Reserved::Loop)) {
        return false;
    }

    enterJumpContext();

    uint16_t startAddr = _rom8.size();

    statement();

    uint16_t loopAddr = _rom8.size();
    
    // Loop back to the beginning
    addOp(Op::Loop);

    uint16_t offset = uint16_t(_rom8.size()) - startAddr + 1;
    expect(offset < 256, Compiler::Error::JumpTooBig);
    addInt(uint8_t(offset));
    
    // Now resolve all the jumps
    exitJumpContext(loopAddr);

    return true;
}

bool
CloverCompileEngine::returnStatement()
{
    if (!match(Reserved::Return)) {
        return false;
    }
    
    if (arithmeticExpression()) {
        // Push the return value
        expect(bakeExpr(ExprAction::Right) == currentFunction().type(), Compiler::Error::MismatchedType);
    } else {
        // If the function return type not None, we need a return value
        expect(currentFunction().type() == Type::None, Compiler::Error::MismatchedType);
        
        // No return value, push a zero
        addOpSingleByteIndex(Op::PushIntConstS, 0);
    }
    
    addOp(Op::Return);
    expect(Token::Semicolon);
    return true;
}

bool
CloverCompileEngine::jumpStatement()
{
    JumpEntry::Type type;
    
    if (match(Reserved::Break)) {
        type = JumpEntry::Type::Break;
    } else if (match(Reserved::Continue)) {
        type = JumpEntry::Type::Continue;
    } else {
        return false;
    }
    
    // Make sure we're in a loop
    expect(_jumpList.empty(), Compiler::Error::OnlyAllowedInLoop);
    
    addJumpEntry(type);

    expect(Token::Semicolon);
    return true;
}

bool
CloverCompileEngine::logStatement()
{
    if (!match(Reserved::Log)) {
        return false;
    }
    
    expect(Token::OpenParen);
    
    std::string str;
    expect(stringValue(str), Compiler::Error::ExpectedString);
    expect(str.length() < 256, Compiler::Error::StringTooLong);
    
    int i = 0;
    while (match(Token::Comma)) {
        expect(arithmeticExpression(), Compiler::Error::ExpectedExpr);        
        expect(++i < 16, Compiler::Error::TooManyVars);
        
        Type type = bakeExpr(ExprAction::Right);
        expect(type == Type::Float || type == Type::Int, Compiler::Error::WrongType);
    }

    // Emit op. Lower 4 bits are num args. String length is next byte,
    // followed by string
    addOpSingleByteIndex(Op::Log, i);
    addInt(str.length());
    for (auto it : str) {
        addInt(uint8_t(it));
    }

    expect(Token::CloseParen);
    expect(Token::Semicolon);

    return true;
}

bool
CloverCompileEngine::expressionStatement()
{
    if (!assignmentExpression()) {
        return false;
    }
    
    // The exprStack may or may not have one entry.
    // If it does it means that there was an unused
    // value from the expression, for instance, a
    // return value from a function. If not it means
    // the expression ended in an assignment. Do 
    // what's needed.
    if (!_exprStack.empty()) {
        expect(_exprStack.size() == 1, Compiler::Error::InternalError);
        _exprStack.pop_back();
        addOp(Op::Drop);
    }
    
    expect(Token::Semicolon);
    return true;
}

bool
CloverCompileEngine::arithmeticExpression(uint8_t minPrec, ArithType arithType)
{
    if (!unaryExpression()) {
        return false;
    }
    
    while(1) {
        OpInfo info;
        if (!opInfo(_scanner.getToken(), info) || info.prec() < minPrec) {
            return true;
        }
        
        // If this is an assignment operator, we only allow one so handle it separately
        uint8_t nextMinPrec = info.prec() + 1;
        _scanner.retireToken();
                
        expect(!(arithType != ArithType::Assign && info.assign() != OpInfo::Assign::None), Compiler::Error::AssignmentNotAllowedHere);
        
        Type leftType = Type::None;
        Type rightType = Type::None;
        
        if (info.assign() != OpInfo::Assign::None) {
            // Turn TOS into Ref
            leftType = bakeExpr(ExprAction::LeftRef);
        } else {
            leftType = bakeExpr(ExprAction::Right);
        }
        
        expect(arithmeticExpression(nextMinPrec), Compiler::Error::ExpectedExpr);

        switch(info.assign()) {
            case OpInfo::Assign::Only: {
                // Bake RHS
                rightType = bakeExpr(ExprAction::Right);
                break;
            }
            case OpInfo::Assign::Op:
                // The ref is on TOS, dup and get the value, then bake the right, then do the op
                addOp(Op::Dup);
                addOp(Op::PushDeref);
                rightType = bakeExpr(ExprAction::Right);
                expect(leftType == rightType, Compiler::Error::MismatchedType);
                addOp((leftType == Type::Int) ? info.intOp() : info.floatOp());
                break;
            case OpInfo::Assign::None: {
                rightType = bakeExpr(ExprAction::Right);
                expect(leftType == rightType, Compiler::Error::MismatchedType);
                
                Op op = (leftType == Type::Int) ? info.intOp() : info.floatOp();
                expect(op != Op::None, Compiler::Error::WrongType);
                addOp(op);

                if (info.resultType() != Type::None) {
                    leftType = info.resultType();
                }
                _exprStack.push_back(ExprEntry::Value(leftType));
                break;
            }
            default: break;
        }
        
        if (info.assign() != OpInfo::Assign::None) {
            expect(bakeExpr(ExprAction::Left) == rightType, Compiler::Error::MismatchedType);
        }
    }
    
    return true;
}

bool
CloverCompileEngine::unaryExpression()
{
    if (postfixExpression()) {
        return true;
    }

    Token token;
    
    if (match(Token::Minus)) {
        token = Token::Minus;
    } else if (match(Token::Twiddle)) {
        token = Token::Twiddle;
    } else if (match(Token::Bang)) {
        token = Token::Bang;
    } else if (match(Token::Inc)) {
        token = Token::Inc;
    } else if (match(Token::Dec)) {
        token = Token::Dec;
    } else if (match(Token::And)) {
        token = Token::And;
    } else {
        return false;
    }
    
    expect(unaryExpression(), Compiler::Error::ExpectedExpr);
    
    // If this is ampersand, make it into a pointer, otherwise bake it into a value
    Type type;

    switch(token) {
        default:
            break;
        case Token::And:
            bakeExpr(ExprAction::Ptr);
            break;
        case Token::Inc:
        case Token::Dec:
            type = bakeExpr(ExprAction::Ref);
            _exprStack.pop_back();
            _exprStack.push_back(ExprEntry::Value(type));

            if (type == Type::Float) {
                addOp((token == Token::Inc) ? Op::PreIncFloat : Op::PreDecFloat);
            } else {
                expect(type == Type::Int, Compiler::Error::MismatchedType);
                addOp((token == Token::Inc) ? Op::PreIncInt : Op::PreDecInt);
            }
            break;
        case Token::Minus:
        case Token::Twiddle:
        case Token::Bang:
            type = bakeExpr(ExprAction::Right);
            _exprStack.push_back(ExprEntry::Value(type));

            if (token == Token::Minus) {
                if (type == Type::Float) {
                    addOp(Op::NegFloat);
                } else {
                    expect(type == Type::Int, Compiler::Error::MismatchedType);
                    addOp(Op::NegInt);
                }
            } else {
                expect(type == Type::Int, Compiler::Error::WrongType);
                addOp((token == Token::Twiddle) ? Op::Not : Op::LNot);
            }
            break;
    }
    
    return true;
}

bool
CloverCompileEngine::postfixExpression()
{
    if (!primaryExpression()) {
        return false;
    }
    
    while (true) {
        if (match(Token::OpenParen)) {
            // Top of exprStack must be a function id
            Function fun;
            expect(findFunction(_exprStack.back(), fun), Compiler::Error::ExpectedFunction);
            expect(argumentList(fun), Compiler::Error::ExpectedArgList);
            expect(Token::CloseParen);
            
            // Replace the top of the exprStack with the function return value
            _exprStack.pop_back();
            
            _exprStack.push_back(ExprEntry::Value(fun.type()));
            
            if (fun.isNative()) {
                addOpId(Op::CallNative, uint8_t(fun.nativeId()));
            } else { 
                addOpTarg(Op::Call, fun.addr());
            }
        } else if (match(Token::OpenBracket)) {
            bakeExpr(ExprAction::Ref);
            expect(arithmeticExpression(), Compiler::Error::ExpectedExpr);
            expect(Token::CloseBracket);
            
            // Bake the contents of the brackets, leaving the result on TOS
            expect(bakeExpr(ExprAction::Right) == Type::Int, Compiler::Error::WrongType);

            // TOS now has the index.
            // Index the Ref
            bakeExpr(ExprAction::Index);
        } else if (match(Token::Dot)) {
            std::string id;
            expect(identifier(id), Compiler::Error::ExpectedIdentifier);
            bakeExpr(ExprAction::Ref);
            
            _exprStack.emplace_back(id);
            bakeExpr(ExprAction::Offset);
            return true;
        } else if (match(Token::Inc)) {
            Type type = bakeExpr(ExprAction::Ref);
            _exprStack.pop_back();
            _exprStack.push_back(ExprEntry::Value(type));

            if (type == Type::Float) {
                addOp(Op::PostIncFloat);
            } else {
                expect(type == Type::Int, Compiler::Error::MismatchedType);
                addOp(Op::PostIncInt);
            }
        } else if (match(Token::Dec)) {
            Type type = bakeExpr(ExprAction::Ref);
            _exprStack.pop_back();
            _exprStack.push_back(ExprEntry::Value(type));

            if (type == Type::Float) {
                addOp(Op::PostDecFloat);
            } else {
                expect(type == Type::Int, Compiler::Error::MismatchedType);
                addOp(Op::PostDecInt);
            }
        } else {
            return true;
        }
    }
}

bool
CloverCompileEngine::primaryExpression()
{
    if (match(Token::OpenParen)) {
        expect(arithmeticExpression(), Compiler::Error::ExpectedExpr);
        expect(Token::CloseParen);
        return true;
    }
    
    std::string id;
    if (identifier(id)) {
        _exprStack.emplace_back(id);
        return true;
    }
        
    float f;
    if (floatValue(f)) {
        _exprStack.emplace_back(f);
        return true;
    }
        
    int32_t i;
    if (integerValue(i)) {
        _exprStack.emplace_back(i);
        return true;
    }
    return false;
}

bool
CloverCompileEngine::formalParameterList()
{
    Type t;
    while (true) {
        if (!type(t)) {
            return true;
        }
        
        std::string id;
        expect(identifier(id), Compiler::Error::ExpectedIdentifier);
        currentLocals().emplace_back(id, currentLocals().size(), t, Symbol::Storage::Local);
        
        if (!match(Token::Comma)) {
            return true;
        }
    }
    
    return true;
}

bool
CloverCompileEngine::argumentList(const Function& fun)
{
    int i = 0;
    while (true) {
        if (!arithmeticExpression()) {
            if (i == 0) {
                break;
            }
            expect(false, Compiler::Error::ExpectedExpr);
        }
        
        i++;
        
        expect(fun.args() >= i, Compiler::Error::WrongNumberOfArgs);
    
        // Bake the arithmeticExpression, leaving the result in r0.
        // Make sure the type matches the formal argument and push r0
        expect(bakeExpr(ExprAction::Right) == fun.locals()[i - 1].type(), Compiler::Error::MismatchedType);

        if (!match(Token::Comma)) {
            break;
        }
    }

    expect(fun.args() == i, Compiler::Error::WrongNumberOfArgs);
    return true;
}

bool
CloverCompileEngine::isReserved(Token token, const std::string str, Reserved& r)
{
    static std::map<std::string, Reserved> reserved = {
        { "struct",        Reserved::Struct },
        { "return",        Reserved::Return },
        { "break",         Reserved::Break },
        { "continue",      Reserved::Continue },
        { "log",           Reserved::Log },
        { "while",         Reserved::While },
        { "loop",          Reserved::Loop },
    };

    if (CompileEngine::isReserved(token, str, r)) {
        return true;
    }

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

uint8_t
CloverCompileEngine::findInt(int32_t i)
{
    // Try to find an existing int const. If found, return
    // its address. If not found, create one and return 
    // that address.
    auto it = find_if(_rom32.begin(), _rom32.end(),
                    [i](uint32_t v) { return uint32_t(i) == v; });
    if (it != _rom32.end()) {
        return it - _rom32.begin();
    }
    
    _rom32.push_back(uint32_t(i));
    return _rom32.size() - 1;
}

uint8_t
CloverCompileEngine::findFloat(float f)
{
    // Try to find an existing fp const. If found, return
    // its address. If not found, create one and return 
    // that address.
    uint32_t i = floatToInt(f);
    auto it = find_if(_rom32.begin(), _rom32.end(),
                    [i](uint32_t v) { return i == v; });
    if (it != _rom32.end()) {
        return it - _rom32.begin();
    }
    
    _rom32.push_back(i);
    return _rom32.size() - 1;
}

CompileEngine::Type
CloverCompileEngine::bakeExpr(ExprAction action)
{
    Type type = Type::None;
    ExprEntry entry = _exprStack.back();   
    Symbol sym;
     
    switch(action) {
        default: break;
        case ExprAction::Right:
            switch(entry.type()) {
                default:
                    expect(false, Compiler::Error::InternalError);
                case ExprEntry::Type::Int: {
                    uint32_t i = uint32_t(int32_t(entry));
                    if (i <= 15) {
                        addOpSingleByteIndex(Op::PushIntConstS, i);
                    } else if (i <= 255) {
                        addOpInt(Op::PushIntConst, i);
                    } else {
                        // Add an int const
                        addOpInt(Op::Push, findInt(i));
                    }
                    type = Type::Int;
                    break;
                }
                case ExprEntry::Type::Float:
                    // Use an fp constant
                    addOpInt(Op::Push, findFloat(entry));
                    type = Type::Float;
                    break;
                case ExprEntry::Type::Id:
                    // Push the value
                    if (findSymbol(entry, sym)) {
                        addOpId(Op::Push, sym.addr());
                        type = sym.isPointer() ? Type::Ptr : sym.type();
                    } else {
                        Def d;
                        expect(findDef(entry, d), Compiler::Error::UndefinedIdentifier);
                        addOpInt(Op::PushIntConst, d._value);
                        type = Type::Int;
                    }
                    break;
                case ExprEntry::Type::Ref: {
                    // If this a ptr then we want to leave the ref on TOS, not the value
                    const ExprEntry::Ref& ref = entry;
                    type = ref._type;
                    if (!ref._ptr) {
                        addOp(Op::PushDeref);
                    } else {
                        type = Type::Ptr;
                    }
                    break;
                }
                case ExprEntry::Type::Value: {
                    // Nothing to do, this just indicates that TOS is a value
                    const ExprEntry::Value& value = entry;
                    type = value._type;
                    break;
                }
            }
            break;
        case ExprAction::Left: {
            // The stack contains a ref      
            expect(entry.type() == ExprEntry::Type::Ref, Compiler::Error::InternalError);
            const ExprEntry::Ref& ref = entry;
            type = ref._ptr ? Type::Ptr : ref._type;
            addOp(Op::PopDeref);
            break;
        }
        case ExprAction::Index: {
            // TOS has an index, get the sym for the var so 
            // we know the size of each element
            if (entry.type() == ExprEntry::Type::Ref) {
                // This is a ref, get the size from the type
                const ExprEntry::Ref& ref = entry;
                type = ref._type;
            } else {
                expect(entry.type() == ExprEntry::Type::Id, Compiler::Error::ExpectedIdentifier);
                expect(findSymbol(entry, sym), Compiler::Error::UndefinedIdentifier);
                expect(sym.storage() == Symbol::Storage::Local || 
                       sym.storage() == Symbol::Storage::Global, Compiler::Error::ExpectedVar);

                type = sym.type();
                _exprStack.pop_back();
                _exprStack.push_back(ExprEntry::Ref(type));
            }
            
            Struct s;
            addOpSingleByteIndex(Op::Index, structFromType(type, s) ? s.size() : 1);
            return type;
        }
        case ExprAction::Offset: {
            // Prev entry has a Ref. Get the type so we can get an element index
            // we know the size of each element
            expect(_exprStack.size() >= 2, Compiler::Error::InternalError);
            const ExprEntry& prevEntry = _exprStack.end()[-2];            
            expect(prevEntry.type() == ExprEntry::Type::Ref, Compiler::Error::InternalError);
            const ExprEntry::Ref& ref = prevEntry;
            
            uint8_t index;
            Type elementType;
            findStructElement(ref._type, entry, index, elementType);
            _exprStack.pop_back();
            _exprStack.pop_back();
            _exprStack.push_back(ExprEntry::Ref(elementType));
            addOpSingleByteIndex(Op::Offset, index);
            return elementType;
        }
        case ExprAction::Ref:
        case ExprAction::LeftRef:
        case ExprAction::Ptr:
            if (entry.type() == ExprEntry::Type::Ref) {
                // Already have a ref
                const ExprEntry::Ref& ref = entry;
                type = ref._type;
                
                // If this is a Ptr action, we want to say that we want the stack to have 
                // a reference to a value rather than the value. So add the _ptr value
                // to the Ref
                if (action == ExprAction::Ptr) {
                    _exprStack.pop_back();
                    _exprStack.push_back(ExprEntry::Ref(type, true));
                }
                return type;
            }
            
            expect(entry.type() == ExprEntry::Type::Id, Compiler::Error::ExpectedIdentifier);

            // Turn this into a Ref
            expect(findSymbol(entry, sym), Compiler::Error::UndefinedIdentifier);
            _exprStack.pop_back();
            _exprStack.push_back(ExprEntry::Ref(sym.type(), action == ExprAction::Ptr || sym.isPointer()));
            
            // If this is a pointer, just push it, otherwise push the ref
            // But if this is a LeftRef action we are just going to store
            // a value here, so still do the PushRef
            addOpId((sym.isPointer() && action == ExprAction::Ref) ? Op::Push : Op::PushRef, sym.addr());
            return sym.isPointer() ? Type::Ptr : sym.type();
    }
    
    _exprStack.pop_back();
    return type;
}
        
bool
CloverCompileEngine::isExprFunction()
{
    expect(!_exprStack.empty(), Compiler::Error::InternalError);
    
    Function fun;
    return findFunction(_exprStack.back(), fun);
}

bool
CloverCompileEngine::structFromType(Type type, Struct& s)
{
    if (uint8_t(type) < 0x80) {
        return false;
    }
    uint8_t index = uint8_t(type) - 0x80;
    expect(index < _structs.size(), Compiler::Error::InternalError);
    
    s = _structs[index];
    return true;
}

void
CloverCompileEngine::findStructElement(Type type, const std::string& id, uint8_t& index, Type& elementType)
{
    Struct s;
    expect(structFromType(type, s), Compiler::Error::ExpectedStructType);
    
    const std::vector<ParamEntry>& entries = s.entries();

    auto it = find_if(entries.begin(), entries.end(),
                    [id](const ParamEntry& ent) { return ent._name == id; });
    expect(it != entries.end(), Compiler::Error::InvalidStructId);
    
    // FIXME: For now assume structs can only have 1 word entries. If we ever support Structs with Structs this is not true
    index = it - entries.begin();
    elementType = it->_type;
}

uint8_t
CloverCompileEngine::elementSize(Type type)
{
    if (uint8_t(type) < 0x80) {
        return 1;
    }
    
    uint8_t structIndex = uint8_t(type) - 0x80;
    expect(structIndex < _structs.size(), Compiler::Error::InternalError);
    return _structs[structIndex].size();
}

void
CloverCompileEngine::exitJumpContext(uint16_t loopAddr)
{
    expect(!_jumpList.empty(), Compiler::Error::InternalError);

    // Go through all the entries in the last _jumpList entry and fill in
    // the addresses. The current address (_rom8.size()) is used if this
    // is a break and the passed loopAddr is used if this is a continue
    uint16_t breakAddr = _rom8.size();
    for (const auto& it : _jumpList.back()) {
        expect(it._addr < breakAddr, Compiler::Error::InternalError);
        
        uint16_t offset = ((it._type == JumpEntry::Type::Break) ? breakAddr : loopAddr) - it._addr - 1;
        expect(offset < 256, Compiler::Error::JumpTooBig);
        expect(_rom8[it._addr] == 0, Compiler::Error::InternalError);
        _rom8[it._addr] = offset;
    }
    
    _jumpList.pop_back();
}

void
CloverCompileEngine::addJumpEntry(JumpEntry::Type type)
{
    expect(!_jumpList.empty(), Compiler::Error::InternalError);
    
    addOp(Op::Jump);
    uint16_t addr = _rom8.size();
    addInt(0);
    _jumpList.back().emplace_back(type, addr);
}
