#include "noforwarding.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

std::vector<int> reg(NUM_REGS, 0);
int mem[1024] = {0};

uint32_t IF::fetch() {
    if (pc < instr_mem.size() * 4) {
        uint32_t instruction = instr_mem[pc / 4];
        pc += 4;
        return instruction;
    }
    return 0; // NOP
}

int EX::out() {
    switch (op) {
        case 0: return a1 + a2;        // ADD
        case 1: return a1 - a2;        // SUB
        case 2: return a1 & a2;        // AND
        case 3: return a1 | a2;        // OR
        case 4: return a1 << (a2 & 0x1F); // SLL
        case 5: return ~a1;            // NOT
        default: return 0;
    }
}

int ID::out(int a1, int a2) {
    if (!branch_decision) return 0;
    if (dec == 0) return (a1 == a2) ? 1 : 0; // beq
    else if (dec == 1) return (a1 < a2) ? 1 : 0; // blt
    else if (dec == 2) return (a1 <= a2) ? 1 : 0; // ble
    else if (dec == 3) return (a1 > a2) ? 1 : 0; // bgt
    else if (dec == 4) return (a1 >= a2) ? 1 : 0; // bge
    else if (dec == 5) return (a1 == 0) ? 1 : 0; // beqz
    return 0;
}

int MEM::putget() {
    if (op == 1) { // Load byte
        reg[reg_number] = mem[mem_addr / 4] & 0xFF;
        return reg[reg_number];
    }
    else if (op == 2) { // Store word
        mem[mem_addr / 4] = reg[reg_number];
        return 0;
    }
    return 0;
}

void WB::write() {
    if (op == 1 && reg_number != 0) {
        reg[reg_number] = value;
    }
}

NoForwardingProcessor::NoForwardingProcessor(std::vector<uint32_t>& instructions)
    : if_stage(instructions), stall(false) {}

bool NoForwardingProcessor::detectHazardID() {
    if (id_ex.control.branch && id_ex.rs1 != 0 && (
        (ex_mem.control.reg_write && ex_mem.rd == id_ex.rs1) ||
        (mem_wb.reg_write && mem_wb.rd == id_ex.rs1))) {
        return true;
    }
    if (id_ex.control.branch && id_ex.rs2 != 0 && (
        (ex_mem.control.reg_write && ex_mem.rd == id_ex.rs2) ||
        (mem_wb.reg_write && mem_wb.rd == id_ex.rs2))) {
        return true;
    }
    return false;
}

bool NoForwardingProcessor::detectHazardEX() {
    if (id_ex.rs1 != 0 && (
        (ex_mem.control.reg_write && ex_mem.rd == id_ex.rs1) ||
        (mem_wb.reg_write && mem_wb.rd == id_ex.rs1))) {
        return true;
    }
    if (id_ex.rs2 != 0 && (
        (ex_mem.control.reg_write && ex_mem.rd == id_ex.rs2) ||
        (mem_wb.reg_write && mem_wb.rd == id_ex.rs2))) {
        return true;
    }
    return false;
}

bool NoForwardingProcessor::detectHazardMEM() {
    if (ex_mem.control.mem_write && ex_mem.rs2 != 0 && 
        (mem_wb.reg_write && mem_wb.rd == ex_mem.rs2)) {
        return true;
    }
    return false;
}

std::string NoForwardingProcessor::decodeMnemonic(uint32_t inst) {
    int opcode = inst & 0x7F;
    int funct3 = (inst >> 12) & 0x7;
    int funct7 = (inst >> 25) & 0x7F;
    int rd = (inst >> 7) & 0x1F;
    int rs1 = (inst >> 15) & 0x1F;
    int rs2 = (inst >> 20) & 0x1F;
    std::stringstream ss;

    if (opcode == 0x33) { // R-type
        if (funct3 == 0x0 && funct7 == 0x00) ss << "add x" << rd << " x" << rs1 << " x" << rs2;
        else if (funct3 == 0x0 && funct7 == 0x20) ss << "sub x" << rd << " x" << rs1 << " x" << rs2;
        else if (funct3 == 0x7) ss << "and x" << rd << " x" << rs1 << " x" << rs2;
        else if (funct3 == 0x6) ss << "or x" << rd << " x" << rs1 << " x" << rs2;
        else if (funct3 == 0x1) ss << "sll x" << rd << " x" << rs1 << " x" << rs2;
    } else if (opcode == 0x13) { // I-type
        int imm = inst >> 20;
        if (funct3 == 0x0) ss << "addi x" << rd << " x" << rs1 << " " << imm;
        else if (funct3 == 0x1) ss << "slli x" << rd << " x" << rs1 << " " << (imm & 0x1F);
        else if (funct3 == 0x4 && imm == 0xFFF) ss << "not x" << rd << " x" << rs1;
    } else if (opcode == 0x63) { // Branch
        if (funct3 == 0x0) ss << "beq x" << rs1 << " x" << rs2 << " " << (int)(((inst >> 31) << 12) | (((inst >> 25) & 0x3F) << 5) | (((inst >> 8) & 0xF) << 1) | ((inst >> 7) & 0x1));
    } else if (opcode == 0x23) { // Store
        ss << "sw x" << rs2 << " " << (((inst >> 25) << 5) | ((inst >> 7) & 0x1F)) << "(x" << rs1 << ")";
    } else if (opcode == 0x03 && funct3 == 0x0) { // Load byte
        ss << "lb x" << rd << " " << (inst >> 20) << "(x" << rs1 << ")";
    } else if (opcode == 0x6f) { // Jump
        ss << "j " << (((inst >> 31) << 20) | ((inst >> 12) & 0xFF) << 12 | ((inst >> 20) & 0x7FE) | (((inst >> 21) & 0x3FF) << 1));
    }
    return ss.str();
}

void NoForwardingProcessor::runCycle() {
    // Writeback Stage
    if (mem_wb.reg_write) {
        wb_stage.op = 1;
        wb_stage.reg_number = mem_wb.rd;
        wb_stage.value = mem_wb.alu_result;
        wb_stage.write();
        if (pipeline_history.count(mem_wb.pc)) {
            pipeline_history[mem_wb.pc].push_back("WB");
        }
        mem_wb = MEMWB();
    } else if (mem_wb.pc != 0 && pipeline_history.count(mem_wb.pc)) {
        pipeline_history[mem_wb.pc].push_back("-");
    }

    // Memory Stage
    if (ex_mem.pc != 0 && !detectHazardMEM()) {
        mem_stage.reg_number = ex_mem.rd;
        mem_stage.op = ex_mem.control.mem_read ? 1 : (ex_mem.control.mem_write ? 2 : 0);
        mem_stage.mem_addr = ex_mem.alu_result;
        mem_wb.pc = ex_mem.pc;
        mem_wb.alu_result = (mem_stage.op == 1) ? mem_stage.putget() : ex_mem.alu_result;
        mem_wb.rd = ex_mem.rd;
        mem_wb.reg_write = ex_mem.control.reg_write;
        if (pipeline_history.count(ex_mem.pc)) {
            pipeline_history[ex_mem.pc].push_back("MEM");
        }
        ex_mem = EXMEM();
    } else if (ex_mem.pc != 0 && pipeline_history.count(ex_mem.pc)) {
        pipeline_history[ex_mem.pc].push_back("-");
    }

    // Execute Stage
    if (id_ex.pc != 0 && !detectHazardEX()) {
        ex_stage.op = id_ex.control.alu_op;
        ex_stage.a1 = id_ex.rs1_val;
        ex_stage.a2 = (id_ex.control.alu_op == 5 || id_ex.rs2 == 0) ? id_ex.imm : id_ex.rs2_val;
        ex_mem.pc = id_ex.pc;
        ex_mem.alu_result = ex_stage.out();
        ex_mem.rs2_val = id_ex.rs2_val;
        ex_mem.rs2 = id_ex.rs2;
        ex_mem.rd = id_ex.rd;
        ex_mem.control = id_ex.control;
        if (pipeline_history.count(id_ex.pc)) {
            pipeline_history[id_ex.pc].push_back("EX");
        }
        if (!id_ex.control.branch) id_ex = IDEX();
    } else if (id_ex.pc != 0 && pipeline_history.count(id_ex.pc)) {
        pipeline_history[id_ex.pc].push_back("-");
    }

    // Decode Stage
    bool id_stall = detectHazardID();
    if (if_id.instruction != 0 && !id_stall) {
        uint32_t inst = if_id.instruction;
        int opcode = inst & 0x7F;
        int funct3 = (inst >> 12) & 0x7;
        int funct7 = (inst >> 25) & 0x7F;
        id_ex.rd = (inst >> 7) & 0x1F;
        id_ex.rs1 = (inst >> 15) & 0x1F;
        id_ex.rs2 = (inst >> 20) & 0x1F;
        id_ex.imm = (opcode == 0x13 || opcode == 0x03) ? (inst >> 20) : 0;

        id_ex.control = ControlSignals();
        if (opcode == 0x33) { // R-type
            id_ex.control.reg_write = true;
            if (funct3 == 0x0 && funct7 == 0x00) id_ex.control.alu_op = 0; // ADD
            else if (funct3 == 0x0 && funct7 == 0x20) id_ex.control.alu_op = 1; // SUB
            else if (funct3 == 0x7) id_ex.control.alu_op = 2; // AND
            else if (funct3 == 0x6) id_ex.control.alu_op = 3; // OR
            else if (funct3 == 0x1) id_ex.control.alu_op = 4; // SLL
        } else if (opcode == 0x13) { // I-type
            id_ex.control.reg_write = true;
            if (funct3 == 0x0) id_ex.control.alu_op = 0; // ADDI
            else if (funct3 == 0x1) id_ex.control.alu_op = 4; // SLLI
            else if (funct3 == 0x4 && id_ex.imm == 0xFFF) id_ex.control.alu_op = 5; // NOT
        } else if (opcode == 0x63) { // Branch
            id_ex.control.branch = true;
            id_stage.branch_decision = true;
            id_stage.dec = funct3;
            id_stage.a1 = reg[id_ex.rs1];
            id_stage.a2 = reg[id_ex.rs2];
            id_ex.imm = ((inst >> 31) << 12) | (((inst >> 25) & 0x3F) << 5) | 
                        (((inst >> 8) & 0xF) << 1) | ((inst >> 7) & 0x1);
            if (id_stage.out(id_stage.a1, id_stage.a2)) {
                if_stage.pc = if_id.pc + id_ex.imm;
            }
        } else if (opcode == 0x23) { // Store
            id_ex.control.mem_write = true;
            id_ex.imm = ((inst >> 25) << 5) | ((inst >> 7) & 0x1F);
        } else if (opcode == 0x03 && funct3 == 0x0) { // Load byte
            id_ex.control.mem_read = true;
            id_ex.control.reg_write = true;
        } else if (opcode == 0x6f) { // Jump
            id_ex.control.branch = true;
            id_stage.branch_decision = true;
            id_stage.dec = 5;
            id_stage.a1 = 1;
            id_stage.a2 = 0;
            id_ex.imm = ((inst >> 31) << 20) | ((inst >> 12) & 0xFF) << 12 | 
                        ((inst >> 20) & 0x7FE) | (((inst >> 21) & 0x3FF) << 1);
            if_stage.pc = if_id.pc + id_ex.imm;
        }

        id_ex.rs1_val = reg[id_ex.rs1];
        id_ex.rs2_val = reg[id_ex.rs2];
        id_ex.pc = if_id.pc;
        if (pipeline_history.count(if_id.pc)) {
            pipeline_history[if_id.pc].push_back("ID");
        }
    } else if (if_id.instruction != 0 && pipeline_history.count(if_id.pc)) {
        pipeline_history[if_id.pc].push_back("-");
    }

    // Fetch Stage
    if (!stall) {
        if_id.instruction = if_stage.fetch();
        if_id.pc = if_stage.pc - 4;
        if (if_id.instruction != 0) {
            instr_mnemonics[if_id.pc] = decodeMnemonic(if_id.instruction);
            pipeline_history[if_id.pc] = {"IF"};
        }
    } else if (if_id.instruction != 0 && pipeline_history.count(if_id.pc)) {
        pipeline_history[if_id.pc].push_back("-");
    }
    stall = id_stall;
}

void NoForwardingProcessor::printPipeline(int cycle) {
    std::cout << "Cycle " << cycle << ":\n";
    for (const auto& entry : pipeline_history) {
        const std::string& mnemonic = instr_mnemonics[entry.first];
        const auto& stages = entry.second;
        std::cout << mnemonic;
        for (const auto& stage : stages) {
            std::cout << ";" << stage;
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}