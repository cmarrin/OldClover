/*-------------------------------------------------------------------------
    This source file is a part of Clover
    For the latest info, see https://github.com/cmarrin/Clover
    Copyright (c) 2021-2022, Chris Marrin
    All rights reserved.
    Use of this source code is governed by the MIT license that can be
    found in the LICENSE file.
-------------------------------------------------------------------------*/

// NativeCore
//
// core native functions for Interpreter

#pragma once

#include "Interpreter.h"

namespace clvr {

constexpr uint8_t CorePrefix0 = 0x00;
constexpr uint8_t CorePrefix1 = 0x00;

class NativeCore : public NativeModule
{
public:
    enum class Id {
        Animate      = CorePrefix0 | 0x00,
        Param        = CorePrefix0 | 0x01,
        Float        = CorePrefix0 | 0x02,
        Int          = CorePrefix0 | 0x03,
        RandomInt    = CorePrefix0 | 0x07,
        RandomFloat  = CorePrefix0 | 0x08,
        InitArray    = CorePrefix0 | 0x09,
        MinInt       = CorePrefix0 | 0x0a,
        MinFloat     = CorePrefix0 | 0x0b,
        MaxInt       = CorePrefix0 | 0x0c,
        MaxFloat     = CorePrefix0 | 0x0d,
    };

    virtual bool hasId(uint8_t id) const override;
    virtual uint8_t numParams(uint8_t id) const override;
    virtual int32_t call(Interpreter*, uint8_t id) override;

#ifndef ARDUINO
    virtual void addFunctions(CompileEngine*) override;
#endif

private:
};

}
