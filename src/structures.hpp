#ifndef STRUCTURES_HPP
#define STRUCTURES_HPP

#include <cstdint>
#include <string>

struct InstructionDetails {
    uint32_t machineCode = 0x00000013; 
    uint32_t programCounter = 0;
    std::string assemblyText = "NOP"; 

    uint32_t opcode = 0x13; 
    uint32_t destReg = 0; 
    uint32_t func3 = 0;   
    uint32_t srcReg1 = 0;  
    uint32_t srcReg2 = 0;  
    uint32_t func7 = 0;   
    int32_t immediate = 0; 

    bool isJump = false;    // True for JAL/JALR (unconditional jumps)
    bool readsMemory = false;  // True for load instructions (e.g., lw)
    bool writesMemory = false; // True for store instructions (e.g., sw)
    bool writesRegister = false; // True if instruction updates a register

    bool isEmpty = false;   // Marks this as a pipeline bubble

    InstructionDetails() : isEmpty(true) {}
    explicit InstructionDetails(bool empty) : isEmpty(empty) {}
};

struct PipelineStage {
    InstructionDetails instruction;
    bool hasData = false;
};

#endif