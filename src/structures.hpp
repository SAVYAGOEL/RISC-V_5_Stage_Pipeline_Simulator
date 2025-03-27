
#ifndef STRUCTURES_HPP
#define STRUCTURES_HPP

#include <cstdint>
#include <string>

// Holds details about a decoded instruction
struct InstructionDetails {
    uint32_t machineCode = 0x00000013; // Default to NOP
    uint32_t programCounter = 0;
    std::string assemblyText = "NOP";  // Human-readable instruction

    // Basic instruction fields
    uint32_t opcode = 0x13; // NOP opcode
    uint32_t destReg = 0;   // Destination register (rd)
    uint32_t func3 = 0;     // Function code 3-bit
    uint32_t srcReg1 = 0;   // Source register 1 (rs1)
    uint32_t srcReg2 = 0;   // Source register 2 (rs2)
    uint32_t func7 = 0;     // Function code 7-bit
    int32_t immediate = 0;  // Immediate value

    // Pipeline control signals
    bool isJump = false;    // True for JAL/JALR (unconditional jumps)
    bool readsMemory = false;  // True for load instructions (e.g., lw)
    bool writesMemory = false; // True for store instructions (e.g., sw)
    bool writesRegister = false; // True if instruction updates a register

    bool isEmpty = false;   // Marks this as a pipeline bubble

    // Constructors
    InstructionDetails() : isEmpty(true) {} // Empty bubble by default
    explicit InstructionDetails(bool empty) : isEmpty(empty) {}
};

// Represents a pipeline stage's temporary storage
struct PipelineStage {
    InstructionDetails instruction;
    bool hasData = false; // Indicates if the stage contains valid data
};

#endif // STRUCTURES_HPP