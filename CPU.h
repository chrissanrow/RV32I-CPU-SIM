#include <iostream>
#include <bitset>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <cstdint>
#include <vector>

/*----Enums / Structs----*/

enum ALUOperation
{
	ADD = 0,
	SUB = 1,
	AND = 2,
	OR = 3,
	SRA = 4,
	SLTIU = 5,
	NOOP = 6
};

enum InstructionType
{
	R_TYPE,
	I_TYPE,
	S_TYPE,
	B_TYPE,
	U_TYPE,
	J_TYPE,
	UNKNOWN_TYPE
};

struct ControlSignals
{
	bool jalr;
	bool lui;
	bool branch;
	bool memRead;
	bool memToReg;
	uint8_t aluOp;
	bool memWrite;
	bool aluSrc;
	bool regWrite;
};

/*----Util/Common Functions----*/

int32_t twoInputMux(bool select, int32_t input0, int32_t input1);

template <size_t N>
std::bitset<N> getBitRange(const std::bitset<N> &bits, size_t high, size_t low);

std::bitset<32> signExtend(const std::bitset<32> &input, size_t originalSize);

/*----Functions----*/

InstructionType getInstructionType(std::bitset<32> instruction);

int32_t immediateGenerator(std::bitset<32> instruction);

/*----CPU Modules----*/

// CAREFUL: consider signed vs unsigned for register file -> might need to change
class RegisterFile
{
public:
	RegisterFile();
	int32_t readRegister(int index) const;
	void writeRegister(int index, int32_t value, bool regWrite);

private:
	int32_t m_registers[32]; // 32 registers
};

class Controller
{
public:
	ControlSignals getControlSignals(uint8_t opcode);

private:
};

class ALUControl
{
public:
	ALUOperation getALUOperation(uint8_t funct3, uint8_t funct7, uint8_t ALUOp);

private:
};

class ALU
{
public:
	// return result of ALU op and zero flag
	std::pair<int32_t, bool> execute(int32_t operand1, int32_t operand2, ALUOperation operation, bool isLUI);

private:
};

class DataMemory
{
public:
	DataMemory();
	int32_t readData(uint32_t address, bool memRead, uint8_t funct3);
	void writeData(uint32_t address, uint32_t value, bool memWrite, uint8_t funct3);

private:
	std::vector<uint8_t> m_dmemory; // data memory byte addressable in little endian fashion;
};

/*----CPU Top-Level----*/

class CPU
{
public:
	CPU(const char instrMem[4096]);
	unsigned long readPC();
	void incPC();
	void setPC(unsigned long newPC);
	uint32_t fetchInstruction(unsigned long PC);

	RegisterFile registerFile;
	Controller controller;
	ALUControl aluControl;
	ALU alu;
	DataMemory dataMemory;

private:
	uint8_t m_instMem[4096]; // instruction memory byte addressable in little endian fashion;
	unsigned long m_PC;		 // pc
};
