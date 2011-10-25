/*
 ModbusMaster.cpp - Modbus RTU protocol library for Arduino 
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

#include "ModbusMaster.h"

HardwareSerial MBSerial = Serial; ///< Pointer to Serial class object


ModbusMaster::ModbusMaster(void)
{
}

void ModbusMaster::begin(uint16_t BaudRate)
{
	begin(0, BaudRate);
}

void ModbusMaster::begin(uint8_t SerialPort, uint16_t BaudRate)
{
	switch(SerialPort)
	{
#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
		case 1:
			MBSerial = Serial1;
			break;
			
		case 2:
			MBSerial = Serial2;
			break;
			
		case 3:
			MBSerial = Serial3;
			break;
#endif
			
		case 0:
		default:
			MBSerial = Serial;
			break;
	}
	
	MBSerial.begin(BaudRate);
	
	clearTransmitBuffer();
}

/**
 Retrieve data from response buffer.
 
 @see ModbusMaster::clearResponseBuffer()
 @param Index index of response buffer array (0x00..0x3F)
 @return value in position Index of response buffer (0x0000..0xFFFF)
 @ingroup buffer
 */
uint16_t ModbusMaster::getResponseBuffer(uint8_t Index)
{
	if (Index < MaxBufferSize)
	{
		return _ResponseBuffer[Index];
	}
	else
	{
		return 0xFFFF;
	}
}

/**
 Clear Modbus response buffer.
 
 @see ModbusMaster::getResponseBuffer(uint8_t Index)
 @ingroup buffer
 */
void ModbusMaster::clearResponseBuffer()
{
	uint8_t i;
	
	for (i = 0; i < MaxBufferSize; i++)
	{
		_ResponseBuffer[i] = 0;
	}
}

/**
 Place data in transmit buffer.
 
 @see ModbusMaster::clearTransmitBuffer()
 @param Index index of transmit buffer array (0x00..0x3F)
 @param Value value to place in position Index of transmit buffer (0x0000..0xFFFF)
 @return 0 on success; exception number on failure
 @ingroup buffer
 */
uint8_t ModbusMaster::setTransmitBuffer(uint8_t Index, uint16_t Value)
{
	if (Index < MaxBufferSize)
	{
		_TransmitBuffer[Index] = Value;
		return MBSuccess;
	}
	else
	{
		return MBIllegalDataAddress;
	}
}

/**
 Clear Modbus transmit buffer.
 
 @see ModbusMaster::setTransmitBuffer(uint8_t Index, uint16_t Value)
 @ingroup buffer
 */
void ModbusMaster::clearTransmitBuffer()
{
	uint8_t i;
	
	for (i = 0; i < MaxBufferSize; i++)
	{
		_TransmitBuffer[i] = 0;
	}
}

/**
 Modbus function 0x01 Read Coils.
 
 This function code is used to read from 1 to 2000 contiguous status of 
 coils in a remote device. The request specifies the starting address, 
 i.e. the address of the first coil specified, and the number of coils. 
 Coils are addressed starting at zero.
 
 The coils in the response buffer are packed as one coil per bit of the 
 data field. Status is indicated as 1=ON and 0=OFF. The LSB of the first 
 data word contains the output addressed in the query. The other coils 
 follow toward the high order end of this word and from low order to high 
 order in subsequent words.
 
 If the returned quantity is not a multiple of sixteen, the remaining 
 bits in the final data word will be padded with zeros (toward the high 
 order end of the word).
 
 @param ReadAddress address of first coil (0x0000..0xFFFF)
 @param BitQty quantity of coils to read (1..2000, enforced by remote device)
 @return 0 on success; exception number on failure
 @ingroup discrete
 */
uint8_t ModbusMaster::readCoils(uint8_t MBSlave, uint16_t ReadAddress, uint16_t BitQty)
{
	_ReadAddress = ReadAddress;
	_ReadQty = BitQty;
	return ModbusMasterTransaction(MBSlave, MBReadCoils);
}

/**
 Modbus function 0x02 Read Discrete Inputs.
 
 This function code is used to read from 1 to 2000 contiguous status of 
 discrete inputs in a remote device. The request specifies the starting 
 address, i.e. the address of the first input specified, and the number 
 of inputs. Discrete inputs are addressed starting at zero.
 
 The discrete inputs in the response buffer are packed as one input per 
 bit of the data field. Status is indicated as 1=ON; 0=OFF. The LSB of 
 the first data word contains the input addressed in the query. The other 
 inputs follow toward the high order end of this word, and from low order 
 to high order in subsequent words.
 
 If the returned quantity is not a multiple of sixteen, the remaining 
 bits in the final data word will be padded with zeros (toward the high 
 order end of the word).
 
 @param ReadAddress address of first discrete input (0x0000..0xFFFF)
 @param BitQty quantity of discrete inputs to read (1..2000, enforced by remote device)
 @return 0 on success; exception number on failure
 @ingroup discrete
 */
uint8_t ModbusMaster::readDiscreteInputs(uint8_t MBSlave, uint16_t ReadAddress,
										 uint16_t BitQty)
{
	_ReadAddress = ReadAddress;
	_ReadQty = BitQty;
	return ModbusMasterTransaction(MBSlave, MBReadDiscreteInputs);
}

/**
 Modbus function 0x03 Read Holding Registers.
 
 This function code is used to read the contents of a contiguous block of 
 holding registers in a remote device. The request specifies the starting 
 register address and the number of registers. Registers are addressed 
 starting at zero.
 
 The register data in the response buffer is packed as one word per 
 register.
 
 @param ReadAddress address of the first holding register (0x0000..0xFFFF)
 @param ReadQty quantity of holding registers to read (1..125, enforced by remote device)
 @return 0 on success; exception number on failure
 @ingroup register
 */
uint8_t ModbusMaster::readHoldingRegisters(uint8_t MBSlave, uint16_t ReadAddress,
										   uint16_t ReadQty)
{
	_ReadAddress = ReadAddress;
	_ReadQty = ReadQty;
	return ModbusMasterTransaction(MBSlave, MBReadHoldingRegisters);
}

/**
 Modbus function 0x04 Read Input Registers.
 
 This function code is used to read from 1 to 125 contiguous input 
 registers in a remote device. The request specifies the starting 
 register address and the number of registers. Registers are addressed 
 starting at zero.
 
 The register data in the response buffer is packed as one word per 
 register.
 
 @param ReadAddress address of the first input register (0x0000..0xFFFF)
 @param ReadQty quantity of input registers to read (1..125, enforced by remote device)
 @return 0 on success; exception number on failure
 @ingroup register
 */
uint8_t ModbusMaster::readInputRegisters(uint8_t MBSlave, uint16_t ReadAddress,
										 uint8_t ReadQty)
{
	_ReadAddress = ReadAddress;
	_ReadQty = ReadQty;
	return ModbusMasterTransaction(MBSlave, MBReadInputRegisters);
}

/**
 Modbus function 0x05 Write Single Coil.
 
 This function code is used to write a single output to either ON or OFF 
 in a remote device. The requested ON/OFF state is specified by a 
 constant in the state field. A non-zero value requests the output to be 
 ON and a value of 0 requests it to be OFF. The request specifies the 
 address of the coil to be forced. Coils are addressed starting at zero.
 
 @param WriteAddress address of the coil (0x0000..0xFFFF)
 @param State 0=OFF, non-zero=ON (0x00..0xFF)
 @return 0 on success; exception number on failure
 @ingroup discrete
 */
uint8_t ModbusMaster::writeSingleCoil(uint8_t MBSlave, uint16_t WriteAddress, uint8_t State)
{
	_WriteAddress = WriteAddress;
	_WriteQty = (State ? 0xFF00 : 0x0000);
	return ModbusMasterTransaction(MBSlave, MBWriteSingleCoil);
}

/**
 Modbus function 0x06 Write Single Register.
 
 This function code is used to write a single holding register in a 
 remote device. The request specifies the address of the register to be 
 written. Registers are addressed starting at zero.
 
 @param WriteAddress address of the holding register (0x0000..0xFFFF)
 @param WriteValue value to be written to holding register (0x0000..0xFFFF)
 @return 0 on success; exception number on failure
 @ingroup register
 */
uint8_t ModbusMaster::writeSingleRegister(uint8_t MBSlave, uint16_t WriteAddress,
										  uint16_t WriteValue)
{
	_WriteAddress = WriteAddress;
	_WriteQty = 0;
	_TransmitBuffer[0] = WriteValue;
	return ModbusMasterTransaction(MBSlave, MBWriteSingleRegister);
}

/**
 Modbus function 0x0F Write Multiple Coils.
 
 This function code is used to force each coil in a sequence of coils to 
 either ON or OFF in a remote device. The request specifies the coil 
 references to be forced. Coils are addressed starting at zero.
 
 The requested ON/OFF states are specified by contents of the transmit 
 buffer. A logical '1' in a bit position of the buffer requests the 
 corresponding output to be ON. A logical '0' requests it to be OFF.
 
 @param WriteAddress address of the first coil (0x0000..0xFFFF)
 @param BitQty quantity of coils to write (1..2000, enforced by remote device)
 @return 0 on success; exception number on failure
 @ingroup discrete
 */
uint8_t ModbusMaster::writeMultipleCoils(uint8_t MBSlave, uint16_t WriteAddress,
										 uint16_t BitQty)
{
	_WriteAddress = WriteAddress;
	_WriteQty = BitQty;
	return ModbusMasterTransaction(MBSlave, MBWriteMultipleCoils);
}

/**
 Modbus function 0x10 Write Multiple Registers.
 
 This function code is used to write a block of contiguous registers (1 
 to 123 registers) in a remote device.
 
 The requested written values are specified in the transmit buffer. Data 
 is packed as one word per register.
 
 @param WriteAddress address of the holding register (0x0000..0xFFFF)
 @param WriteQty quantity of holding registers to write (1..123, enforced by remote device)
 @return 0 on success; exception number on failure
 @ingroup register
 */
uint8_t ModbusMaster::writeMultipleRegisters(uint8_t MBSlave, uint16_t WriteAddress,
											 uint16_t WriteQty)
{
	_WriteAddress = WriteAddress;
	_WriteQty = WriteQty;
	return ModbusMasterTransaction(MBSlave, MBWriteMultipleRegisters);
}

/**
 Modbus function 0x16 Mask Write Register.
 
 This function code is used to modify the contents of a specified holding 
 register using a combination of an AND mask, an OR mask, and the 
 register's current contents. The function can be used to set or clear 
 individual bits in the register.
 
 The request specifies the holding register to be written, the data to be 
 used as the AND mask, and the data to be used as the OR mask. Registers 
 are addressed starting at zero.
 
 The function's algorithm is:
 
 Result = (Current Contents && And_Mask) || (Or_Mask && (~And_Mask))
 
 @param WriteAddress address of the holding register (0x0000..0xFFFF)
 @param AndMask AND mask (0x0000..0xFFFF)
 @param OrMask OR mask (0x0000..0xFFFF)
 @return 0 on success; exception number on failure
 @ingroup register
 */
uint8_t ModbusMaster::maskWriteRegister(uint8_t MBSlave, uint16_t WriteAddress,
										uint16_t AndMask, uint16_t OrMask)
{
	_WriteAddress = WriteAddress;
	_TransmitBuffer[0] = AndMask;
	_TransmitBuffer[1] = OrMask;
	return ModbusMasterTransaction(MBSlave, MBMaskWriteRegister);
}

/**
 Modbus function 0x17 Read Write Multiple Registers.
 
 This function code performs a combination of one read operation and one 
 write operation in a single MODBUS transaction. The write operation is 
 performed before the read. Holding registers are addressed starting at 
 zero.
 
 The request specifies the starting address and number of holding 
 registers to be read as well as the starting address, and the number of 
 holding registers. The data to be written is specified in the transmit 
 buffer.
 
 @param ReadAddress address of the first holding register (0x0000..0xFFFF)
 @param ReadQty quantity of holding registers to read (1..125, enforced by remote device)
 @param WriteAddress address of the first holding register (0x0000..0xFFFF)
 @param WriteQty quantity of holding registers to write (1..121, enforced by remote device)
 @return 0 on success; exception number on failure
 @ingroup register
 */
uint8_t ModbusMaster::readWriteMultipleRegisters(uint8_t MBSlave, uint16_t ReadAddress,
												 uint16_t ReadQty, uint16_t WriteAddress, uint16_t WriteQty)
{
	_ReadAddress = ReadAddress;
	_ReadQty = ReadQty;
	_WriteAddress = WriteAddress;
	_WriteQty = WriteQty;
	return ModbusMasterTransaction(MBSlave, MBReadWriteMultipleRegisters);
}

/**
 Modbus transaction engine.
 Sequence:
 - assemble Modbus Request Application Data Unit (ADU),
 based on particular function called
 - transmit request over selected serial port
 - wait for/retrieve response
 - evaluate/disassemble response
 - return status (success/exception)
 
 @param MBFunction Modbus function (0x01..0xFF)
 @return 0 on success; exception number on failure
 */
uint8_t ModbusMaster::ModbusMasterTransaction(uint8_t MBSlave, uint8_t MBFunction)
{
	uint8_t ModbusADU[256];
	uint8_t ModbusADUSize = 0;
	uint8_t i, Qty;
	uint16_t CRC;
	uint8_t TimeLeft = MBResponseTimeout;
	uint8_t BytesLeft = 8;
	uint8_t MBStatus = MBSuccess;
	
	///if(_RxTxTogglePin != -1)
	///{
	///	digitalWrite(_RxTxTogglePin, HIGH);
	///	delay(1);
	///}
	
	// assemble Modbus Request Application Data Unit
	ModbusADU[ModbusADUSize++] = MBSlave;
	ModbusADU[ModbusADUSize++] = MBFunction;
	
	switch(MBFunction)
	{
		case MBReadCoils:
		case MBReadDiscreteInputs:
		case MBReadInputRegisters:
		case MBReadHoldingRegisters:
		case MBReadWriteMultipleRegisters:
			ModbusADU[ModbusADUSize++] = highByte(_ReadAddress);
			ModbusADU[ModbusADUSize++] = lowByte(_ReadAddress);
			ModbusADU[ModbusADUSize++] = highByte(_ReadQty);
			ModbusADU[ModbusADUSize++] = lowByte(_ReadQty);
			break;
	}
	
	switch(MBFunction)
	{
		case MBWriteSingleCoil:
		case MBMaskWriteRegister:
		case MBWriteMultipleCoils:
		case MBWriteSingleRegister:
		case MBWriteMultipleRegisters:
		case MBReadWriteMultipleRegisters:
			ModbusADU[ModbusADUSize++] = highByte(_WriteAddress);
			ModbusADU[ModbusADUSize++] = lowByte(_WriteAddress);
			break;
	}
	
	switch(MBFunction)
	{
		case MBWriteSingleCoil:
			ModbusADU[ModbusADUSize++] = highByte(_WriteQty);
			ModbusADU[ModbusADUSize++] = lowByte(_WriteQty);
			break;
			
		case MBWriteSingleRegister:
			ModbusADU[ModbusADUSize++] = highByte(_TransmitBuffer[0]);
			ModbusADU[ModbusADUSize++] = lowByte(_TransmitBuffer[0]);
			break;
			
		case MBWriteMultipleCoils:
			ModbusADU[ModbusADUSize++] = highByte(_WriteQty);
			ModbusADU[ModbusADUSize++] = lowByte(_WriteQty);
			Qty = (_WriteQty % 8) ? ((_WriteQty >> 3) + 1) : (_WriteQty >> 3);
			ModbusADU[ModbusADUSize++] = Qty;
			for (i = 0; i < Qty; i++)
			{
				switch(i % 2)
				{
					case 0: // i is even
						ModbusADU[ModbusADUSize++] = lowByte(_TransmitBuffer[i >> 1]);
						break;
						
					case 1: // i is odd
						ModbusADU[ModbusADUSize++] = highByte(_TransmitBuffer[i >> 1]);
						break;
				}
			}
			break;
			
		case MBWriteMultipleRegisters:
		case MBReadWriteMultipleRegisters:
			ModbusADU[ModbusADUSize++] = highByte(_WriteQty);
			ModbusADU[ModbusADUSize++] = lowByte(_WriteQty);
			ModbusADU[ModbusADUSize++] = lowByte(_WriteQty << 1);
			
			for (i = 0; i < lowByte(_WriteQty); i++)
			{
				ModbusADU[ModbusADUSize++] = highByte(_TransmitBuffer[i]);
				ModbusADU[ModbusADUSize++] = lowByte(_TransmitBuffer[i]);
			}
			break;
			
		case MBMaskWriteRegister:
			ModbusADU[ModbusADUSize++] = highByte(_TransmitBuffer[0]);
			ModbusADU[ModbusADUSize++] = lowByte(_TransmitBuffer[0]);
			ModbusADU[ModbusADUSize++] = highByte(_TransmitBuffer[1]);
			ModbusADU[ModbusADUSize++] = lowByte(_TransmitBuffer[1]);
			break;
	}
	
	
	// append CRC
	CRC = 0xFFFF;
	for (i = 0; i < ModbusADUSize; i++)
	{
		CRC = _crc16_update(CRC, ModbusADU[i]);
	}
	ModbusADU[ModbusADUSize++] = lowByte(CRC);
	ModbusADU[ModbusADUSize++] = highByte(CRC);
	ModbusADU[ModbusADUSize] = 0;
	
	// transmit request
	for (i = 0; i < ModbusADUSize; i++)
	{
		MBSerial.print(ModbusADU[i], BYTE);
	}
	
	ModbusADUSize = 0;
	MBSerial.flush();
	
	
	///if(_RxTxTogglePin != -1)
	///{
	///	delay(2);
	///	digitalWrite(_RxTxTogglePin, LOW);
	///}
	
	// loop until we run out of time or bytes, or an error occurs
	while (TimeLeft && BytesLeft && !MBStatus)
	{
		if (MBSerial.available())
		{
			ModbusADU[ModbusADUSize++] = MBSerial.read();
			
			if(ModbusADUSize == 1 && (ModbusADU[ModbusADUSize-1] == 0x00 || ModbusADU[ModbusADUSize-1] == 0xFF))
				ModbusADUSize--;
			
			BytesLeft--;
		}
		else
		{
			delayMicroseconds(1000);
			TimeLeft--;
		}
		
		// evaluate slave ID, function code once enough bytes have been read
		if (ModbusADUSize == 5)
		{
			// verify response is for correct Modbus slave
			if (ModbusADU[0] != MBSlave)
			{
				MBStatus = MBInvalidSlaveID;
				//Serial.print("Error Invalid Slave ID: ");
				//Serial.println((int)ModbusADU[0]);
				break;
			}
			
			// verify response is for correct Modbus function code (mask exception bit 7)
			if ((ModbusADU[1] & 0x7F) != MBFunction)
			{
				MBStatus = MBInvalidFunction;
				break;
			}
			
			// check whether Modbus exception occurred; return Modbus Exception Code
			if (bitRead(ModbusADU[1], 7))
			{
				MBStatus = ModbusADU[2];
				break;
			}
			
			// evaluate returned Modbus function code
			switch(ModbusADU[1])
			{
				case MBReadCoils:
				case MBReadDiscreteInputs:
				case MBReadInputRegisters:
				case MBReadHoldingRegisters:
				case MBReadWriteMultipleRegisters:
					BytesLeft = ModbusADU[2];
					break;
					
				case MBWriteSingleCoil:
				case MBWriteMultipleCoils:
				case MBWriteSingleRegister:
					BytesLeft = 3;
					break;
					
				case MBMaskWriteRegister:
					BytesLeft = 5;
					break;
			}
		}
		
		if (ModbusADUSize == 6)
		{
			switch(ModbusADU[1])
			{
				case MBWriteMultipleRegisters:
					BytesLeft = ModbusADU[5];
					break;
			}
		}
	}
	
	// verify response is large enough to inspect further
	if (!MBStatus && (TimeLeft == 0 || ModbusADUSize < 5))
	{
		MBStatus = MBResponseTimedOut;
	}
	
	// calculate CRC
	CRC = 0xFFFF;
	for (i = 0; i < (ModbusADUSize - 2); i++)
	{
		CRC = _crc16_update(CRC, ModbusADU[i]);
	}
	
	// verify CRC
	if (!MBStatus && (lowByte(CRC) != ModbusADU[ModbusADUSize - 2] ||
						highByte(CRC) != ModbusADU[ModbusADUSize - 1]))
	{
		MBStatus = MBInvalidCRC;
	}
	
	// disassemble ADU into words
	if (!MBStatus)
	{
		// evaluate returned Modbus function code
		switch(ModbusADU[1])
		{
			case MBReadCoils:
			case MBReadDiscreteInputs:
				// load bytes into word; response bytes are ordered L, H, L, H, ...
				for (i = 0; i < (ModbusADU[2] >> 1); i++)
				{
					if (i < MaxBufferSize)
					{
						_ResponseBuffer[i] = word(ModbusADU[2 * i + 4], ModbusADU[2 * i + 3]);
					}
				}
				
				// in the event of an odd number of bytes, load last byte into zero-padded word
				if (ModbusADU[2] % 2)
				{
					if (i < MaxBufferSize)
					{
						_ResponseBuffer[i] = word(0, ModbusADU[2 * i + 3]);
					}
				}
				break;
				
			case MBReadInputRegisters:
			case MBReadHoldingRegisters:
			case MBReadWriteMultipleRegisters:
				// load bytes into word; response bytes are ordered H, L, H, L, ...
				for (i = 0; i < (ModbusADU[2] >> 1); i++)
				{
					if (i < MaxBufferSize)
					{
						_ResponseBuffer[i] = word(ModbusADU[2 * i + 3], ModbusADU[2 * i + 4]);
					}
				}
				break;
		}
	}
	
	return MBStatus;
}

ModbusMaster Modbus = ModbusMaster();
