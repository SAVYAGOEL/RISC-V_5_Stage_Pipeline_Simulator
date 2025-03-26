// src/forwarding.cpp
#include <iostream>
#include <fstream>
#include <string>
#include <bitset>
using namespace std;

// Struct definitions
typedef struct IF_ID {
    bitset<32> inst;
    int pc;
    bool valid;
} IF_ID;

typedef struct ID_EX {
    bitset<32> inst;
    int pc;
    int rs1;
    int rs2;
    int rd;
    int imm;
    int data1;
    int data2;
    int alu_src;
    int alu_op;
    int branch;
    int mem_read;
    int mem_write;
    int mem_to_reg;
    int reg_write;
    bool valid;
} ID_EX;

typedef struct EX_MEM {
    bitset<32> inst;
    int pc;
    int rs1;
    int rs2;
    int rd;
    int rd_val;
    int data2;
    int branch;
    int mem_read;
    int mem_write;
    int mem_to_reg;
    int reg_write;
    bool valid;
} EX_MEM;

typedef struct MEM_WB {
    bitset<32> inst;
    int pc;
    int rd;
    int rs1;
    int rs2;
    int rd_val;
    int mem_read;
    int mem_data;
    int mem_to_reg;
    int reg_write;
    bool valid;
} MEM_WB;

typedef struct WB_IF {
    bitset<32> inst;
    int pc;
    int rs1;
    int rs2;
    int rd;
    int rd_val;
    int mem_data;
    int mem_to_reg;
    int reg_write;
    bool valid;
} WB_IF;

typedef struct PC {
    int pc;
    int branch_addr;
    int branch;
    bool valid;
} PC;

typedef struct forwarding_unit {
    int fwdA;
    int fwdB;
    int fwdA_val;
    int fwdB_val;
    bool valid;
} forwarding_unit;

// Global variables
int reg[32] = {0};
int data_mem[1024 * 1024] = {0};
IF_ID if_id = {bitset<32>(0), 0, false};
ID_EX id_ex = {bitset<32>(0), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false};
EX_MEM ex_mem = {bitset<32>(0), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false};
MEM_WB mem_wb = {bitset<32>(0), 0, 0, 0, 0, 0, 0, 0, 0, false};
WB_IF wb_if = {bitset<32>(0), 0, 0, 0, 0, 0, 0, 0, 0, false};
PC pc = {0, 0, 0, true};
forwarding_unit fwd_unit = {0, 0, 0, 0, false};
bitset<32> inst_mem[1024];
int inst_count = 0;

void initialize_memory() {
    for (int i = 0; i < 1024 * 1024; i++) {
        data_mem[i] = 0;
    }
}

void load_instructions(const string& filename) {
    ifstream infile(filename);
    if (!infile.is_open()) {
        cerr << "Error: Could not open file " << filename << endl;
        exit(1);
    }

    string line;
    inst_count = 0;
    int line_num = 0;
    while (getline(infile, line) && inst_count < 1024) {
        line_num++;
        if (line.empty()) continue;
        try {
            unsigned long val = stoul(line, nullptr, 16);
            if (val > 0xFFFFFFFFUL) {
                cerr << "Error: Instruction at line " << line_num << " (" << line << ") exceeds 32-bit range" << endl;
                exit(1);
            }
            inst_mem[inst_count++] = bitset<32>(val);
            cout << "inst_mem[" << inst_count - 1 << "] = " << val << " (" << inst_mem[inst_count - 1] << ")" << endl;
        } catch (const std::exception& e) {
            cerr << "Error at line " << line_num << " (" << line << "): " << e.what() << endl;
            exit(1);
        }
    }
    infile.close();
    cout << "Loaded " << inst_count << " instructions from " << filename << endl;
}

void instruction_fetch(int cycle) {
    if (pc.valid && pc.pc / 4 < inst_count) {
        if (pc.branch) {
            if_id.inst = inst_mem[pc.pc / 4];
            if_id.pc = pc.pc;
            if_id.valid = true;
            cout << "Cycle " << cycle << ": IF fetched " << if_id.inst << " at PC=" << pc.pc << endl;
        } else {
            if_id.inst = inst_mem[pc.pc / 4];
            if_id.pc = pc.pc;
            if_id.valid = true;
            pc.pc += 4;
            cout << "Cycle " << cycle << ": IF fetched " << if_id.inst << " at PC=" << if_id.pc << endl;
        }
    } else {
        if_id.valid = false;
        cout << "Cycle " << cycle << ": IF stage invalid" << endl;
    }
    pc.branch = 0;
}

void instruction_decode(int cycle) {
    if (!if_id.valid) {
        id_ex.valid = false;
        return;
    }

    // Check for load-use hazard
    bool stall = false;
    bitset<32> inst = if_id.inst;
    bitset<7> opcode(inst.to_ulong() & 0b1111111);
    int rs1 = (int)((inst.to_ulong() >> 15) & 0b11111);
    int rs2 = (int)((inst.to_ulong() >> 20) & 0b11111);
    bitset<7> prev_opcode(id_ex.inst.to_ulong() & 0b1111111);
    bitset<7> prev2_opcode(ex_mem.inst.to_ulong() & 0b1111111);

    //normal case of load-use hazard
    if (id_ex.valid && id_ex.mem_read && (id_ex.rd == rs1 || id_ex.rd == rs2) && (opcode.to_ulong() == 0b1100011)) {
        stall = true;
        cout << "Cycle " << cycle << ": ID stalled due to load-use hazard" << endl;
    }

    cout << "id_ex.valid: " << id_ex.valid << ", id_ex.mem_read: " << id_ex.mem_read << ", id_ex.rd: " << id_ex.rd << ", rs1: " << rs1 << ", rs2: " << rs2 << ", opcode: " << opcode << ", prev_opcode: " << prev_opcode << endl;
    //case when current instruction is branch type and depends on previous instruction
    if (id_ex.valid && id_ex.reg_write && (id_ex.rd == rs1 || id_ex.rd == rs2) && (opcode.to_ulong() == 0b1100011)) {
        stall = true;
        cout << "Cycle " << cycle << ": ID stalled due to branch hazard level1" << endl;
    }

    //case when current instr is branch type, it depends on prev to prev instr and prev to prev instr is load type
    cout << "ex_mem.valid: " << mem_wb.valid << ", ex_mem.mem_read: " << ex_mem.mem_read << ", ex_mem.rd: " << ex_mem.rd << ", rs1: " << rs1 << ", rs2: " << rs2 << ", opcode: " << opcode << ", prev_prev_opcode: " << prev2_opcode << endl;
    if (ex_mem.valid && ex_mem.mem_read && (ex_mem.rd == rs1 || ex_mem.rd == rs2) && (opcode.to_ulong() == 0b1100011)) {
        stall = true;
        cout << "Cycle " << cycle << ": ID stalled due to branch hazard level2" << endl;
    }

    if (stall) {
        id_ex.valid = false;
        pc.pc -= 4;
        
        return;
    }

    // Clear id_ex
    id_ex.inst = bitset<32>(0);
    id_ex.pc = 0;
    id_ex.rs1 = 0;
    id_ex.rs2 = 0;
    id_ex.rd = 0;
    id_ex.imm = 0;
    id_ex.data1 = 0;
    id_ex.data2 = 0;
    id_ex.alu_src = 0;
    id_ex.alu_op = 0;
    id_ex.branch = 0;
    id_ex.mem_read = 0;
    id_ex.mem_write = 0;
    id_ex.mem_to_reg = 0;
    id_ex.reg_write = 0;
    id_ex.valid = false;

    cout << "Instruction in binary: " << inst << endl;
    id_ex.inst = if_id.inst;
    id_ex.pc = if_id.pc;
    id_ex.valid = true;

    bitset<3> funct3((inst.to_ulong() >> 12) & 0b111);
    bitset<7> funct7((inst.to_ulong() >> 25) & 0b1111111);

    cout << "opcode: " << opcode << ", funct3: " << funct3 << ", funct7: " << funct7 << endl;

    switch (opcode.to_ulong()) {
        case 0b0110011: // R-type
            id_ex.rd = (int)((inst.to_ulong() >> 7) & 0b11111);
            id_ex.rs1 = (int)((inst.to_ulong() >> 15) & 0b11111);
            id_ex.rs2 = (int)((inst.to_ulong() >> 20) & 0b11111);
            id_ex.reg_write = 1;
            id_ex.alu_src = 0;
            id_ex.mem_to_reg = 0;
            id_ex.mem_read = 0;
            id_ex.mem_write = 0;
            id_ex.branch = 0;
            id_ex.alu_op = 2; // ALUOp = 10
            cout << "R-type: rd=" << id_ex.rd << ", rs1=" << id_ex.rs1 << ", rs2=" << id_ex.rs2 << endl;
            break;

        case 0b0010011: // I-type (arithmetic)
            id_ex.rd = (int)((inst.to_ulong() >> 7) & 0b11111);
            id_ex.rs1 = (int)((inst.to_ulong() >> 15) & 0b11111);
            id_ex.imm = (int)((inst.to_ulong() >> 20) & 0b111111111111);
            if (id_ex.imm & 0b100000000000) id_ex.imm |= 0b11111111111111111111000000000000;
            id_ex.reg_write = 1;
            id_ex.alu_src = 1;
            id_ex.mem_to_reg = 0;
            id_ex.mem_read = 0;
            id_ex.mem_write = 0;
            id_ex.branch = 0;
            id_ex.alu_op = 0; // ALUOp = 00
            cout << "I-type rd: " << id_ex.rd << ", rs1: " << id_ex.rs1 << ", imm: " << id_ex.imm << endl;
            break;

        case 0b0000011: // I-type (load)
            id_ex.rd = (int)((inst.to_ulong() >> 7) & 0b11111);
            id_ex.rs1 = (int)((inst.to_ulong() >> 15) & 0b11111);
            id_ex.imm = (int)((inst.to_ulong() >> 20) & 0b111111111111);
            if (id_ex.imm & 0b100000000000) id_ex.imm |= 0b11111111111111111111000000000000;
            id_ex.reg_write = 1;
            id_ex.mem_read = 1;
            id_ex.mem_to_reg = 1;
            id_ex.alu_src = 1;
            id_ex.mem_write = 0;
            id_ex.branch = 0;
            id_ex.alu_op = 0; // ALUOp = 00
            cout << "Load: rd=" << id_ex.rd << ", rs1=" << id_ex.rs1 << ", imm: " << id_ex.imm << endl;
            cout << "id_ex.mem_read: " << id_ex.mem_read << endl;
            break;

        case 0b0100011: // S-type (store)
            id_ex.rs1 = (int)((inst.to_ulong() >> 15) & 0b11111);
            id_ex.rs2 = (int)((inst.to_ulong() >> 20) & 0b11111);
            id_ex.imm = ((int)((inst.to_ulong() >> 25) & 0b1111111) << 5) | 
                        ((int)((inst.to_ulong() >> 7) & 0b11111));
            if (id_ex.imm & 0b100000000000) id_ex.imm |= 0b11111111111111111111000000000000;
            id_ex.reg_write = 0;
            id_ex.mem_read = 0;
            id_ex.mem_write = 1;
            id_ex.alu_src = 1;
            id_ex.mem_to_reg = 0; // Don't care
            id_ex.branch = 0;
            id_ex.alu_op = 0; // ALUOp = 00
            cout << "S-type: rs1=" << id_ex.rs1 << ", rs2=" << id_ex.rs2 << ", imm=" << id_ex.imm << endl;
            break;

        case 0b1100011: // SB-type (branch)
            id_ex.rs1 = (int)((inst.to_ulong() >> 15) & 0b11111);
            id_ex.rs2 = (int)((inst.to_ulong() >> 20) & 0b11111);
            id_ex.imm = ((inst[31] << 12) | (((inst.to_ulong() >> 25) & 0b111111) << 5) |
                        (((inst.to_ulong() >> 8) & 0b1111) << 1) | (inst[7] << 11));
            if (id_ex.imm & 0b1000000000000) id_ex.imm |= 0b11111111111111111111100000000000;
            id_ex.reg_write = 0;
            id_ex.mem_read = 0;
            id_ex.mem_write = 0;
            id_ex.alu_src = 0;
            id_ex.mem_to_reg = 0; // Don't care
            id_ex.branch = 1;
            id_ex.alu_op = 1; // ALUOp = 01
            cout << "SB-type: rs1=" << id_ex.rs1 << ", rs2=" << id_ex.rs2 << ", imm=" << id_ex.imm << endl;
            break;

        case 0b1101111: // UJ-type (jal)
            id_ex.rd = (int)((inst.to_ulong() >> 7) & 0b11111);
            id_ex.imm = ((inst[31] << 20) | (((inst.to_ulong() >> 12) & 0b11111111) << 12) |
                        (inst[20] << 11) | (((inst.to_ulong() >> 21) & 0b1111111111) << 1));
            if (id_ex.imm & 0b100000000000000000000) id_ex.imm |= 0b11111111111100000000000000000000;
            id_ex.reg_write = 1;
            id_ex.mem_read = 0;
            id_ex.mem_write = 0;
            id_ex.alu_src = 0; // Don't care
            id_ex.mem_to_reg = 0;
            id_ex.branch = 2;
            id_ex.alu_op = 0; // Don't care
            cout << "J-type jal: rd=" << id_ex.rd << ", imm=" << id_ex.imm << endl;
            break;

        case 0b1100111: // I-type (jalr)
            id_ex.rd = (int)((inst.to_ulong() >> 7) & 0b11111);
            id_ex.rs1 = (int)((inst.to_ulong() >> 15) & 0b11111);
            id_ex.imm = (int)((inst.to_ulong() >> 20) & 0b111111111111);
            if (id_ex.imm & 0b100000000000) id_ex.imm |= 0b11111111111111111111000000000000;
            if (funct3.to_ulong() == 0b000) {
                id_ex.reg_write = 1;
                id_ex.mem_read = 0;
                id_ex.mem_write = 0;
                id_ex.alu_src = 0; // Don't care
                id_ex.mem_to_reg = 0;
                id_ex.branch = 3;
                id_ex.alu_op = 0; // Don't care
            }
            cout << "I-type jalr: rd=" << id_ex.rd << ", rs1=" << id_ex.rs1 << ", imm: " << id_ex.imm << endl;
            break;

        default:
            cout << "Unknown opcode: " << opcode << endl;
            break;
    }

    id_ex.data1 = reg[id_ex.rs1];
    id_ex.data2 = reg[id_ex.rs2];

    cout << "reg[" << 0 << "] = " << reg[0] << endl;
    cout << "reg[" << 5 << "] = " << reg[5] << endl;
    cout << "reg[" << 6 << "] = " << reg[6] << endl;
    cout << "reg[" << 10 << "] = " << reg[10] << endl;
    
    cout << "Data 1(rs1): " << id_ex.data1 << ", Data 2(rs2): " << id_ex.data2 << endl;

    int valA = reg[id_ex.rs1];
    int valB = reg[id_ex.rs2];

    cout << "ex_mem.reg_write: " << ex_mem.reg_write << ", ex_mem.rd: " << ex_mem.rd << ", id_ex.rs1: " << id_ex.rs1 << endl;
    cout << "mem_wb.reg_write: " << mem_wb.reg_write << ", mem_wb.rd: " << mem_wb.rd << ", id_ex.rs1: " << id_ex.rs1 << endl;
    cout << "ex_mem.mem_read: " << ex_mem.mem_read << ", mem_wb.mem_read: " << mem_wb.mem_read << endl;
    if (ex_mem.reg_write && ex_mem.rd != 0 && ex_mem.rd == id_ex.rs1) {
        valA = ex_mem.rd_val;
        cout << "ID Forwarding EX/MEM to valA: " << valA << endl;
    }
    else if (mem_wb.reg_write && mem_wb.rd != 0 && mem_wb.rd == id_ex.rs1) {
        valA = mem_wb.mem_to_reg ? mem_wb.mem_data : mem_wb.rd_val;
        cout << "ID Forwarding MEM/WB to valA: " << valA << endl;
    }
    if (ex_mem.reg_write && ex_mem.rd != 0 && ex_mem.rd == id_ex.rs2) {
        valB = ex_mem.rd_val;
        cout << "ID Forwarding EX/MEM to valB: " << valB << endl;
    } else if (mem_wb.reg_write && mem_wb.rd != 0 && mem_wb.rd == id_ex.rs2) {
        valB = mem_wb.mem_to_reg ? mem_wb.mem_data : mem_wb.rd_val;
        cout << "ID Forwarding MEM/WB to valB: " << valB << endl;
    }

    cout << "valA: " << valA << ", valB: " << valB << endl;

    pc.branch = 0;
    if (id_ex.branch == 1) {
        bool take_branch = false;
        switch (funct3.to_ulong()) {
            case 0b000: // beq
                take_branch = (valA == valB);
                cout << "BEQ: valA=" << valA << ", valB=" << valB << ", take_branch=" << take_branch << endl;
                break;
            default:
                cout << "Unsupported branch instruction: funct3=" << funct3 << endl;
                break;
        }
        if (take_branch) {
            pc.branch = 1;
            pc.branch_addr = id_ex.pc + id_ex.imm;
            pc.pc = pc.branch_addr;
            if_id.valid = false;
            cout << "Branch taken to PC=" << pc.pc << endl;
        }
    } else if (id_ex.branch == 2) {
        pc.branch = 1;
        pc.branch_addr = id_ex.pc + id_ex.imm;
        pc.pc = pc.branch_addr;
        if_id.valid = false;
        cout << "JAL to PC=" << pc.pc << endl;
    } else if (id_ex.branch == 3) {
        pc.branch = 1;
        pc.branch_addr = valA + id_ex.imm;
        pc.pc = pc.branch_addr;
        if_id.valid = false;
        cout << "JALR to PC=" << pc.pc << endl;
    }

    cout << "id_ex.mem_read at end of ID: " << id_ex.mem_read << endl;
    cout << "Cycle " << cycle << ": ID decoded " << inst << endl;
}

void execute(int cycle) {

    ex_mem.inst = id_ex.inst;
    ex_mem.pc = id_ex.pc;
    ex_mem.rs1 = id_ex.rs1;
    ex_mem.rs2 = id_ex.rs2;
    ex_mem.rd = id_ex.rd;
    ex_mem.branch = id_ex.branch;
    ex_mem.mem_read = id_ex.mem_read;
    ex_mem.mem_write = id_ex.mem_write;
    ex_mem.mem_to_reg = id_ex.mem_to_reg;
    ex_mem.reg_write = id_ex.reg_write;
    ex_mem.valid = id_ex.valid;

    if (!id_ex.valid) {
        ex_mem.valid = false;
        cout << "Cycle " << cycle << ": EX stage invalid" << endl;
        return;
    }

    // Update ex_mem with current instruction's control signals *first*
    

    // Forwarding logic *after* updating ex_mem
    fwd_unit.fwdA = 0;
    fwd_unit.fwdB = 0;

    cout << "ex_mem.rd: " << ex_mem.rd << ", id_ex.rs1: " << id_ex.rs1 << ", id_ex.rs2: " << id_ex.rs2 << ", ex_mem.reg_write: " << ex_mem.reg_write << endl;

    if (mem_wb.reg_write && mem_wb.rd != 0 && mem_wb.rd == ex_mem.rs1) {
        fwd_unit.fwdA = 1;
    }
    if (mem_wb.reg_write && mem_wb.rd != 0 && mem_wb.rd == ex_mem.rs2) {
        fwd_unit.fwdB = 1;
    }

    if (wb_if.reg_write && wb_if.rd != 0 && wb_if.rd == ex_mem.rs1 && fwd_unit.fwdA == 0) {
        fwd_unit.fwdA = 2;
    }
    if (wb_if.reg_write && wb_if.rd != 0 && wb_if.rd == ex_mem.rs2 && fwd_unit.fwdB == 0) {
        fwd_unit.fwdB = 2;
    }

    int valA = id_ex.data1;
    int valB = id_ex.data2;

    if (fwd_unit.fwdA == 1) {
        valA = ex_mem.rd_val;
        cout << "Forwarding EX/MEM to valA: " << valA << endl;
    } else if (fwd_unit.fwdA == 2) {
        valA = mem_wb.mem_to_reg ? mem_wb.mem_data : mem_wb.rd_val;
        cout << "Forwarding MEM/WB to valA: " << valA << endl;
    }

    if (fwd_unit.fwdA == 1) {
        cout << "valA is coming from EX/MEM stage" << endl;
    } else if (fwd_unit.fwdA == 2) {
        cout << "valA is coming from MEM/WB stage" << endl;
    } else {
        cout << "valA is coming from ID/EX stage" << endl;
    }

    if (fwd_unit.fwdB == 1) {
        valB = ex_mem.rd_val;
        cout << "Forwarding EX/MEM to valB: " << valB << endl;
    } else if (fwd_unit.fwdB == 2) {
        valB = mem_wb.mem_to_reg ? mem_wb.mem_data : mem_wb.rd_val;
        cout << "Forwarding MEM/WB to valB: " << valB << endl;
    }

    if (fwd_unit.fwdB == 1) {
        cout << "valB is coming from EX/MEM stage" << endl;
    } else if (fwd_unit.fwdB == 2) {
        cout << "valB is coming from MEM/WB stage" << endl;
    } else {
        cout << "valB is coming from ID/EX stage" << endl;
    }

    // Compute ALU result or handle branch/jump
    int operand2 = id_ex.alu_src ? id_ex.imm : valB;
    if (id_ex.branch == 1) {
        // For branch instructions like beq, compute the branch target address
        ex_mem.rd_val = id_ex.pc + id_ex.imm; // Branch target address (not used since branch is resolved in ID)
        cout << "Cycle " << cycle << ": EX computed branch target " << ex_mem.rd_val << " for beq" << endl;
    } else if (id_ex.branch == 2 || id_ex.branch == 3) {
        // For jal and jalr, compute the return address
        ex_mem.rd_val = id_ex.pc + 4;
        cout << "Cycle " << cycle << ": EX computed return address " << ex_mem.rd_val << " for jal/jalr" << endl;
    } else {
        // Determine ALU operation based on id_ex.alu_op
        int alu_control = 0; // Default to 0000 (AND)
        bitset<3> funct3((id_ex.inst.to_ulong() >> 12) & 0b111);
        bitset<7> funct7((id_ex.inst.to_ulong() >> 25) & 0b1111111);

        if (id_ex.alu_op == 0) { // ALUOp = 00 (ld, sd, addi)
            alu_control = 0b0010; // add
        } else if (id_ex.alu_op == 1) { // ALUOp = 01 (beq)
            alu_control = 0b0110; // subtract
        } else if (id_ex.alu_op == 2) { // ALUOp = 10 (R-type)
            if (funct7.to_ulong() == 0b0000000 && funct3.to_ulong() == 0b000) {
                alu_control = 0b0010; // add
            } else if (funct7.to_ulong() == 0b0100000 && funct3.to_ulong() == 0b000) {
                alu_control = 0b0110; // subtract
            } else if (funct7.to_ulong() == 0b0000000 && funct3.to_ulong() == 0b111) {
                alu_control = 0b0000; // AND
            } else if (funct7.to_ulong() == 0b0000000 && funct3.to_ulong() == 0b110) {
                alu_control = 0b0001; // OR
            } else {
                cout << "Unsupported R-type instruction: funct7=" << funct7 << ", funct3=" << funct3 << endl;
                alu_control = 0b0010; // Default to add
            }
        }

        // Perform the ALU operation based on alu_control
        switch (alu_control) {
            case 0b0000: // AND
                ex_mem.rd_val = valA & operand2;
                cout << "Cycle " << cycle << ": EX computed " << ex_mem.rd_val << " (valA=" << valA << ", operand2=" << operand2 << ", AND)" << endl;
                break;
            case 0b0001: // OR
                ex_mem.rd_val = valA | operand2;
                cout << "Cycle " << cycle << ": EX computed " << ex_mem.rd_val << " (valA=" << valA << ", operand2=" << operand2 << ", OR)" << endl;
                break;
            case 0b0010: // add
                ex_mem.rd_val = valA + operand2;
                cout << "Cycle " << cycle << ": EX computed " << ex_mem.rd_val << " (valA=" << valA << ", operand2=" << operand2 << ", add)" << endl;
                break;
            case 0b0110: // subtract
                ex_mem.rd_val = valA - operand2;
                cout << "Cycle " << cycle << ": EX computed " << ex_mem.rd_val << " (valA=" << valA << ", operand2=" << operand2 << ", subtract)" << endl;
                break;
            default:
                ex_mem.rd_val = 0;
                cout << "Cycle " << cycle << ": EX set rd_val to 0 (unsupported ALU operation)" << endl;
                break;
        }
    }
    ex_mem.data2 = valB;
}

void memory(int cycle) {

    mem_wb.inst = ex_mem.inst;
    mem_wb.rs1 = ex_mem.rs1;
    mem_wb.rs2 = ex_mem.rs2;
    mem_wb.pc = ex_mem.pc;
    mem_wb.rd = ex_mem.rd;
    mem_wb.mem_read = ex_mem.mem_read;
    mem_wb.mem_to_reg = ex_mem.mem_to_reg;
    mem_wb.reg_write = ex_mem.reg_write;
    mem_wb.valid = ex_mem.valid;

    if (!ex_mem.valid) {
        mem_wb.valid = false;
        cout << "Cycle " << cycle << ": MEM stage invalid" << endl;
        return;
    }

    cout << "ex_mem.mem_read: " << ex_mem.mem_read << endl;
    if (ex_mem.mem_read) {

        //add logic for forwarding, in case computed address needs to be forwarded

        if(wb_if.reg_write && wb_if.rd != 0 && wb_if.rd == mem_wb.rs1){
            cout << "MEM Forwarding rs1 from WB/IF: " << ex_mem.rs1 << endl;
            int addr = wb_if.mem_data;
            int word = data_mem[addr / 4];
            int byte_offset = addr % 4;
            mem_wb.mem_data = (word >> (byte_offset * 8)) & 0xFF;
            mem_wb.rd_val = mem_wb.mem_data;
            cout << "Cycle " << cycle << ": MEM read from addr " << addr << ", data=" << mem_wb.mem_data << endl;
        }
        else{
            int addr = ex_mem.rd_val;
            int word = data_mem[addr / 4];
            int byte_offset = addr % 4;
            mem_wb.mem_data = (word >> (byte_offset * 8)) & 0xFF;
            mem_wb.rd_val = mem_wb.mem_data;
            cout << "Cycle " << cycle << ": MEM read from addr " << addr << ", data=" << mem_wb.mem_data << endl;
        }
        
    } else {
        mem_wb.rd_val = ex_mem.rd_val;
        mem_wb.mem_data = 0;
        cout << "Cycle " << cycle << ": MEM passed rd_val=" << mem_wb.rd_val << endl;
    }
}

void write_back(int cycle) {

    wb_if.inst = mem_wb.inst;
    wb_if.rs1 = mem_wb.rs1;
    wb_if.rs2 = mem_wb.rs2;
    wb_if.pc = mem_wb.pc;
    wb_if.rd = mem_wb.rd;
    wb_if.rd_val = mem_wb.rd_val;
    wb_if.mem_data = mem_wb.mem_data;
    wb_if.mem_to_reg = mem_wb.mem_to_reg;
    wb_if.reg_write = mem_wb.reg_write;
    wb_if.valid = mem_wb.valid;

    if (!mem_wb.valid) {
        cout << "Cycle " << cycle << ": WB stage invalid" << endl;
        return;
    }

    if (mem_wb.reg_write && mem_wb.rd != 0) {
        reg[mem_wb.rd] = mem_wb.mem_to_reg ? mem_wb.mem_data : mem_wb.rd_val;
        cout << "Cycle " << cycle << ": WB wrote " << reg[mem_wb.rd] << " to reg " << mem_wb.rd << endl;
        cout << "x5 = " << reg[5] << ", x6 = " << reg[6] << endl;
    } else {
        cout << "Cycle " << cycle << ": WB no write" << endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cout << "Usage: ./forward ../inputfiles/filename.txt cyclecount" << endl;
        return 1;
    }

    string filename = argv[1];
    try {
        int cycle_count = stoi(argv[2]);
        if (cycle_count <= 0) {
            cerr << "Error: cyclecount must be positive" << endl;
            return 1;
        }

        initialize_memory();
        load_instructions(filename);

        reg[5] = 2;
        reg[10] = 1024;
        data_mem[256] = ('o' << 24) | ('l' << 16) | ('l' << 8) | 'h';
        data_mem[257] = 0;

        bool done = false;
        for (int cycle = 1; cycle <= cycle_count && !done; cycle++) {
            write_back(cycle);
            memory(cycle);
            execute(cycle);
            instruction_decode(cycle);
            instruction_fetch(cycle);

            if (id_ex.valid && id_ex.branch == 3) {
                done = true;
            }
        }

        cout << "Final a0 (length): " << reg[10] << endl;
    } catch (const std::exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }

    return 0;
}