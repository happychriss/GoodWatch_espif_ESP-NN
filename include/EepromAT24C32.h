
// The source code has been copied from https://github.com/Makuna/Rtc and extended to read data from more than a block
#pragma once

//I2C Slave Address  
static const uint8_t AT24C32_ADDRESS = 0x57; // 0b0 1010 A2 A1 A0
static const uint8_t EEPROM_PAGE_SIZE = 32;
static const uint16_t EEPROM_SIZE_KBIT = 32768;                         // EEPROMs are sized in kilobit
static const uint16_t EEPROM_BYTES = EEPROM_SIZE_KBIT / 8;
static const uint8_t EEPROM_PAGES = EEPROM_BYTES / EEPROM_PAGE_SIZE;

template<class T_WIRE_METHOD>
class EepromAt24c32 {
public:
    EepromAt24c32(T_WIRE_METHOD &wire, uint8_t addressBits = 0b111) :
            _address(AT24C32_ADDRESS | (addressBits & 0b00000111)),
            _wire(wire),
            _lastError(0) {
    }

    void Begin() {
        _wire.begin();
    }

    uint8_t LastError() {
        return _lastError;
    }

    void SetMemory(uint16_t memoryAddress, uint8_t value) {
        WriteMemoryBlock(memoryAddress, &value, 1);
    }

    uint8_t GetMemory(uint16_t memoryAddress) {
        uint8_t value;

        GetMemory(memoryAddress, &value, 1);

        return value;
    }

    // note: this method will write within a single page of eeprom.
    // Pages are 32 bytes (5 bits), so writing past a page boundary will
    // just wrap within the page of the starting memory address.  
    // 
    // xxxppppp pppaaaaa => p = page #, a = address within the page
    //
    // NOTE: hardware WIRE libraries often have a limit of a 32 byte send buffer.  The 
    // effect of this is that only 30 bytes can be sent, 2 bytes for the address to write to,
    // and then 30 bytes of the actual data. 
    uint8_t WriteMemoryBlock(uint16_t memoryAddress, const uint8_t *pValue, uint8_t countBytes) {
        uint8_t countWritten = 0;

        beginTransmission(memoryAddress);

        while (countBytes > 0) {
            _wire.write(*pValue++);
            delay(10); // per spec, memory writes

            countBytes--;
            countWritten++;
        }

        _lastError = _wire.endTransmission();

        return countWritten;
    }


    // reading data does not wrap within pages, but due to only using
    // 12 (32K) or 13 (64K) bits are used, they will wrap within the memory limits
    // of the installed EEPROM
    //
    // NOTE: hardware WIRE libraries may have a limit of a 32 byte recieve buffer.  The 
    // effect of this is that only 32 bytes can be read at one time.
    uint8_t GetMemory(uint16_t memoryAddress, uint8_t *pValue, uint8_t countBytes) {
        // set address to read from
        beginTransmission(memoryAddress);
        _lastError = _wire.endTransmission();

        if (_lastError != 0) {
            return 0;
        }

        // read the data
        uint8_t countRead = 0;

        countRead = _wire.requestFrom(_address, countBytes);
        countBytes = countRead;

        while (countBytes-- > 0) {
            *pValue++ = _wire.read();
        }

        return countRead;
    }

    //***************************************************************************
    uint8_t WriteMemoryBuffer(uint8_t *pValue, uint8_t countBytes) {
//        DP("Write Memory Buffer, size:");
//        DPL(countBytes);
        uint16_t eepromWriteAddress = 0;
        beginTransmission(eepromWriteAddress);

        while (countBytes > 0) {

//            DPF("[%i] -  %i\n", countBytes,  *pValue);
            _wire.write(*pValue++);
            delay(10); // per spec, memory writes
            countBytes--;
            eepromWriteAddress++;

            // xxxppppp pppa aaaa => p = page #, a = address within the page
            if (eepromWriteAddress < EEPROM_BYTES && ((eepromWriteAddress >> 4) & 0xFF) != (((eepromWriteAddress - 1) >> 4) & 0xFF)) {
//                DPL("***!!! NEW Page !!!***");
//                DP("WriteAddress:");
//                DPL(eepromWriteAddress);
                // This is a new page, finish the previous write and start a new one
                endTransmission();
                beginTransmission(eepromWriteAddress);
            }
        }

        endTransmission();

        return 1;
    }


private:
    const uint8_t _address;

    T_WIRE_METHOD &_wire;
    uint8_t _lastError;

    void beginTransmission(uint16_t memoryAddress) {
        _wire.beginTransmission(_address);
        _wire.write(memoryAddress >> 8) & 0xFf;
        _wire.write(memoryAddress & 0xFf);
    }

    uint8_t endTransmission() {
        if (_wire.endTransmission() > 0) {
            // Failure
            return 0;
        }
//        DPL("Wait for complete");
        // Poll for write to complete
        while (!Wire.requestFrom(_address, (uint8_t) 1));
        // Move to new page
    }
};