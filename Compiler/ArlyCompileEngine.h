/*-------------------------------------------------------------------------
    This source file is a part of Clover
    For the latest info, see https://github.com/cmarrin/Clover
    Copyright (c) 2021-2022, Chris Marrin
    All rights reserved.
    Use of this source code is governed by the MIT license that can be
    found in the LICENSE file.
-------------------------------------------------------------------------*/

// Arly compiler internals

/*

Arly Source Format

program         ::= defs constants tables vars functions commands
defs            ::= { def <n> }
def             ::= 'def' <id> <integer>
constants       ::= { constant <n> }
constant        ::= 'const' type <id> value
tables          ::= { table <n> }
table           ::= 'table' type <id> <n> tableEntries 'end'
tableEntries    ::= { values <n> }
functions       ::= {function <n>
function        ::= 'function' <id> <n> statements 'end'
commands        ::= { command <n> }
command         ::= 'command' <id> <integer> <n> init loop 'end'
vars            ::= { var <n> }
var             ::= type <id> <integer>
init            ::= 'init' <n> statements 'end' <n>
loop            ::= 'loop' <n> statements 'end' <n>

statements      ::= { statement <n> }
statement       ::= opStatement | forStatement | ifStatement
opStatement     ::= op opParams
forStatement    ::= 'for' <id> <n> statements 'end'
ifStatement     ::= 'if' <n> statements { 'else' <n> statements } 'end'

type            ::= 'float' | 'int'
values          ::= { value }
value           ::= ['-'] <float> | ['-'] <integer>
opParams        ::= { value }
opParam         ::= <id> | <integer>

op              ::= <list of opcodes in opcodes.h>

*/

#pragma once

#include "CompileEngine.h"

namespace clvr {

class ArlyCompileEngine : public CompileEngine
{
public:
    ArlyCompileEngine(std::istream* stream)
        : CompileEngine(stream, nullptr)
    { }
    
    virtual bool program() override;
    
    bool opcode(Op op, std::string& str, OpParams& par);
    
protected:
    virtual bool statement() override;
    virtual bool function() override;
    virtual bool table() override;

private:
    void statements();
    void defs();
    void vars();
    void constants();
    void tables();
    void functions();
    void commands();

    void tableEntries(Type);

    bool var();
        
    uint8_t handleI();
    uint8_t handleId() { Symbol::Storage s; return handleId(s); }
    uint8_t handleId(Symbol::Storage&);
    uint8_t handleConst();
    void handleOpParams(uint8_t a);
    void handleOpParams(uint8_t a, uint8_t b);
    void handleOpParamsReverse(uint8_t a, uint8_t b);
    void handleOpParamsRdRs(Op op, uint8_t rd, uint8_t rs);
    void handleOpParamsRdRs(Op op, uint8_t id, uint8_t rd, uint8_t i, uint8_t rs);
    void handleOpParamsRdRsI(Op op, uint8_t rd, uint8_t rs, uint8_t i);
    void handleOpParamsRdIRs(Op op, uint8_t rd, uint8_t i, uint8_t rs);
    
    bool opStatement();
    bool forStatement();
    bool ifStatement();
    
    void ignoreNewLines();

    // This allows the specific reserved words for this compile engine
    virtual bool isReserved(Token token, const std::string str, Reserved& r) override;
    
    // These methods check to see if the next token is of the
    // appropriate type. Some versions just return true or
    // false, others also return details about the token
    bool opcode();
    bool opcode(Token token, const std::string str, Op& op, OpParams& par);
    bool opcode(Token token, const std::string str);
};

}
