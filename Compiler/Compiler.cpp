//
//  Compiler.cpp
//  CompileArly
//
//  Created by Chris Marrin on 1/9/22.
//

#include "Compiler.h"

#include "ArlyCompileEngine.h"
#include "CloverCompileEngine.h"

#include <map>
#include <vector>

using namespace arly;

bool Compiler::compile(std::istream* istream, Language lang, std::vector<uint8_t>& executable,
                       std::vector<std::pair<int32_t, std::string>>* annotations)
{
    CompileEngine* engine = nullptr;
    
    switch(lang) {
        case Language::Arly:
            engine = new ArlyCompileEngine(istream);
            break;
        case Language::Clover:
            engine = new CloverCompileEngine(istream, annotations);
            break;
    }
    
    if (!engine) {
        _error = Error::UnrecognizedLanguage;
        return false;
    }
    
    engine->program();
    _error = engine->error();
    _expectedToken = engine->expectedToken();
    _expectedString = engine->expectedString();
    _lineno = engine->lineno();
    _charno = engine->charno();
    
    try {
        if (_error == Error::None) {
            engine->emit(executable);
        }
    }
    catch(...) {
    }
    
    delete engine;
    
    return _error == Error::None;
}
