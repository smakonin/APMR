/*
 ModbusMaster.h - Modbus RTU protocol library for Arduino 
 Originally Written by Doc Walker (Rx)
 Major Changes By Stephen Makonin
 Copyright (C) 2010 Doc Walker and Stephen Makonin.  All right reserved.
 
 ModbusMaster is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 ModbusMaster is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with ModbusMaster.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ModbusMaster_h
#define ModbusMaster_h

#include "Arduino.h"
#include <util/crc16.h>

#define lowWord(ww) ((uint16_t) ((ww) & 0xFFFF))
#define highWord(ww) ((uint16_t) ((ww) >> 16))
#define LONG(hi, lo) ((uint32_t) ((hi) << 16 | (lo)))

class ModbusMaster
{
	private:
		///uint8_t  _RxTxTogglePin;
		static const uint8_t MaxBufferSize                = 64;
		uint16_t _ReadAddress;
		uint16_t _ReadQty;
		uint16_t _ResponseBuffer[MaxBufferSize];
		uint16_t _WriteAddress;
		uint16_t _WriteQty;
		uint16_t _TransmitBuffer[MaxBufferSize];
		
		// Modbus function codes for bit access
		static const uint8_t MBReadCoils                  = 0x01;
		static const uint8_t MBReadDiscreteInputs         = 0x02;
		static const uint8_t MBWriteSingleCoil            = 0x05;
		static const uint8_t MBWriteMultipleCoils         = 0x0F;
		
		// Modbus function codes for 16 bit access
		static const uint8_t MBReadHoldingRegisters       = 0x03;
		static const uint8_t MBReadInputRegisters         = 0x04;
		static const uint8_t MBWriteSingleRegister        = 0x06;
		static const uint8_t MBWriteMultipleRegisters     = 0x10;
		static const uint8_t MBMaskWriteRegister          = 0x16;
		static const uint8_t MBReadWriteMultipleRegisters = 0x17;
		
		// Modbus timeout [milliseconds]
		static const uint8_t MBResponseTimeout            = 3000;
		
		// master function that conducts Modbus transactions
		uint8_t ModbusMasterTransaction(uint8_t, uint8_t);
	
	public:		
		// Modbus exception codes
		static const uint8_t MBIllegalFunction            = 0x01;
		static const uint8_t MBIllegalDataAddress         = 0x02;
		static const uint8_t MBIllegalDataValue           = 0x03;
		static const uint8_t MBSlaveDeviceFailure         = 0x04;
		
		// Class-defined success/exception codes
		static const uint8_t MBSuccess                    = 0x00;
		static const uint8_t MBInvalidSlaveID             = 0xE0;
		static const uint8_t MBInvalidFunction            = 0xE1;
		static const uint8_t MBResponseTimedOut           = 0xE2;
		static const uint8_t MBInvalidCRC                 = 0xE3;

		ModbusMaster();
		void begin(uint16_t);
		void begin(uint8_t, uint16_t);
		
		uint16_t getResponseBuffer(uint8_t);
		void     clearResponseBuffer();
		uint8_t  setTransmitBuffer(uint8_t, uint16_t);
		void     clearTransmitBuffer();
		
		uint8_t  readCoils(uint8_t, uint16_t, uint16_t);
		uint8_t  readDiscreteInputs(uint8_t, uint16_t, uint16_t);
		uint8_t  readHoldingRegisters(uint8_t, uint16_t, uint16_t);
		uint8_t  readInputRegisters(uint8_t, uint16_t, uint8_t);
		uint8_t  writeSingleCoil(uint8_t, uint16_t, uint8_t);
		uint8_t  writeSingleRegister(uint8_t, uint16_t, uint16_t);
		uint8_t  writeMultipleCoils(uint8_t, uint16_t, uint16_t);
		uint8_t  writeMultipleRegisters(uint8_t, uint16_t, uint16_t);
		uint8_t  maskWriteRegister(uint8_t, uint16_t, uint16_t, uint16_t);
		uint8_t  readWriteMultipleRegisters(uint8_t, uint16_t, uint16_t, uint16_t, uint16_t);
    
};

extern ModbusMaster Modbus;

#endif
