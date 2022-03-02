/*-------------------------------------------------------------------------
    This source file is a part of Clover
    For the latest info, see https://github.com/cmarrin/Clover
    Copyright (c) 2021-2022, Chris Marrin
    All rights reserved.
    Use of this source code is governed by the MIT license that can be
    found in the LICENSE file.
-------------------------------------------------------------------------*/

#include "Compiler.h"

#include "ArlyCompileEngine.h"
#include "CloverCompileEngine.h"
#include "NativeCore.h"

#include <map>
#include <vector>

using namespace clvr;

bool Compiler::compile(std::istream* istream, Language lang, std::vector<uint8_t>& executable,
                       const std::vector<NativeModule*>& modules,
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
    
    // Install the modules in the engine
    // First add the core
    NativeCore().addFunctions(engine);
    
    for (const auto& it : modules) {
        it->addFunctions(engine);
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
