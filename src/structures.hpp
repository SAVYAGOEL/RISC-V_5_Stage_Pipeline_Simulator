#ifndef STRUCTURES_HPP
#define STRUCTURES_HPP

#include <cstdint>
#include <string>
#include <vector>

// Represents the information decoded from an instruction
struct InstructionInfo {
    uint32_t instruction = 0x00000013; // Original machine code (defaults to NOP)
    uint32_t pc = 0;
    std::string assemblyString = "NOP"; // For output

    // Decoded fields (needed for identification and hazard check)
    uint32_t opcode = 0x13; // NOP opcode
    uint32_t rd = 0;
    uint32_t funct3 = 0;
    uint32_t rs1 = 0;
    uint32_t rs2 = 0;
    uint32_t funct7 = 0;
    int32_t imm = 0;

    // Control Signals (Determined in ID stage)
    // These are crucial for hazard detection and pipeline control
    bool isBranch = false;      // True ONLY for unconditional jumps (jal, jalr) that need flush
    bool memRead = false;       // True if instruction is a load type (e.g., lw, lb)
    bool memWrite = false;      // True if instruction is a store type (e.g., sw, sb)
    bool regWrite = false;      // True if instruction potentially writes to rd
    // bool aluSrc = false;      // No longer strictly needed without ALU
    // int aluOp = 0;          // No longer needed without ALU

    // Values previously read/computed - REMOVED as per simplified requirements
    // int32_t rs1Value = 0;
    // int32_t rs2Value = 0;
    // int32_t aluResult = 0;
    // int32_t memoryData = 0;

    bool isBubble = false;      // Indicates if this latch holds a bubble

    InstructionInfo() : isBubble(true) {} // Default constructor creates a bubble
    InstructionInfo(bool bubble) : isBubble(bubble) {}
};

// Pipeline latches (registers between stages)
struct PipelineLatch {
    InstructionInfo info;
    bool valid = false; // Is the data in the latch valid?
};

#endif // STRUCTURES_HPP