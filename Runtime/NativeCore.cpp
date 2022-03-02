/*-------------------------------------------------------------------------
    This source file is a part of Clover
    For the latest info, see https://github.com/cmarrin/Clover
    Copyright (c) 2021-2022, Chris Marrin
    All rights reserved.
    Use of this source code is governed by the MIT license that can be
    found in the LICENSE file.
-------------------------------------------------------------------------*/

#include "NativeCore.h"

#include "Interpreter.h"

using namespace clvr;

#ifndef ARDUINO
#include "CompileEngine.h"

void
NativeCore::addFunctions(CompileEngine* comp)
{
    comp->addNative("Animate", uint8_t(Id::Animate), CompileEngine::Type::Int,
                            CompileEngine::SymbolList {
                                { "p", 0, CompileEngine::Type::Ptr },
                            });
    comp->addNative("Param", uint8_t(Id::Param), CompileEngine::Type::Int,
                            CompileEngine::SymbolList {
                                { "p", 0, CompileEngine::Type::Int },
                            });
    comp->addNative("Float", uint8_t(Id::Float), CompileEngine::Type::Float,
                            CompileEngine::SymbolList {
                                { "v", 0, CompileEngine::Type::Int },
                            });
    comp->addNative("Int", uint8_t(Id::Int), CompileEngine::Type::Int,
                            CompileEngine::SymbolList {
                                { "v", 0, CompileEngine::Type::Float },
                            });
    comp->addNative("RandomInt", uint8_t(Id::RandomInt), CompileEngine::Type::Int,
                            CompileEngine::SymbolList {
                                { "min", 0, CompileEngine::Type::Int },
                                { "max", 0, CompileEngine::Type::Int },
                            });
    comp->addNative("RandomFloat", uint8_t(Id::RandomFloat), CompileEngine::Type::Float,
                            CompileEngine::SymbolList {
                                { "min", 0, CompileEngine::Type::Float },
                                { "max", 0, CompileEngine::Type::Float },
                            });
    comp->addNative("InitArray", uint8_t(Id::InitArray), CompileEngine::Type::None,
                            CompileEngine::SymbolList {
                                { "dst", 0, CompileEngine::Type::Ptr },
                                { "v",   0, CompileEngine::Type::Int },
                                { "n",   0, CompileEngine::Type::Int },
                            });
    comp->addNative("MinInt", uint8_t(Id::MinInt), CompileEngine::Type::Int,
                            CompileEngine::SymbolList {
                                { "a",   0, CompileEngine::Type::Int },
                                { "b",   0, CompileEngine::Type::Int },
                            });
    comp->addNative("MinFloat", uint8_t(Id::MinFloat), CompileEngine::Type::Float,
                            CompileEngine::SymbolList {
                                { "a",   0, CompileEngine::Type::Float },
                                { "b",   0, CompileEngine::Type::Float },
                            });
    comp->addNative("MaxInt", uint8_t(Id::MaxInt), CompileEngine::Type::Int,
                            CompileEngine::SymbolList {
                                { "a",   0, CompileEngine::Type::Int },
                                { "b",   0, CompileEngine::Type::Int },
                            });
    comp->addNative("MaxFloat", uint8_t(Id::MaxFloat), CompileEngine::Type::Float,
                            CompileEngine::SymbolList {
                                { "a",   0, CompileEngine::Type::Float },
                                { "b",   0, CompileEngine::Type::Float },
                            });
}
#endif

bool
NativeCore::hasId(uint8_t id) const 
{
    switch(Id(id)) {
        default:
            return false;
        case Id::Animate      :
        case Id::Param        :
        case Id::Float        :
        case Id::Int          :
        case Id::RandomInt    :
        case Id::RandomFloat  :
        case Id::InitArray    :
        case Id::MinInt       :
        case Id::MinFloat     :
        case Id::MaxInt       :
        case Id::MaxFloat     :
            return true;
    }
}

uint8_t
NativeCore::numParams(uint8_t id) const
{
    switch(Id(id)) {
        default                 : return 0;
        case Id::Animate        : return 1;
        case Id::Param          : return 1;
        case Id::Float          : return 1;
        case Id::Int            : return 1;
        case Id::RandomInt      : return 2;
        case Id::RandomFloat    : return 2;
        case Id::InitArray      : return 3;
        case Id::MinInt         : return 2;
        case Id::MinFloat       : return 2;
        case Id::MaxInt         : return 2;
        case Id::MaxFloat       : return 2;
    }
}

int32_t
NativeCore::call(Interpreter* interp, uint8_t id)
{
    switch(Id(id)) {
        default:
            return 0;
        case Id::Animate      : {
            uint32_t i = interp->stackLocal(0);
            return interp->animate(i);
        }
        case Id::Param        : {
            uint32_t i = interp->stackLocal(0);
            return uint32_t(interp->param(i));
        }
        case Id::Float        : {
            uint32_t v = interp->stackLocal(0);
            return floatToInt(float(v));
        }
        case Id::Int          : {
            float v = intToFloat(interp->stackLocal(0));
            return uint32_t(int32_t(v));
        }
        case Id::RandomInt    : {
            int32_t min = interp->stackLocal(0);
            int32_t max = interp->stackLocal(1);
            return uint32_t(interp->random(min, max));
        }
        case Id::RandomFloat  : {
            float min = intToFloat(interp->stackLocal(0));
            float max = intToFloat(interp->stackLocal(1));
            return floatToInt(interp->random(min, max));
        }
        case Id::InitArray    : {
            uint32_t i = interp->stackLocal(0);
            uint32_t v = interp->stackLocal(1);
            uint32_t n = interp->stackLocal(2);
            interp->initArray(i, v, n);
            return 0;
        }
        case Id::MinInt       : {
            interp->stackPush(min(int32_t(interp->stackLocal(0)), int32_t(interp->stackLocal(1))));
            return 0;
        }
        case Id::MinFloat     : {
            interp->stackPush(floatToInt(min(intToFloat(interp->stackLocal(0)), intToFloat(interp->stackLocal(1)))));
            return 0;
        }
        case Id::MaxInt       : {
            interp->stackPush(max(int32_t(interp->stackLocal(0)), int32_t(interp->stackLocal(1))));
            return 0;
        }
        case Id::MaxFloat     : {
            interp->stackPush(floatToInt(max(intToFloat(interp->stackLocal(0)), intToFloat(interp->stackLocal(1)))));
            return 0;
        }
    }
}
