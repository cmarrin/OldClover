/*-------------------------------------------------------------------------
    This source file is a part of Clover
    For the latest info, see https://github.com/cmarrin/Clover
    Copyright (c) 2021-2022, Chris Marrin
    All rights reserved.
    Use of this source code is governed by the MIT license that can be
    found in the LICENSE file.
-------------------------------------------------------------------------*/

// Clover compiler
//
// A simple imperative language which generates code that can be 
// executed by the Interpreter
//

#pragma once

#include "Compiler.h"
#include "CompileEngine.h"
#include "Opcodes.h"
#include "Scanner.h"
#include <cstdint>
#include <istream>
#include <variant>

namespace clvr {

//////////////////////////////////////////////////////////////////////////////
//
//  Class: CompileEngine
//
//////////////////////////////////////////////////////////////////////////////

/*

BNF:

program:
    { element } ;

element:
    def | constant | table | struct | var | function | command ;
    
def:
    'def' <id> <integer> ';'

constant:
    'const' type <id> value ';' ;
    
table:
    'table' type <id> '{' values '}' ;

struct:
    'struct' <id> '{' { structEntry } '}' ;
    
// First integer is num elements, second is size of each element
var:
    'var' type [ '*' ] <id> [ <integer> ] ';' ;

function:
    'function' [ <type> ] <id> '( formalParameterList ')' '{' { var } { statement } '}' ;

command:
    'command' <id> <integer> <id> <id> ';' ;

structEntry:
    type <id> ';' ;

// <id> is a struct name
type:
    'float' | 'int' | <id> 

value:
    ['-'] <float> | ['-'] <integer>

statement:
      compoundStatement
    | ifStatement
    | forStatement
    | whileStatement
    | loopStatement
    | returnStatement
    | jumpStatement
    | logStatement
    | expressionStatement
    ;
  
compoundStatement:
    '{' { statement } '}' ;

ifStatement:
    'if' '(' arithmeticExpression ')' statement ['else' statement ] ;

forStatement:
    'foreach' '(' identifier ':' arithmeticExpression ')' statement ;
    
whileStatement:
    'while' '(' arithmeticExpression ')' statement ;

loopStatement:
    'loop' statement ;

returnStatement:
      'return' [ arithmeticExpression ] ';' ;
      
jumpStatement:
      'break' ';'
    | 'continue' ';'
    ;

logStatement:
    'log' '(' <string> { ',' arithmeticExpression } ')' ';' ;

expressionStatement:
    arithmeticExpression ';' ;
    
arithmeticExpression:
      unaryExpression
    | unaryExpression operator arithmeticExpression

unaryExpression:
      postfixExpression
    | '-' unaryExpression
    | '~' unaryExpression
    | '!' unaryExpression
    | '&' unaryExpression
    ;

postfixExpression:
      primaryExpression
    | postfixExpression '(' argumentList ')'
    | postfixExpression '[' arithmeticExpression ']'
    | postfixExpression '.' identifier
    ;

primaryExpression:
      '(' arithmeticExpression ')'
    | <id>
    | <float>
    | <integer>
    ;
    
formalParameterList:
      (* empty *)
    | type identifier { ',' type identifier }
    ;

argumentList:
        (* empty *)
      | arithmeticExpression { ',' arithmeticExpression }
      ;

operator: (* operator   precedence   association *)
               '='     (*   1          Right    *)
    |          '+='    (*   1          Right    *)
    |          '-='    (*   1          Right    *)
    |          '*='    (*   1          Right    *)
    |          '/='    (*   1          Right    *)
    |          '&='    (*   1          Right    *)
    |          '|='    (*   1          Right    *)
    |          '^='    (*   1          Right    *)
    |          '||'    (*   2          Left     *)
    |          '&&'    (*   3          Left     *)
    |          '|'     (*   4          Left     *)
    |          '^'     (*   5          Left     *)
    |          '&'     (*   6          Left     *)
    |          '=='    (*   7          Left     *)
    |          '!='    (*   7          Left     *)
    |          '<'     (*   8          Left     *)
    |          '>'     (*   8          Left     *)
    |          '>='    (*   8          Left     *)
    |          '<='    (*   8          Left     *)
    |          '+'     (*   10         Left     *)
    |          '-'     (*   10         Left     *)
    |          '*'     (*   11         Left     *)
    |          '/'     (*   11         Left     *)
    ;
    
*/

class CloverCompileEngine : public CompileEngine {
public:
  	CloverCompileEngine(std::istream* stream, std::vector<std::pair<int32_t, std::string>>* annotations)
        : CompileEngine(stream, annotations)
    { }
  	
    virtual bool program() override;

protected:
    virtual bool statement() override;
    virtual bool function() override;
    virtual bool table() override;
    virtual bool type(Type&) override;
  
    bool var();

private:
    class OpInfo {
    public:        
        OpInfo() { }
        
        enum class Assign { None, Only, Op };
        
        // assign says this is an assignmentOperator, opAssign says it also has a binary op
        OpInfo(Token token, uint8_t prec, Op intOp, Op floatOp, Assign assign, Type resultType)
            : _token(token)
            , _intOp(intOp)
            , _floatOp(floatOp)
            , _prec(prec)
            , _assign(assign)
            , _resultType(resultType)
        {
        }
        
        bool operator==(const Token& t)
        {
            return static_cast<Token>(_token) == t;
        }
        
        Token token() const { return _token; }
        uint8_t prec() const { return _prec; }
        Op intOp() const { return _intOp; }
        Op floatOp() const { return _floatOp; }
        Assign assign() const { return _assign; }
        Type resultType() const { return _resultType; }

    private:
        Token _token;
        Op _intOp;
        Op _floatOp;
        uint8_t _prec;
        Assign _assign;
        Type _resultType;
    };
    
    bool element();
    bool strucT();
    
    bool structEntry();

    bool compoundStatement();
    bool ifStatement();
    bool forStatement();
    bool whileStatement();
    bool loopStatement();
    bool returnStatement();
    bool jumpStatement();
    bool logStatement();
    bool expressionStatement();
    
    enum class ArithType { Assign, Op };
    bool assignmentExpression() { return arithmeticExpression(1, ArithType::Assign); }
    bool arithmeticExpression(uint8_t minPrec = 1, ArithType = ArithType::Op);
    bool unaryExpression();
    bool postfixExpression();
    bool primaryExpression();

    bool formalParameterList();
    bool argumentList(const Function& fun);
    
    bool opInfo(Token token, OpInfo&) const;

    virtual bool isReserved(Token token, const std::string str, Reserved&) override;

    uint8_t findInt(int32_t);
    uint8_t findFloat(float);
    
    // The ExprStack
    //
    // This is a stack of the operators being processed. Values can be:
    //
    //      Id      - string id
    //      Float   - float constant
    //      Int     - int32_t constant
    //      Dot     - r0 contains a ref. entry is an index into a Struct.

    // ExprAction indicates what to do with the top entry on the ExprStack during baking
    //
    //      Right       - Entry is a RHS, so it can be a float, int or id and the value 
    //                    is loaded into r0
    //      Left        - Entry is a LHS, so it must be an id, value in r0 is stored at the id
    //      Function    - Entry is the named function which has already been emitted so value
    //                    is the return value in r0
    //      Ref         - Value left in r0 must be an address to a value (Const or RAM)
    //      Deref       = Value must be a Struct entry for the value in _stackEntry - 1
    //      Dot         - Dot operator. TOS must be a struct id, TOS-1 must be a ref with
    //                    a type. Struct id must be a member of the type of the ref.
    //                    pop the two 
    //
    enum class ExprAction { Left, Right, Ref, LeftRef, Ptr, Index, Offset };
    Type bakeExpr(ExprAction);
    bool isExprFunction();
    uint8_t elementSize(Type);
    
    struct ParamEntry
    {
        ParamEntry(const std::string& name, Type type)
            : _name(name)
            , _type(type)
        { }
        std::string _name;
        Type _type;
    };
    
    class Struct
    {
    public:
        Struct() { }
        
        Struct(const std::string& name)
            : _name(name)
        { }
        
        void addEntry(const std::string& name, Type type)
        {
            _entries.emplace_back(name, type);
            
            // FIXME: For now assume all 1 word types. Will we support Structs in Structs?
            // FIXME: Max size is 16, check that
            _size++;
        }
        
        const std::vector<ParamEntry>& entries() const { return _entries; }
        
        const std::string& name() const { return _name; }
        uint8_t size() const { return _size; }
        
    private:
        std::string _name;
        std::vector<ParamEntry> _entries;
        uint8_t _size = 0;
    };

    class ExprEntry
    {
    public:
        struct Ref
        {
            Ref(Type type, bool ptr = false) : _type(type), _ptr(ptr) { }
            
            Type _type;
            bool _ptr;
        };
        
        struct Function
        {
            Function(const std::string& s) : _name(s) { }
            std::string _name;
        };
        
        struct Dot
        {
            Dot(uint8_t index) : _index(index) { }
            
            uint8_t _index;
        };
        
        struct Value
        {
            Value(Type type) : _type(type) { }
            
            Type _type;
        };
        
        enum class Type {
            None = 0,
            Id = 1,
            Float = 2, 
            Int = 3, 
            Ref = 4, 
            Function = 5, 
            Dot = 6, 
            Value = 7
        };
        
        ExprEntry() { _variant = std::monostate(); }
        ExprEntry(const std::string& s) { _variant = s; }
        ExprEntry(float f) { _variant = f; }
        ExprEntry(int32_t i) { _variant = i; }
        ExprEntry(const Ref& ref) { _variant = ref; }
        ExprEntry(const Function& fun) { _variant = fun; }
        ExprEntry(const Dot& dot) { _variant = dot; }
        ExprEntry(const Value& val) { _variant = val; }
                
        operator const std::string&() const { return std::get<std::string>(_variant); }
        operator float() const { return std::get<float>(_variant); }
        operator int32_t() const { return std::get<int32_t>(_variant); }
        operator const Ref&() const { return std::get<Ref>(_variant); }
        operator const Dot&() const { return std::get<Dot>(_variant); }
        operator const Value&() const { return std::get<Value>(_variant); }
        
        Type type() const { return Type(_variant.index()); }

    private:
        std::variant<std::monostate
                     , std::string
                     , float
                     , int32_t
                     , Ref
                     , Function
                     , Dot
                     , Value
                     > _variant;
    };
    
    bool structFromType(Type, Struct&);
    void findStructElement(Type, const std::string& id, uint8_t& index, Type&);

    struct JumpEntry
    {
        enum class Type { Break, Continue };
        
        JumpEntry(Type type, uint16_t addr) : _type(type), _addr(addr) { }
        
        Type _type;
        uint16_t _addr;
    };

    void enterJumpContext() { _jumpList.emplace_back(); }
    void exitJumpContext(uint16_t loopAddr);
    void addJumpEntry(JumpEntry::Type);
    
    std::vector<Struct> _structs;
    std::vector<ExprEntry> _exprStack;
    std::vector<Symbol> _builtins;
    
    // The jump list is an array of arrays of JumpEntries. The outermost array
    // is a stack of active looping statements (for, loop, etc.). Each of these
    // has an array of break or continue statements that need to be resolved
    // when the looping statement ends.
    std::vector<std::vector<JumpEntry>> _jumpList;
};

}
