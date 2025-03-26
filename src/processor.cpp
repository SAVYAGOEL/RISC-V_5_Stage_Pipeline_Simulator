#include "processor.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept> // For runtime_error
#include <algorithm> // For std::min

// Constructor, initializeRegisters, loadProgram remain the same

Processor::Processor() : pc(0), regs(32, 0), currentCycle(0), maxCycles(0) {
    initializeRegisters();
    if_id_latch.info = InstructionInfo(true); if_id_latch.valid = false; // Corrected typo
    id_ex_latch.info = InstructionInfo(true); id_ex_latch.valid = false;
    ex_mem_latch.info = InstructionInfo(true); ex_mem_latch.valid = false;
    mem_wb_latch.info = InstructionInfo(true); mem_wb_latch.valid = false;
}

void Processor::initializeRegisters(uint32_t sp_val, uint32_t gp_val) {
    regs.assign(32, 0);
    if (regs.size() > 1) regs[1] = HALT_ADDRESS;
    if (regs.size() > 2) regs[2] = sp_val;
    if (regs.size() > 3) regs[3] = gp_val;
}

void Processor::loadProgram(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) { throw std::runtime_error("Error: Could not open input file " + filename); }
    std::string line; uint32_t current_address = 0; instructionMemory.clear();
    while (getline(file, line)) {
        std::stringstream ss(line); std::string assembly_str; uint32_t machine_code;
        if (!(ss >> std::hex >> machine_code)) {
             if (!line.empty() && line.find_first_not_of(" \t\r\n") != std::string::npos) {
                 std::cerr << "Warning: Could not parse machine code from line: \"" << line << "\"" << std::endl;
             } continue;
        }
        getline(ss, assembly_str); size_t first_char = assembly_str.find_first_not_of(" \t");
        if (std::string::npos != first_char) { assembly_str = assembly_str.substr(first_char); } else { assembly_str = ""; }
        instructionMemory[current_address] = {machine_code, assembly_str}; current_address += 4;
    }
    file.close(); pc = 0;
    std::cout << "Program loaded. Starting PC: 0x" << std::hex << pc << std::dec << ". " << instructionMemory.size() << " instructions." << std::endl;
}

// run(), recordStage remain the same

void Processor::run(int cycles) {
    maxCycles = cycles;
    if (instructionMemory.empty()) { std::cout << "No instructions loaded. Halting." << std::endl; maxCycles = 0; return; }
    for (currentCycle = 0; currentCycle < maxCycles; ++currentCycle) {
        if (pc == HALT_ADDRESS) {
            std::cout << "HALT address (0x" << std::hex << HALT_ADDRESS << std::dec << ") reached. Halting simulation at cycle " << currentCycle << "." << std::endl;
            maxCycles = currentCycle; break;
        }
        stallPipeline = false; // Reset stall signal each cycle
        // Execute stages in reverse order
        writeback(); memory(); execute(); decode(); fetch();
        // Check for empty pipeline termination
         bool pipelineEmpty = !if_id_latch.valid && !id_ex_latch.valid && !ex_mem_latch.valid && !mem_wb_latch.valid;
         if (pipelineEmpty) {
             uint32_t max_addr = 0; if (!instructionMemory.empty()) { max_addr = instructionMemory.rbegin()->first; }
             if (pc > max_addr && pc != HALT_ADDRESS) {
                std::cout << "Pipeline empty and PC (0x" << std::hex << pc << std::dec << ") beyond last instruction. Halting early at cycle " << currentCycle + 1 << "." << std::endl;
                maxCycles = currentCycle + 1; break;
             }
         }
    }
    maxCycles = std::min(maxCycles, currentCycle); // Adjust maxCycles if halted early
}

void Processor::recordStage(uint32_t instr_pc, const std::string& stage) {
    if (currentCycle >= maxCycles) return;
    if (pipelineTrace[instr_pc].size() <= static_cast<size_t>(currentCycle)) {
        pipelineTrace[instr_pc].resize(static_cast<size_t>(currentCycle) + 1, "-");
    }
    if (static_cast<size_t>(currentCycle) < pipelineTrace[instr_pc].size()) {
         pipelineTrace[instr_pc][static_cast<size_t>(currentCycle)] = stage;
    }
}

// Fetch stage remains the same (correctly handles stallPipeline)

void Processor::fetch() {
    if (currentCycle >= maxCycles) return;
    uint32_t pc_to_fetch = pc;
    bool record_if = false; std::string current_assembly_str = "NOP"; uint32_t current_instr_word = 0x13;
    if (instructionMemory.count(pc_to_fetch)) {
         record_if = true; const auto& instr_data = instructionMemory.at(pc_to_fetch);
         current_instr_word = instr_data.first; current_assembly_str = instr_data.second;
         recordStage(pc_to_fetch, "IF");
    } else if (pc_to_fetch != HALT_ADDRESS) { /* Handle out of bounds PC */ }
    if (stallPipeline) { return; } // <<< If ID is stalled, Fetch does NOTHING else this cycle (no latch update, no PC update)
    if (record_if) {
        if_id_latch.info = InstructionInfo(false); if_id_latch.info.instruction = current_instr_word;
        if_id_latch.info.assemblyString = current_assembly_str; if_id_latch.info.pc = pc_to_fetch;
        if_id_latch.valid = true;
        pc = pc_to_fetch + 4; // Update PC for NEXT fetch ONLY if not stalled and no jump taken
    } else {
        if_id_latch.info = InstructionInfo(true); if_id_latch.valid = false;
    }
}

// checkForHazard remains the same

bool Processor::checkForHazard(const InstructionInfo& id_instr_potential) {
    uint32_t inst = id_instr_potential.instruction;
    uint32_t rs1_addr = (inst >> 15) & 0x1F; uint32_t rs2_addr = (inst >> 20) & 0x1F;
    bool need_rs1 = (rs1_addr != 0); bool need_rs2 = (rs2_addr != 0);
    // Check against instruction completing EX stage (in ex_mem_latch)
    if (ex_mem_latch.valid && !ex_mem_latch.info.isBubble && ex_mem_latch.info.regWrite && ex_mem_latch.info.rd != 0) {
        if ((need_rs1 && ex_mem_latch.info.rd == rs1_addr) || (need_rs2 && ex_mem_latch.info.rd == rs2_addr)) { return true; }
    }
    // Check against instruction completing MEM stage (in mem_wb_latch)
    if (mem_wb_latch.valid && !mem_wb_latch.info.isBubble && mem_wb_latch.info.regWrite && mem_wb_latch.info.rd != 0) {
         if ((need_rs1 && mem_wb_latch.info.rd == rs1_addr) || (need_rs2 && mem_wb_latch.info.rd == rs2_addr)) { return true; }
    }
    return false; // No hazard detected
}

// decodeInstruction remains the same

InstructionInfo Processor::decodeInstruction(uint32_t instr_word, uint32_t current_pc) {
    // ... (Code is identical to the previous correct version) ...
    // ... (Sets flags: regWrite, memRead, memWrite, isBranch(for jal/jalr)) ...
     InstructionInfo info(false); info.instruction = instr_word; info.pc = current_pc;
    info.opcode = instr_word & 0x7F; info.rd = (instr_word >> 7) & 0x1F; info.funct3 = (instr_word >> 12) & 0x7;
    info.rs1 = (instr_word >> 15) & 0x1F; info.rs2 = (instr_word >> 20) & 0x1F; info.funct7 = (instr_word >> 25) & 0x7F;
    info.regWrite = false; info.memRead = false; info.memWrite = false; info.isBranch = false; // isBranch for unconditional jumps only

    switch (info.opcode) {
        case 0x33: info.regWrite = (info.rd != 0); break; // R-type. Check rd != 0
        case 0x13: if (instr_word == 0x00000013) { info = InstructionInfo(true); break; } info.regWrite = (info.rd != 0); info.imm = (int32_t)(instr_word & 0xFFF00000) >> 20; break; // I-type (ADDI etc), NOP
        case 0x03: info.regWrite = (info.rd != 0); info.memRead = true; info.imm = (int32_t)(instr_word & 0xFFF00000) >> 20; break; // I-type Load
        case 0x23: info.memWrite = true; info.imm = ((int32_t)(instr_word & 0xFE000000) >> 20) | ((instr_word >> 7) & 0x1F); if (info.imm & 0x800) info.imm |= 0xFFFFF000; break; // S-type Store
        case 0x63: /* BEQ - No flush needed */ info.imm = (((int32_t)(instr_word & 0x80000000) >> 19)) | ((instr_word & 0x80) << 4) | ((instr_word >> 20) & 0x7E0) | ((instr_word >> 7) & 0x1E); if (info.imm & 0x1000) info.imm |= 0xFFFFE000; break; // B-type
        case 0x6F: info.regWrite = (info.rd != 0); info.isBranch = true; /* JAL - Flush needed */ info.imm = (((int32_t)(instr_word & 0x80000000) >> 11)) | (instr_word & 0xFF000) | ((instr_word >> 9) & 0x800) | ((instr_word >> 20) & 0x7FE); if (info.imm & 0x100000) info.imm |= 0xFFE00000; break; // J-type
        case 0x67: info.regWrite = (info.rd != 0); info.isBranch = true; /* JALR - Flush needed */ info.imm = (int32_t)(instr_word & 0xFFF00000) >> 20; break; // I-type
        case 0x00: if (instr_word == 0x00000000) { info = InstructionInfo(true); } else { info = InstructionInfo(true); } break; // Zero instr as NOP
        default: std::cerr << "Warning: Unsupported opcode: 0x" << std::hex << info.opcode << std::dec << " (Instruction: 0x" << instr_word << ")" << std::endl; info = InstructionInfo(true); break;
    }
    return info;
}


// --- Decode Stage (REVERTED to STANDARD STALL) ---
void Processor::decode() {
    if (currentCycle >= maxCycles) return;

    uint32_t original_pc = if_id_latch.valid ? if_id_latch.info.pc : 0;
    bool was_valid_and_not_bubble = if_id_latch.valid && !if_id_latch.info.isBubble;

    // --- Handle Flush ---
    if (flushIFID) {
        if (was_valid_and_not_bubble) { recordStage(original_pc, "-"); } // Record flush
        if_id_latch.valid = false; if_id_latch.info = InstructionInfo(true);
        flushIFID = false; was_valid_and_not_bubble = false;
    }

    // --- Propagate Bubble / Invalid Latch ---
    if (!was_valid_and_not_bubble) {
        id_ex_latch.info = InstructionInfo(true); id_ex_latch.valid = false; return;
    }

    InstructionInfo current_instr_info = if_id_latch.info;

    // --- Standard Hazard Detection & Stall ---
    // *** REVERTED LOGIC START ***
    if (checkForHazard(current_instr_info)) { // Check if ANY RAW hazard exists
        stallPipeline = true; // Signal Fetch and IF/ID to hold
        id_ex_latch.info = InstructionInfo(true); // Insert bubble into ID/EX
        id_ex_latch.valid = false;
        recordStage(current_instr_info.pc, "-"); // Record stall in this cycle's ID slot
        return; // Stall!
    }
    // *** REVERTED LOGIC END ***

    // --- No Stall / No Flush - Proceed with Decode ---
    id_ex_latch.valid = true;
    id_ex_latch.info = decodeInstruction(current_instr_info.instruction, current_instr_info.pc);
    id_ex_latch.info.assemblyString = current_instr_info.assemblyString;

    if (id_ex_latch.info.isBubble) { // Handle decode error
        id_ex_latch.valid = false;
        recordStage(id_ex_latch.info.pc, "ERR"); return;
    }

    // Record ID stage completion
    recordStage(id_ex_latch.info.pc, "ID");
}


// execute(), memory(), writeback(), printPipelineDiagram() remain the same

void Processor::execute() {
    if (currentCycle >= maxCycles) return;
    if (!id_ex_latch.valid || id_ex_latch.info.isBubble) {
        ex_mem_latch.info = InstructionInfo(true); ex_mem_latch.valid = false; return;
    }
    ex_mem_latch.valid = true; ex_mem_latch.info = id_ex_latch.info;
    InstructionInfo& info = ex_mem_latch.info;

    if (info.isBranch) { // Set only for JAL/JALR
        flushIFID = true;
        if (info.opcode == 0x6F) { pc = info.pc + info.imm; }
        else if (info.opcode == 0x67) {
            if (info.rs1 == 1 && info.imm == 0) { pc = HALT_ADDRESS; } else { pc = (0 + info.imm) & ~1U; }
        }
    }
    recordStage(info.pc, "EX");
}

void Processor::memory() {
    if (currentCycle >= maxCycles) return;
    if (!ex_mem_latch.valid || ex_mem_latch.info.isBubble) {
        mem_wb_latch.info = InstructionInfo(true); mem_wb_latch.valid = false; return;
    }
    mem_wb_latch.valid = true; mem_wb_latch.info = ex_mem_latch.info;
    recordStage(mem_wb_latch.info.pc, "MEM");
}

void Processor::writeback() {
    if (currentCycle >= maxCycles) return;
    if (!mem_wb_latch.valid || mem_wb_latch.info.isBubble) { return; }
    recordStage(mem_wb_latch.info.pc, "WB");
}

void Processor::printPipelineDiagram() {
    std::cout << "\nPipeline Diagram (" << maxCycles << " cycles):\n";
    std::vector<std::pair<uint32_t, std::string>> sorted_instructions;
    for (const auto& pair : instructionMemory) { sorted_instructions.push_back({pair.first, pair.second.second}); }
    std::sort(sorted_instructions.begin(), sorted_instructions.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (const auto& instr_pair : sorted_instructions) {
        uint32_t instr_pc = instr_pair.first; const std::string& assembly_str = instr_pair.second;
        if (pipelineTrace.count(instr_pc)) {
            std::cout << std::left << std::setw(20) << assembly_str.substr(0,19); // Limit assembly str width
             const auto& trace = pipelineTrace.at(instr_pc);
            for (int cycle = 0; cycle < maxCycles; ++cycle) {
                std::cout << ";";
                if (static_cast<size_t>(cycle) < trace.size()) { std::cout << std::setw(3) << std::left << trace[static_cast<size_t>(cycle)]; }
                else { std::cout << std::setw(3) << std::left << "-"; }
            }
            std::cout << std::endl;
        }
    }
     std::cout << "\nFinal Register State (Simplified - Values mostly unused):\n";
     std::cout << std::hex; // Set output to hexadecimal
     for(size_t i = 0; i < regs.size(); ++i) {
         if (i % 4 == 0 && i != 0) std::cout << "\n"; // 4 registers per line
         std::cout << "x" << std::setw(2) << std::dec << i << ": " // Print register number in decimal
                   << std::setw(11) << std::hex << "0x" << regs[i] << "  "; // Print value in hex
     }
     std::cout << std::dec << std::endl; // Reset to decimal output
}