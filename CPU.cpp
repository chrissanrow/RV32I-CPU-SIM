#include "CPU.h"

/*----Helper/Util Functions----*/

int32_t twoInputMux(bool select, int32_t input0, int32_t input1)
{
	return select ? input1 : input0;
}

template <size_t N>
std::bitset<N> getBitRange(const std::bitset<N> &bits, size_t high, size_t low)
{
	std::bitset<N> result = 0;
	for (size_t i = low; i <= high; ++i)
	{
		result[i - low] = bits[i];
	}
	return result;
}

std::bitset<32> signExtend(const std::bitset<32> &input, size_t originalSize)
{
	std::bitset<32> result = input;
	bool signBit = input[originalSize - 1];
	for (size_t i = originalSize; i < 32; ++i)
	{
		result[i] = signBit;
	}
	return result;
}

/*----Static Functions----*/

InstructionType getInstructionType(std::bitset<32> instruction)
{
	InstructionType type;
	auto opcode = getBitRange(instruction, 6, 0).to_ulong();
	if (opcode == 0b0110011)
	{
		// R-type (SUB, AND)
		type = InstructionType::R_TYPE;
	}
	else if (opcode == 0b0010011 || opcode == 0b0000011)
	{
		// I-type (ADDI, ORI, SLTIU, LBU, LW)
		type = InstructionType::I_TYPE;
	}
	else if (opcode == 0b0100011)
	{
		// S-type (SH, SW)
		type = InstructionType::S_TYPE;
	}
	else if (opcode == 0b1100011)
	{
		// B-type (BNE)
		type = InstructionType::B_TYPE;
	}
	else if (opcode == 0b0110111)
	{
		// U-type (LUI)
		type = InstructionType::U_TYPE;
	}
	else if (opcode == 0b1100111) // J-type
	{
		// J-type (JALR)
		type = InstructionType::J_TYPE;
	}
	else
	{
		type = InstructionType::UNKNOWN_TYPE;
	}
	return type;
}

int32_t immediateGenerator(std::bitset<32> instruction)
{
	// Determine type of instruction
	auto type = getInstructionType(instruction);
	uint32_t rawInstruction = instruction.to_ulong();
	// Funct3 is needed to differentiate I-Type variants

	switch (type)
	{
	case InstructionType::I_TYPE:
	case InstructionType::J_TYPE:
	{
		uint32_t funct3 = getBitRange(instruction, 14, 12).to_ulong();
		// ADDI, ORI, LW/LBU, SLTIU, JALR: Instr[31:20] sign extended
		int32_t signedImmediate = (int32_t)(rawInstruction) >> 20;
		return signedImmediate;
	}
	case InstructionType::S_TYPE:
	{
		// S-TYPE: Imm[11:5] (Instr[31:25]) || Imm[4:0] (Instr[11:7])
		std::bitset<32> immBits;
		for (size_t i = 0; i < 5; ++i)
		{
			immBits[i] = instruction[i + 7];
		}
		for (size_t i = 0; i < 7; ++i)
		{
			immBits[i + 5] = instruction[i + 25];
		}

		// sign-extend from 12 bits
		auto res = signExtend(immBits, 12);
		return static_cast<int32_t>(res.to_ulong());
	}
	case InstructionType::B_TYPE:
	{
		// B-TYPE: Imm[12:0] - sign extended
		// bits 12[31], [7], [30:25], [11:8]
		std::bitset<32> immBits;
		// force immediate to be even -> set bit 0 to 0
		immBits[0] = 0;
		immBits[11] = instruction[7]; // bit 7 to bit 11
		for (size_t i = 1; i <= 4; ++i)
		{
			immBits[i] = instruction[i + 7]; // bits 11:8 to bits 4:1
		}
		for (size_t i = 5; i <= 10; ++i)
		{
			immBits[i] = instruction[i + 20]; // bits 30:25 to bits 10:5
		}
		immBits[12] = instruction[31]; // bit 31 to bit 12
		// sign-extend
		auto res = signExtend(immBits, 13);
		return static_cast<int32_t>(res.to_ulong());
	}
	case InstructionType::U_TYPE:
	{
		// LUI: mask out lower 12 bits to get upper 20 bits
		return rawInstruction & 0xFFFFF000;
	}
	default:
	{
		// R-type or unknown type
		return 0;
	}
	}
}

/*----CPU Top-Level----*/

CPU::CPU(const char instrMem[4096])
{
	m_PC = 0;					   // set PC to 0
	for (int i = 0; i < 4096; i++) // copy instrMEM
	{
		m_instMem[i] = static_cast<uint8_t>(instrMem[i]);
	}
}

uint32_t CPU::fetchInstruction(unsigned long PC)
{
	uint32_t instruction = 0;
	instruction |= (static_cast<uint32_t>(m_instMem[PC]) & 0xFF);
	instruction |= (static_cast<uint32_t>(m_instMem[PC + 1]) & 0xFF) << 8;
	instruction |= (static_cast<uint32_t>(m_instMem[PC + 2]) & 0xFF) << 16;
	instruction |= (static_cast<uint32_t>(m_instMem[PC + 3]) & 0xFF) << 24;
	return instruction;
}

unsigned long CPU::readPC()
{
	return m_PC;
}

void CPU::incPC()
{
	m_PC += 4;
}

void CPU::setPC(unsigned long newPC)
{
	m_PC = newPC;
}

/*----Register File----*/

RegisterFile::RegisterFile()
{
	for (int i = 0; i < 32; i++)
	{
		m_registers[i] = 0;
	}
}

int32_t RegisterFile::readRegister(int index) const
{
	return m_registers[index];
}

void RegisterFile::writeRegister(int index, int32_t value, bool regWrite)
{
	if (regWrite && index != 0)
	{
		m_registers[index] = value;
	}
}

/*----Controller-----*/

ControlSignals Controller::getControlSignals(uint8_t opcode)
{
	ControlSignals signals{};
	/*
	ALUOp mapping:
	ADD = 00 (LW and SW)
	SUB = 01 (BNE)
	R-TYPE = 10 (SUB, AND, SRA)
	I-TYPE = 11 (ADDI, ORI, SLTIU)
	*/
	switch (opcode)
	{
	// R-TYPE (SUB, AND, SRA)
	case 0b0110011:
		signals.jalr = 0;
		signals.lui = 0;
		signals.branch = 0;
		signals.memRead = 0;
		signals.memToReg = 0;
		signals.memWrite = 0;
		signals.aluSrc = 0;
		signals.regWrite = 1;
		signals.aluOp = 0b10;
		break;

	// I-TYPE (ADDI, ORI, SLTIU)
	case 0b0010011:
		signals.jalr = 0;
		signals.lui = 0;
		signals.branch = 0;
		signals.memRead = 0;
		signals.memToReg = 0;
		signals.memWrite = 0;
		signals.aluSrc = 1;
		signals.regWrite = 1;
		signals.aluOp = 0b11;
		break;

	// LOAD (lw/lbu)
	case 0b0000011:
		signals.jalr = 0;
		signals.lui = 0;
		signals.branch = 0;
		signals.memRead = 1;
		signals.memToReg = 1;
		signals.memWrite = 0;
		signals.aluSrc = 1; // ALU calculates address: rs1 + Imm
		signals.regWrite = 1;
		signals.aluOp = 0b00; // ALU operation is ADD (for address calculation)
		break;

	// STORE (sw/sh)
	case 0b0100011:
		signals.jalr = 0;
		signals.lui = 0;
		signals.branch = 0;
		signals.memRead = 0;
		signals.memToReg = 0;
		signals.memWrite = 1;
		signals.aluSrc = 1;	  // ALU calculates address: rs1 + Imm
		signals.regWrite = 0; // Don't write back to register file
		signals.aluOp = 0b00; // ALU operation is ADD (for address calculation)
		break;

	// BNE
	case 0b1100011:
		signals.jalr = 0;
		signals.lui = 0;
		signals.branch = 1; // Enable branch logic
		signals.memRead = 0;
		signals.memToReg = 0;
		signals.memWrite = 0;
		signals.aluSrc = 0; // ALU compares rs1 and rs2
		signals.regWrite = 0;
		signals.aluOp = 0b01; // ALU operation is SUB (for comparison)
		break;

	// JALR
	case 0b1100111:
		signals.jalr = 1; // Enable JALR specific logic
		signals.lui = 0;
		signals.branch = 0;
		signals.memRead = 0;
		signals.memToReg = 0;
		signals.memWrite = 0;
		signals.aluSrc = 1;	  // target = rs1 + Imm
		signals.regWrite = 1; // write PC + 4 to rd
		signals.aluOp = 0b00;
		break;

	// LUI
	case 0b0110111:
		signals.jalr = 0;
		signals.lui = 1; // Enable LUI specific logic (used in WB stage for PC+4 MUX)
		signals.branch = 0;
		signals.memRead = 0;
		signals.memToReg = 0;
		signals.memWrite = 0;
		signals.aluSrc = 1;
		signals.regWrite = 1;
		signals.aluOp = 0b00; // any ALU operation (will be ignored)
		break;

	default:
		// default to NOP
		signals.jalr = 0;
		signals.lui = 0;
		signals.branch = 0;
		signals.memRead = 0;
		signals.memToReg = 0;
		signals.memWrite = 0;
		signals.aluSrc = 0;
		signals.regWrite = 0;
		signals.aluOp = 0b00;
		break;
	}
	return signals;
}

/*----ALU Control----*/

ALUOperation ALUControl::getALUOperation(uint8_t ALUOp, uint8_t funct3, uint8_t funct7)
{
	switch (ALUOp)
	{
	case 0b00:
		return ALUOperation::ADD; // LW/LBU and SW/SH and JALR
	case 0b01:
		return ALUOperation::SUB; // BNE
	case 0b10:					  // R-TYPE
		switch (funct3)
		{
		case 0b000: // SUB
			return ALUOperation::SUB;
		case 0b111: // AND
			return ALUOperation::AND;
		case 0b101: // SRA
			return ALUOperation::SRA;
		default:
			return ALUOperation::NOOP;
		}
	case 0b11: // I-TYPE
		switch (funct3)
		{
		case 0b000: // ADDI
			return ALUOperation::ADD;
		case 0b110: // ORI
			return ALUOperation::OR;
		case 0b011: // SLTIU
			return ALUOperation::SLTIU;
		default:
			return ALUOperation::NOOP;
		}
	default:
		return ALUOperation::NOOP;
	}
}

/*----ALU----*/

std::pair<int32_t, bool> ALU::execute(int32_t operand1, int32_t operand2, ALUOperation operation, bool isLUI)
{
	int32_t result = 0;
	bool zeroFlag = false;

	if (isLUI)
	{
		result = operand2;
	}
	else
	{
		switch (operation)
		{
		case ALUOperation::ADD:
			result = operand1 + operand2;
			break;
		case ALUOperation::SUB:
			result = operand1 - operand2;
			break;
		case ALUOperation::AND:
			result = operand1 & operand2;
			break;
		case ALUOperation::OR:
			result = operand1 | operand2;
			break;
		case ALUOperation::SRA:
			result = operand1 >> (operand2 & 0b11111); // shift by lower 5 bits of operand2
			break;
		case ALUOperation::SLTIU:
			// unsigned comparison
			result = (static_cast<uint32_t>(operand1) < static_cast<uint32_t>(operand2)) ? 1 : 0;
			break;
		case ALUOperation::NOOP:
		default:
			result = 0;
			break;
		}
	}

	zeroFlag = (result == 0);
	return {result, zeroFlag};
}

/*----Data Memory----*/

DataMemory::DataMemory()
{
	m_dmemory.resize(409600, 0); // initialize data memory to 0
}

// read data in little-endian fashion
int32_t DataMemory::readData(uint32_t address, bool memRead, uint8_t funct3)
{
	if (!memRead)
		return 0;

	uint32_t value = 0;
	// always read first 2 bytes for LBU/LW
	value |= static_cast<uint32_t>(m_dmemory[address]);

	if (funct3 == 0b010) // load last 3 bytes for LW
	{
		value |= static_cast<uint32_t>(m_dmemory[address + 1]) << 8;
		value |= static_cast<uint32_t>(m_dmemory[address + 2]) << 16;
		value |= static_cast<uint32_t>(m_dmemory[address + 3]) << 24;
	}
	return static_cast<int32_t>(value);
}

// write data in little-endian fashion
void DataMemory::writeData(uint32_t address, uint32_t value, bool memWrite, uint8_t funct3)
{
	if (!memWrite)
		return;

	// always write first 2 bytes for SH and SW
	m_dmemory[address] = static_cast<uint8_t>(value & 0xFF);
	m_dmemory[address + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);

	if (funct3 == 0b010) // write last 2 bytes for SW
	{
		m_dmemory[address + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
		m_dmemory[address + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
	}
}