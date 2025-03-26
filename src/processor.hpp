#ifndef PROCESSOR_HPP
#define PROCESSOR_HPP

#include "structures.hpp"
#include <vector>
#include <string>
#include <map>
#include <utility> // For std::pair

// Define a halt address
const uint32_t HALT_ADDRESS = 0xFFFFFFFF;

class Processor {
public:
    Processor();
    void loadProgram(const std::string& filename);
    void run(int cycles);
    void printPipelineDiagram();

private:
    uint32_t pc;
    std::vector<int32_t> regs;
    std::map<uint32_t, std::pair<uint32_t, std::string>> instructionMemory;

    PipelineLatch if_id_latch;
    PipelineLatch id_ex_latch;
    PipelineLatch ex_mem_latch;
    PipelineLatch mem_wb_latch;

    std::map<uint32_t, std::vector<std::string>> pipelineTrace;
    int currentCycle;
    int maxCycles;

    bool stallPipeline = false;
    bool flushIFID = false;

    void fetch();
    void decode();
    void execute();
    void memory();
    void writeback();

    InstructionInfo decodeInstruction(uint32_t instr_word, uint32_t current_pc);
    void initializeRegisters(uint32_t sp_val = 0x7ffffff0, uint32_t gp_val = 0x10000000);
    void recordStage(uint32_t instr_pc, const std::string& stage);
    bool checkForHazard(const InstructionInfo& id_instr_potential); // Will be updated
};

#endif // PROCESSOR_HPP