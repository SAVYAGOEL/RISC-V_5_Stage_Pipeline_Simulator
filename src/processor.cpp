#include "processor.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

Processor::Processor() 
    : programCounter(0), registers(32, 0), currentCycleCount(0), maxCycleLimit(0) {
    setupRegisters();
    fetchToDecode.instruction = InstructionDetails(true);
    decodeToExecute.instruction = InstructionDetails(true);
    executeToMemory.instruction = InstructionDetails(true);
    memoryToWriteback.instruction = InstructionDetails(true);
}

void Processor::setupRegisters(uint32_t stackPointer, uint32_t globalPointer) {
    registers.assign(32, 0);
    if (registers.size() > 1) registers[1] = STOP_ADDRESS; // ra (return address)
    if (registers.size() > 2) registers[2] = stackPointer;  // sp
    if (registers.size() > 3) registers[3] = globalPointer; // gp
}

void Processor::loadProgramFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }

    std::string line;
    uint32_t address = 0;
    programMemory.clear();

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        uint32_t machineCode;
        std::string assembly;

        if (!(ss >> std::hex >> machineCode)) {
            if (!line.empty() && line.find_first_not_of(" \t\r\n") != std::string::npos) {
                std::cerr << "Warning: Skipping invalid line: \"" << line << "\"" << std::endl;
            }
            continue;
        }

        std::getline(ss, assembly);
        assembly.erase(0, assembly.find_first_not_of(" \t"));
        programMemory[address] = std::make_pair(machineCode, assembly.empty() ? "NOP" : assembly);
        address += 4;
    }

    file.close();
    programCounter = 0;
    std::cout << "Loaded " << programMemory.size() << " instructions. Starting at 0x" 
              << std::hex << programCounter << std::dec << "." << std::endl;
}

void Processor::runSimulation(int totalCycles) {
    maxCycleLimit = totalCycles;
    if (programMemory.empty()) {
        std::cout << "No program loaded. Stopping." << std::endl;
        maxCycleLimit = 0;
        return;
    }

    for (currentCycleCount = 0; currentCycleCount < maxCycleLimit; ++currentCycleCount) {
        if (programCounter == STOP_ADDRESS) {
            std::cout << "Reached STOP_ADDRESS (0x" << std::hex << STOP_ADDRESS 
                      << std::dec << ") at cycle " << currentCycleCount << "." << std::endl;
            maxCycleLimit = currentCycleCount;
            break;
        }

        pausePipeline = false;
        writeBackToRegisters();
        accessMemory();
        executeInstruction();
        decodeInstruction();
        fetchInstruction();

        bool isPipelineEmpty = !fetchToDecode.hasData && !decodeToExecute.hasData && 
                              !executeToMemory.hasData && !memoryToWriteback.hasData;
        if (isPipelineEmpty) {
            uint32_t lastAddress = programMemory.empty() ? 0 : programMemory.rbegin()->first;
            if (programCounter > lastAddress && programCounter != STOP_ADDRESS) {
                std::cout << "Pipeline empty and PC (0x" << std::hex << programCounter 
                          << std::dec << ") past end. Stopping at cycle " << currentCycleCount + 1 << "." << std::endl;
                maxCycleLimit = currentCycleCount + 1;
                break;
            }
        }
    }
    maxCycleLimit = std::min(maxCycleLimit, currentCycleCount);
}

void Processor::logStage(uint32_t address, const std::string& stageName) {
    if (currentCycleCount >= maxCycleLimit) return;
    auto& history = pipelineHistory[address];
    if (history.size() <= static_cast<size_t>(currentCycleCount)) {
        history.resize(currentCycleCount + 1, "-");
    }
    history[currentCycleCount] = stageName;
}

void Processor::fetchInstruction() {
    if (currentCycleCount >= maxCycleLimit) return;
    uint32_t currentAddress = programCounter;
    bool canFetch = programMemory.count(currentAddress) > 0;

    if (pausePipeline) {
        if (canFetch) logStage(currentAddress, "-");
        return;
    }

    if (canFetch) {
        auto [code, text] = programMemory[currentAddress];
        fetchToDecode.instruction = InstructionDetails(false);
        fetchToDecode.instruction.machineCode = code;
        fetchToDecode.instruction.assemblyText = text;
        fetchToDecode.instruction.programCounter = currentAddress;
        fetchToDecode.hasData = true;
        logStage(currentAddress, "IF");
        programCounter += 4;
    } else {
        fetchToDecode.instruction = InstructionDetails(true);
        fetchToDecode.hasData = false;
    }
}

void Processor::decodeInstruction() {
    if (currentCycleCount >= maxCycleLimit) return;
    uint32_t instructionAddress = fetchToDecode.hasData ? fetchToDecode.instruction.programCounter : 0;
    bool hasValidInstruction = fetchToDecode.hasData && !fetchToDecode.instruction.isEmpty;

    if (clearFetchDecode) {
        if (hasValidInstruction) logStage(instructionAddress, "-");
        fetchToDecode.instruction = InstructionDetails(true);
        fetchToDecode.hasData = false;
        clearFetchDecode = false;
        hasValidInstruction = false;
    }

    if (!hasValidInstruction) {
        decodeToExecute.instruction = InstructionDetails(true);
        decodeToExecute.hasData = false;
        return;
    }

    InstructionDetails current = fetchToDecode.instruction;
    if (hasDataHazard(current)) {
        pausePipeline = true;
        decodeToExecute.instruction = InstructionDetails(true);
        decodeToExecute.hasData = false;
        logStage(current.programCounter, "-");
        return;
    }

    decodeToExecute.instruction = interpretInstruction(current.machineCode, current.programCounter);
    decodeToExecute.instruction.assemblyText = current.assemblyText;
    decodeToExecute.hasData = !decodeToExecute.instruction.isEmpty;
    if (decodeToExecute.hasData) {
        logStage(decodeToExecute.instruction.programCounter, "ID");
    }
}

bool Processor::hasDataHazard(const InstructionDetails& current) {
    uint32_t code = current.machineCode;
    uint32_t reg1 = (code >> 15) & 0x1F; // rs1
    uint32_t reg2 = (code >> 20) & 0x1F; // rs2
    bool usesReg1 = reg1 != 0;
    bool usesReg2 = reg2 != 0;

    if (executeToMemory.hasData && !executeToMemory.instruction.isEmpty && 
        executeToMemory.instruction.writesRegister) {
        uint32_t targetReg = executeToMemory.instruction.destReg;
        if (targetReg != 0 && ((usesReg1 && targetReg == reg1) || (usesReg2 && targetReg == reg2))) {
            return true;
        }
    }

    if (memoryToWriteback.hasData && !memoryToWriteback.instruction.isEmpty && 
        memoryToWriteback.instruction.writesRegister) {
        uint32_t targetReg = memoryToWriteback.instruction.destReg;
        if (targetReg != 0 && ((usesReg1 && targetReg == reg1) || (usesReg2 && targetReg == reg2))) {
            return true;
        }
    }
    return false;
}

InstructionDetails Processor::interpretInstruction(uint32_t machineCode, uint32_t address) {
    InstructionDetails details(false);
    details.machineCode = machineCode;
    details.programCounter = address;

    // Extract instruction fields
    details.opcode = machineCode & 0x7F;
    details.destReg = (machineCode >> 7) & 0x1F;
    details.func3 = (machineCode >> 12) & 0x7;
    details.srcReg1 = (machineCode >> 15) & 0x1F;
    details.srcReg2 = (machineCode >> 20) & 0x1F;
    details.func7 = (machineCode >> 25) & 0x7F;

    switch (details.opcode) {
        case 0x33: // R-type (e.g., ADD, SUB)
            details.writesRegister = (details.destReg != 0);
            break;

        case 0x13: // I-type ALU (e.g., ADDI)
            if (machineCode == 0x00000013) return InstructionDetails(true); // NOP
            details.writesRegister = (details.destReg != 0);
            details.immediate = (int32_t)(machineCode & 0xFFF00000) >> 20;
            break;

        case 0x03: // I-type Load (e.g., LW)
            details.writesRegister = (details.destReg != 0);
            details.readsMemory = true;
            details.immediate = (int32_t)(machineCode & 0xFFF00000) >> 20;
            break;

        case 0x23: // S-type Store (e.g., SW)
            details.writesMemory = true;
            details.immediate = ((int32_t)(machineCode & 0xFE000000) >> 20) | 
                               ((machineCode >> 7) & 0x1F);
            if (details.immediate & 0x800) details.immediate |= 0xFFFFF000;
            break;

        case 0x63: // B-type Branch (e.g., BEQ) - Not taken in this sim
            details.immediate = (((int32_t)(machineCode & 0x80000000) >> 19)) | 
                               ((machineCode & 0x80) << 4) | 
                               ((machineCode >> 20) & 0x7E0) | 
                               ((machineCode >> 7) & 0x1E);
            if (details.immediate & 0x1000) details.immediate |= 0xFFFFE000;
            break;

        case 0x6F: // J-type JAL
            details.writesRegister = (details.destReg != 0);
            details.isJump = true;
            details.immediate = (((int32_t)(machineCode & 0x80000000) >> 11)) | 
                               (machineCode & 0xFF000) | 
                               ((machineCode >> 9) & 0x800) | 
                               ((machineCode >> 20) & 0x7FE);
            if (details.immediate & 0x100000) details.immediate |= 0xFFE00000;
            break;

        case 0x67: // I-type JALR
            if (details.func3 == 0x0) {
                details.writesRegister = (details.destReg != 0);
                details.isJump = true;
                details.immediate = (int32_t)(machineCode & 0xFFF00000) >> 20;
            } else {
                std::cerr << "Warning: Unknown JALR funct3 at 0x" << std::hex << address << std::dec << std::endl;
                return InstructionDetails(true);
            }
            break;

        case 0x37: // U-type LUI
            details.writesRegister = (details.destReg != 0);
            details.immediate = (int32_t)(machineCode & 0xFFFFF000);
            break;

        case 0x17: // U-type AUIPC
            details.writesRegister = (details.destReg != 0);
            details.immediate = (int32_t)(machineCode & 0xFFFFF000);
            break;

        case 0x73: // System (ECALL/EBREAK)
        case 0x0F: // Fence
        case 0x00: // Zero instruction
            return InstructionDetails(true);

        default:
            std::cerr << "Warning: Unknown opcode 0x" << std::hex << details.opcode 
                      << " at 0x" << address << std::dec << std::endl;
            return InstructionDetails(true);
    }
    return details;
}

void Processor::executeInstruction() {
    if (currentCycleCount >= maxCycleLimit) return;
    if (!decodeToExecute.hasData || decodeToExecute.instruction.isEmpty) {
        executeToMemory.instruction = InstructionDetails(true);
        executeToMemory.hasData = false;
        return;
    }

    executeToMemory = decodeToExecute;
    InstructionDetails& current = executeToMemory.instruction;

    if (current.isJump) {
        clearFetchDecode = true;
        if (current.opcode == 0x6F) { // JAL
            programCounter = current.programCounter + current.immediate;
        } else if (current.opcode == 0x67) { // JALR
            if (current.srcReg1 == 1 && current.immediate == 0 && current.destReg == 0) {
                programCounter = STOP_ADDRESS;
            } else {
                programCounter = (0 + current.immediate) & ~1U;
            }
        }
    }
    logStage(current.programCounter, "EX");
}

void Processor::accessMemory() {
    if (currentCycleCount >= maxCycleLimit) return;
    if (!executeToMemory.hasData || executeToMemory.instruction.isEmpty) {
        memoryToWriteback.instruction = InstructionDetails(true);
        memoryToWriteback.hasData = false;
        return;
    }

    memoryToWriteback = executeToMemory;
    logStage(memoryToWriteback.instruction.programCounter, "MEM");
}

void Processor::writeBackToRegisters() {
    if (currentCycleCount >= maxCycleLimit) return;
    if (!memoryToWriteback.hasData || memoryToWriteback.instruction.isEmpty) return;
    logStage(memoryToWriteback.instruction.programCounter, "WB");
}

void Processor::displayPipeline() {
    std::cout << "\nPipeline Diagram (" << maxCycleLimit << " cycles):\n";
    std::vector<std::pair<uint32_t, std::string> > instructions;
    for (const auto& [addr, data] : programMemory) {
        instructions.emplace_back(addr, data.second);
    }
    std::sort(instructions.begin(), instructions.end());

    std::cout << std::left << std::setw(20) << "Instruction";
    for (size_t cycle = 0; cycle < static_cast<size_t>(maxCycleLimit); ++cycle) {
        std::cout << ";" << std::setw(3) << std::right << cycle;
    }
    std::cout << std::endl;

    for (const auto& [addr, text] : instructions) {
        if (pipelineHistory.count(addr)) {
            std::cout << std::left << std::setw(20) << text.substr(0, 19);
            const auto& stages = pipelineHistory[addr];
            for (size_t cycle = 0; cycle < static_cast<size_t>(maxCycleLimit); ++cycle) {
                std::cout << ";";
                std::cout << std::setw(3) << std::left 
                          << (cycle < stages.size() ? stages[cycle] : "-");
            }
            std::cout << std::endl;
        }
    }

    std::cout << "\nFinal Register State:\n" << std::hex;
    for (size_t i = 0; i < registers.size(); ++i) {
        if (i % 4 == 0 && i != 0) std::cout << "\n";
        std::cout << "x" << std::setw(2) << std::dec << i << ": " 
                  << std::setw(11) << std::hex << "0x" << registers[i] << "  ";
    }
    std::cout << std::dec << std::endl;
}