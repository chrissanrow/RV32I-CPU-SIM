#include "CPU.h"

#include <iostream>
#include <bitset>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>

int main(int argc, char *argv[])
{
	// Read instruction memory from given file
	char instMem[4096];

	if (argc < 2)
	{
		// std::cout << "No file name entered. Exiting...";
		return -1;
	}

	std::ifstream infile(argv[1]); // open the file
	if (!(infile.is_open() && infile.good()))
	{
		std::cout << "error opening file\n";
		return 0;
	}

	std::string line;
	int i = 0;
	while (infile >> line && i < 4096)
	{
		unsigned int byte;
		std::stringstream ss;
		ss << std::hex << line;
		ss >> byte;

		instMem[i] = static_cast<char>(byte);
		i++;
	}
	int maxPC = i;
	// std::cout << "MAXPC: " << maxPC << std::endl;

	CPU myCPU(instMem);

	bool done = true;
	while (done == true) // Processor's main loop. Each iteration is equal to one clock cycle.
	{
		// ----FETCH----

		// instruction fetch from instruction memory
		auto cycle_pc = myCPU.readPC();
		auto instruction = myCPU.fetchInstruction(cycle_pc);
		std::bitset<32> instBits(instruction);
		// std::cout << "PC: " << cycle_pc << " Instruction: " << instBits << std::endl;

		// ----DECODE----
		auto opcode = getBitRange(instBits, 6, 0);
		if (opcode.to_ulong() == 0)
		{
			break; // halt on opcode 0
		}
		auto rs1 = getBitRange(instBits, 19, 15);
		auto rs2 = getBitRange(instBits, 24, 20);
		auto rd = getBitRange(instBits, 11, 7);
		auto funct3 = getBitRange(instBits, 14, 12);
		auto funct7 = instBits[30];

		// REGISTER FILE
		auto rs1_val = myCPU.registerFile.readRegister(static_cast<int>(rs1.to_ulong()));
		auto rs2_val = myCPU.registerFile.readRegister(static_cast<int>(rs2.to_ulong()));
		// std::cout << "PC: " << cycle_pc << " -> ";
		//  print instruction in hex with leading 0s
		// std::cout << "Instruction: " << std::hex << std::setfill('0') << std::setw(8) << instruction << std::dec << std::endl;
		// std::cout << "rd: " << rd.to_ulong() << " | ";
		// std::cout << "x" << rs1.to_ulong() << " value: " << rs1_val << " | " << "x" << rs2.to_ulong() << " value: " << rs2_val << " | ";

		//  IMMEDIATE GENERATOR
		auto imm = immediateGenerator(instBits);
		// std::cout << "Immediate: " << imm << std::endl;

		// CONTROLLER

		auto controlSignals = myCPU.controller.getControlSignals(static_cast<uint8_t>(opcode.to_ulong()));

		// ----EXECUTE----

		// ALU CONTROL

		auto aluControlSignal = myCPU.aluControl.getALUOperation(controlSignals.aluOp, static_cast<uint8_t>(funct3.to_ulong()), funct7);

		// std::cout << "ALU Control Signal: " << static_cast<int>(aluControlSignal) << std::endl;

		// determine inputs for ALU

		auto aluInput1 = rs1_val;
		auto aluInput2 = twoInputMux(controlSignals.aluSrc, rs2_val, imm);

		// std::cout << "ALU Input1: " << aluInput1 << " | ALU Input2: " << aluInput2 << std::endl;

		// ALU EXECUTION

		auto [aluResult, zeroFlag] = myCPU.alu.execute(aluInput1, aluInput2, aluControlSignal, controlSignals.lui);

		// ----MEMORY ACCESS----

		auto memReadData = myCPU.dataMemory.readData(static_cast<uint32_t>(aluResult), controlSignals.memRead, static_cast<uint8_t>(funct3.to_ulong()));
		myCPU.dataMemory.writeData(static_cast<uint32_t>(aluResult), static_cast<uint32_t>(rs2_val), controlSignals.memWrite, static_cast<uint8_t>(funct3.to_ulong()));

		// select read memory or ALU result to write back
		auto wbData = twoInputMux(controlSignals.memToReg, aluResult, static_cast<int32_t>(memReadData));
		// select jump and link data (PC + 4) or wbData to write back
		wbData = twoInputMux(controlSignals.jalr, wbData, static_cast<int32_t>(cycle_pc + 4));

		// ----WRITE BACK----

		myCPU.registerFile.writeRegister(static_cast<int>(rd.to_ulong()), wbData, controlSignals.regWrite);

		// compute PC + immediate for branch/jump target
		auto branchTarget = static_cast<uint32_t>(cycle_pc) + imm;
		auto jumpTarget = static_cast<uint32_t>(rs1_val) + imm;
		// branch if bne and zero flag is false
		if (controlSignals.branch && !zeroFlag)
		{
			myCPU.setPC(branchTarget);
		}
		// jump if jalr
		else if (controlSignals.jalr)
		{
			myCPU.setPC(jumpTarget);
		}
		else
		{
			myCPU.setPC(cycle_pc + 4);
		}

		// std::cout << "-----" << std::endl;
		if (myCPU.readPC() >= maxPC)
			break;
	}
	int a0 = myCPU.registerFile.readRegister(10);
	int a1 = myCPU.registerFile.readRegister(11);
	std::cout << "(" << a0 << "," << a1 << ")" << std::endl;

	return 0;
}