#ifndef PROCESSOR_HPP
#define PROCESSOR_HPP

#include "structures.hpp"
#include <vector>
#include <string>
#include <map>
#include <utility>

const uint32_t STOP_ADDRESS = 0xFFFFFFFF;

class Processor {
public:
    Processor();
    void loadProgramFromFile(const std::string& filename);
    void runSimulation(int totalCycles);
    void displayPipeline();

private:
    uint32_t programCounter;
    std::vector<int32_t> registers; 
    std::map<uint32_t, std::pair<uint32_t, std::string>> programMemory; 

    PipelineStage fetchToDecode;
    PipelineStage decodeToExecute;
    PipelineStage executeToMemory;
    PipelineStage memoryToWriteback;

    std::map<uint32_t, std::vector<std::string>> pipelineHistory; 
    int currentCycleCount;
    int maxCycleLimit;

    bool pausePipeline = false; 
    bool clearFetchDecode = false; 

    // Core pipeline functions
    void fetchInstruction();
    void decodeInstruction();
    void executeInstruction();
    void accessMemory();
    void writeBackToRegisters();

    InstructionDetails interpretInstruction(uint32_t machineCode, uint32_t address);
    void setupRegisters(uint32_t stackPointer = 0x7ffffff0, uint32_t globalPointer = 0x10000000);
    void logStage(uint32_t address, const std::string& stageName);
    bool hasDataHazard(const InstructionDetails& currentInstruction);
};

#endif