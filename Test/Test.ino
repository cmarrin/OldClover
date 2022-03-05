/*-------------------------------------------------------------------------
    This source file is a part of Clover
    For the latest info, see https://github.com/cmarrin/Clover
    Copyright (c) 2021-2022, Chris Marrin
    All rights reserved.
    Use of this source code is governed by the MIT license that can be
    found in the LICENSE file.
-------------------------------------------------------------------------*/

#include <Clover.h>
#include <EEPROM.h>
#include "Test.h"
#include "Test1.h"

/*

Test

Each test is included as a .h file generated on the Mac and appears as a
uint_t array with a name of the form 'EEPROM_Upload_<name>'. Each array is
uploaded to EEPROM and then the test is run. All tests are named simply
"test".
*/

class Device : public clvr::Interpreter
{
public:
	Device() : Interpreter(nullptr, 0) { }
	
    virtual uint8_t rom(uint16_t i) const override
    {
        return EEPROM[i];
    }
    
    virtual void log(const char* s) const override
    {
        Serial.print(s);
    }
};

class Test
{
public:
	Test() { }
	~Test() { }
 
    void showError(clvr::Interpreter::Error error)
    {
        String errorMsg;
        
        switch(error) {
            case Device::Error::None:
            errorMsg = F("???");
            break;
            case Device::Error::CmdNotFound:
            errorMsg = F("bad cmd");
            break;
            break;
            case Device::Error::UnexpectedOpInIf:
            errorMsg = F("bad op in if");
            break;
            case Device::Error::InvalidOp:
            errorMsg = F("inv op");
            break;
            case Device::Error::OnlyMemAddressesAllowed:
            errorMsg = F("mem addrs only");
            break;
            case Device::Error::AddressOutOfRange:
            errorMsg = F("addr out of rng");
            break;
            case Device::Error::InvalidModuleOp:
            errorMsg = F("inv mod op");
            break;
            case Device::Error::ExpectedSetFrame:
            errorMsg = F("SetFrame needed");
            break;
            case Device::Error::InvalidNativeFunction:
            errorMsg = F("inv native func");
            break;
            case Device::Error::NotEnoughArgs:
            errorMsg = F("not enough args");
            break;
            case Device::Error::StackOverrun:
            errorMsg = F("can't call, stack full");
            break;
            case Device::Error::StackUnderrun:
            errorMsg = F("stack underrun");
            break;
            case Device::Error::StackOutOfRange:
            errorMsg = F("stack out of range");
            break;
            case Device::Error::WrongNumberOfArgs:
            errorMsg = F("wrong arg cnt");
            break;
        }

        Serial.print(F("Interp err: "));
        Serial.println(errorMsg);
    }
    
    void runTest(const char* name, const uint8_t* testCode, uint32_t size)
    {
        Serial.print(F("\nRunning test script '"));
        Serial.print(name);
        Serial.println(F("'..."));

        // First see if the test is already uploaded to avoid an EEPROM write
        Serial.println(F("Checking EEPROM..."));
        
        bool same = true;
        for (int i = 0; i < size; ++i) {
            if (EEPROM[i] != pgm_read_byte(&(testCode[i]))) {
                same = false;
                break;
            }
        }
        
        if (same) {
            Serial.println(F("EEPROM has correct code, skipping write..."));
        } else {
            Serial.println(F("EEPROM does not have correct code, writing..."));

            // Upload the test
            for (int i = 0; i < size; ++i) {
                EEPROM[i] = pgm_read_byte(&(testCode[i]));
            }
        }
        
        // Run the test
        uint8_t buf[2] = { 0, 0 };
        if (!_device.init("test", buf, 1)) {
            showError(_device.error());
        }
        Serial.println(F("...Finished running test"));
    }
	
	void setup()
	{
	    Serial.begin(115200);
		delay(500);
        randomSeed(millis());
        
		Serial.println(F("Test v0.1"));
  
        runTest("Test", EEPROM_Upload_Test, sizeof(EEPROM_Upload_Test));
        runTest("Test1", EEPROM_Upload_Test1, sizeof(EEPROM_Upload_Test1));
    }

	void loop()
	{
		delay(100);
	}

private:
    Device _device;
};

Test test;

void setup()
{
	test.setup();
}

void loop()
{
	test.loop();
}
