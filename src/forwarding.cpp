// src/forwarding.cpp
// RISC-V simulator for RV32I base integer instruction set
// 5-stage pipelined processor with forwarding, using binary operations

#include <iostream>
#include <fstream>
#include <string>
#include <bitset>
using namespace std;

// Struct definitions (unchanged except for instruction as bitset)
typedef struct IF_ID {
    bitset<32> inst; // 32-bit instruction in binary
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
    int alu_src;    // 0: rs2, 1: imm
    int alu_op;     // ALU operation code
    int branch;     // 0: none, 1: beqz, 2: jal, 3: jalr
    int mem_read;
    int mem_write;
    int mem_to_reg;
    int reg_write;
    bool valid;
} ID_EX;

typedef struct EX_MEM {
    bitset<32> inst;
    int pc;
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
    int rd_val;
    int mem_data;
    int mem_to_reg;
    int reg_write;
    bool valid;
} MEM_WB;

typedef struct PC {
    int pc;
    int branch_addr;
    int branch; // 1 if branch taken
    bool valid;
} PC;

typedef struct forwarding_unit {
    int fwdA; // 0: no fwd, 1: EX/MEM, 2: MEM/WB
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
EX_MEM ex_mem = {bitset<32>(0), 0, 0, 0, 0, 0, 0, 0, 0, 0, false};
MEM_WB mem_wb = {bitset<32>(0), 0, 0, 0, 0, 0, 0, false};
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
            unsigned long val = stoul(line, nullptr, 16); // Hex to unsigned long
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
        if_id.inst = inst_mem[pc.pc / 4];
        if_id.pc = pc.pc;
        if_id.valid = true;
        pc.pc += 4;
        cout << "Cycle " << cycle << ": IF fetched " << if_id.inst << endl;
    } else {
        if_id.valid = false;
    }
}

void instruction_decode(int cycle) {
    if (!if_id.valid) {
        id_ex.valid = false;
        return;
    }

    bitset<32> inst = if_id.inst;
    cout << "Instruction in binary: " << inst << endl;
    id_ex.inst = if_id.inst; // Both are bitset<32>
    id_ex.pc = if_id.pc;
    id_ex.valid = true;

    // Extract opcode, funct3, and funct7 to determine instruction type
    bitset<7> opcode(inst.to_ulong() & 0b1111111);        // Bits 6:0
    bitset<3> funct3((inst.to_ulong() >> 12) & 0b111);    // Bits 14:12
    bitset<7> funct7((inst.to_ulong() >> 25) & 0b1111111); // Bits 31:25

    cout << "opcode: " << opcode << ", funct3: " << funct3 << ", funct7: " << funct7 << endl;

    // Reset control signals and fields
    id_ex.reg_write = 0;
    id_ex.mem_read = 0;
    id_ex.mem_write = 0;
    id_ex.mem_to_reg = 0;
    id_ex.alu_src = 0;
    id_ex.alu_op = 0;
    id_ex.branch = 0;
    id_ex.rd = 0;
    id_ex.rs1 = 0;
    id_ex.rs2 = 0;
    id_ex.imm = 0;

    // Define alu_op values
    // 0: add, 1: sub, 2: sll, 3: xor, 4: srl, 5: sra, 6: or, 7: and
    switch (opcode.to_ulong()) {
        case 0b0110011: // R-type
            id_ex.rd = (int)((inst.to_ulong() >> 7) & 0b11111);   // Bits 11:7
            id_ex.rs1 = (int)((inst.to_ulong() >> 15) & 0b11111); // Bits 19:15
            id_ex.rs2 = (int)((inst.to_ulong() >> 20) & 0b11111); // Bits 24:20
            id_ex.reg_write = 1;
            if (funct3.to_ulong() == 0b000 && funct7.to_ulong() == 0b0000000) {
                id_ex.alu_op = 0; // add
            } else if (funct3.to_ulong() == 0b000 && funct7.to_ulong() == 0b0100000) {
                id_ex.alu_op = 1; // sub
            } else if (funct3.to_ulong() == 0b001 && funct7.to_ulong() == 0b0000000) {
                id_ex.alu_op = 2; // sll
            } else if (funct3.to_ulong() == 0b100 && funct7.to_ulong() == 0b0000000) {
                id_ex.alu_op = 3; // xor
            } else if (funct3.to_ulong() == 0b101 && funct7.to_ulong() == 0b0000000) {
                id_ex.alu_op = 4; // srl
            } else if (funct3.to_ulong() == 0b101 && funct7.to_ulong() == 0b0100000) {
                id_ex.alu_op = 5; // sra
            } else if (funct3.to_ulong() == 0b110 && funct7.to_ulong() == 0b0000000) {
                id_ex.alu_op = 6; // or
            } else if (funct3.to_ulong() == 0b111 && funct7.to_ulong() == 0b0000000) {
                id_ex.alu_op = 7; // and
            } else {
                cout << "Unsupported R-type instruction: funct3=" << funct3 << ", funct7=" << funct7 << endl;
            }
            break;

        case 0b0010011: // I-type (arithmetic)
            id_ex.rd = (int)((inst.to_ulong() >> 7) & 0b11111);   // Bits 11:7
            id_ex.rs1 = (int)((inst.to_ulong() >> 15) & 0b11111); // Bits 19:15
            id_ex.imm = (int)((inst.to_ulong() >> 20) & 0b111111111111); // Bits 31:20
            if (id_ex.imm & 0b100000000000) id_ex.imm |= 0b11111111111111111111000000000000; // Sign extend
            id_ex.reg_write = 1;
            id_ex.alu_src = 1;
            if (funct3.to_ulong() == 0b000) {
                id_ex.alu_op = 0; // addi
            } else if (funct3.to_ulong() == 0b001 && funct7.to_ulong() == 0b0000000) {
                id_ex.alu_op = 2; // slli
            } else if (funct3.to_ulong() == 0b100) {
                id_ex.alu_op = 3; // xori
            } else if (funct3.to_ulong() == 0b101 && funct7.to_ulong() == 0b0000000) {
                id_ex.alu_op = 4; // srli
            } else if (funct3.to_ulong() == 0b101 && funct7.to_ulong() == 0b0100000) {
                id_ex.alu_op = 5; // srai
            } else if (funct3.to_ulong() == 0b110) {
                id_ex.alu_op = 6; // ori
            } else if (funct3.to_ulong() == 0b111) {
                id_ex.alu_op = 7; // andi
            } else {
                cout << "Unsupported I-type arithmetic instruction: funct3=" << funct3 << endl;
            }
            break;

        case 0b0000011: // I-type (load)
            id_ex.rd = (int)((inst.to_ulong() >> 7) & 0b11111);   // Bits 11:7
            id_ex.rs1 = (int)((inst.to_ulong() >> 15) & 0b11111); // Bits 19:15
            id_ex.imm = (int)((inst.to_ulong() >> 20) & 0b111111111111); // Bits 31:20
            if (id_ex.imm & 0b100000000000) id_ex.imm |= 0b11111111111111111111000000000000;
            id_ex.reg_write = 1;
            id_ex.mem_read = 1;
            id_ex.mem_to_reg = 1;
            id_ex.alu_src = 1;
            id_ex.alu_op = 0; // Address calculation (add)
            if (funct3.to_ulong() != 0b000) { // Only lb supported for now
                cout << "Unsupported load instruction: funct3=" << funct3 << endl;
            }
            break;

        case 0b1100011: // SB-type (branch)
            id_ex.rs1 = (int)((inst.to_ulong() >> 15) & 0b11111); // Bits 19:15
            id_ex.rs2 = (int)((inst.to_ulong() >> 20) & 0b11111); // Bits 24:20
            id_ex.imm = ((inst[31] << 12) | (((inst.to_ulong() >> 25) & 0b111111) << 5) |
                        (((inst.to_ulong() >> 8) & 0b1111) << 1) | (inst[7] << 11));
            if (id_ex.imm & 0b1000000000000) id_ex.imm |= 0b11111111111111111111100000000000;
            id_ex.branch = 1;
            id_ex.alu_op = funct3.to_ulong(); // Pass funct3 to EX for branch condition
            break;

        case 0b1101111: // UJ-type (jal)
            id_ex.rd = (int)((inst.to_ulong() >> 7) & 0b11111); // Bits 11:7
            id_ex.imm = ((inst[31] << 20) | (((inst.to_ulong() >> 12) & 0b11111111) << 12) |
                        (inst[20] << 11) | (((inst.to_ulong() >> 21) & 0b1111111111) << 1));
            if (id_ex.imm & 0b100000000000000000000) id_ex.imm |= 0b11111111111100000000000000000000;
            id_ex.reg_write = 1;
            id_ex.branch = 2;
            break;

        case 0b1100111: // I-type (jalr)
            id_ex.rd = (int)((inst.to_ulong() >> 7) & 0b11111);   // Bits 11:7
            id_ex.rs1 = (int)((inst.to_ulong() >> 15) & 0b11111); // Bits 19:15
            id_ex.imm = (int)((inst.to_ulong() >> 20) & 0b111111111111); // Bits 31:20
            if (id_ex.imm & 0b100000000000) id_ex.imm |= 0b11111111111111111111000000000000;
            if (funct3.to_ulong() == 0b000) {
                id_ex.reg_write = 1;
                id_ex.branch = 3;
            } else {
                cout << "Unsupported jalr instruction: funct3=" << funct3 << endl;
            }
            break;

        default:
            cout << "Unknown opcode: " << opcode << endl;
            break;
    }

    cout << "rd: " << id_ex.rd << ", rs1: " << id_ex.rs1 << ", rs2: " << id_ex.rs2 << ", imm: " << id_ex.imm << endl;

    id_ex.data1 = reg[id_ex.rs1];
    id_ex.data2 = reg[id_ex.rs2];

    // Forwarding for branch
    int valA = id_ex.data1;
    int valB = id_ex.data2;
    if (ex_mem.reg_write && ex_mem.rd != 0 && ex_mem.rd == id_ex.rs1)
        valA = ex_mem.rd_val;
    else if (mem_wb.reg_write && mem_wb.rd != 0 && mem_wb.rd == id_ex.rs1)
        valA = mem_wb.rd_val;
    if (ex_mem.reg_write && ex_mem.rd != 0 && ex_mem.rd == id_ex.rs2)
        valB = ex_mem.rd_val;
    else if (mem_wb.reg_write && mem_wb.rd != 0 && mem_wb.rd == id_ex.rs2)
        valB = mem_wb.rd_val;

    pc.branch = 0;
    if (id_ex.branch == 1) { // SB-type branch
        bool take_branch = false;
        switch (funct3.to_ulong()) {
            case 0b000: // beq
                take_branch = (valA == valB);
                break;
            case 0b001: // bne
                take_branch = (valA != valB);
                break;
            case 0b100: // blt
                take_branch = (valA < valB);
                break;
            case 0b101: // bge
                take_branch = (valA >= valB);
                break;
            case 0b110: // bltu
                take_branch = (unsigned int)valA < (unsigned int)valB;
                break;
            case 0b111: // bgeu
                take_branch = (unsigned int)valA >= (unsigned int)valB;
                break;
            default:
                cout << "Unsupported branch instruction: funct3=" << funct3 << endl;
                break;
        }
        if (take_branch) {
            pc.branch = 1;
            pc.branch_addr = id_ex.pc + id_ex.imm;
        }
    } else if (id_ex.branch == 2) { // jal
        pc.branch = 1;
        pc.branch_addr = id_ex.pc + id_ex.imm;
    } else if (id_ex.branch == 3) { // jalr
        pc.branch = 1;
        pc.branch_addr = valA + id_ex.imm;
    }

    cout << "Cycle " << cycle << ": ID decoded " << inst << endl;
}

void execute(int cycle) {
    if (!id_ex.valid) {
        ex_mem.valid = false;
        return;
    }

    ex_mem.inst = id_ex.inst;
    ex_mem.pc = id_ex.pc;
    ex_mem.rd = id_ex.rd;
    ex_mem.branch = id_ex.branch;
    ex_mem.mem_read = id_ex.mem_read;
    ex_mem.mem_write = id_ex.mem_write;
    ex_mem.mem_to_reg = id_ex.mem_to_reg;
    ex_mem.reg_write = id_ex.reg_write;
    ex_mem.valid = true;

    fwd_unit.fwdA = 0;
    fwd_unit.fwdB = 0;
    if (ex_mem.reg_write && ex_mem.rd != 0 && ex_mem.rd == id_ex.rs1)
        fwd_unit.fwdA = 1;
    if (ex_mem.reg_write && ex_mem.rd != 0 && ex_mem.rd == id_ex.rs2)
        fwd_unit.fwdB = 1;
    if (mem_wb.reg_write && mem_wb.rd != 0 &&
        !(ex_mem.reg_write && ex_mem.rd != 0 && ex_mem.rd == id_ex.rs1) &&
        mem_wb.rd == id_ex.rs1)
        fwd_unit.fwdA = 2;
    if (mem_wb.reg_write && mem_wb.rd != 0 &&
        !(ex_mem.reg_write && ex_mem.rd != 0 && ex_mem.rd == id_ex.rs2) &&
        mem_wb.rd == id_ex.rs2)
        fwd_unit.fwdB = 2;

    int valA = (fwd_unit.fwdA == 1) ? ex_mem.rd_val : (fwd_unit.fwdA == 2) ? mem_wb.rd_val : id_ex.data1;
    int valB = (fwd_unit.fwdB == 1) ? ex_mem.rd_val : (fwd_unit.fwdB == 2) ? mem_wb.rd_val : id_ex.data2;

    int operand2 = id_ex.alu_src ? id_ex.imm : valB;
    if (id_ex.alu_op == 0) {
        ex_mem.rd_val = valA + operand2;
    } else if (id_ex.branch == 2 || id_ex.branch == 3) {
        ex_mem.rd_val = id_ex.pc + 4;
    }
    ex_mem.data2 = valB;

    cout << "Cycle " << cycle << ": EX computed " << ex_mem.rd_val << endl;
}

void memory(int cycle) {
    if (!ex_mem.valid) {
        mem_wb.valid = false;
        return;
    }

    mem_wb.inst = ex_mem.inst;
    mem_wb.pc = ex_mem.pc;
    mem_wb.rd = ex_mem.rd;
    mem_wb.mem_to_reg = ex_mem.mem_to_reg;
    mem_wb.reg_write = ex_mem.reg_write;
    mem_wb.valid = true;

    if (ex_mem.mem_read) {
        int addr = ex_mem.rd_val;
        int word = data_mem[addr / 4];
        int byte_offset = addr % 4;
        mem_wb.mem_data = (word >> (byte_offset * 8)) & 0xFF;
        mem_wb.rd_val = mem_wb.mem_data;
    } else {
        mem_wb.rd_val = ex_mem.rd_val;
    }

    cout << "Cycle " << cycle << ": MEM completed" << endl;
}

void write_back(int cycle) {
    if (!mem_wb.valid) return;

    if (mem_wb.reg_write && mem_wb.rd != 0) {
        reg[mem_wb.rd] = mem_wb.mem_to_reg ? mem_wb.mem_data : mem_wb.rd_val;
        cout << "Cycle " << cycle << ": WB wrote " << reg[mem_wb.rd] << " to reg " << mem_wb.rd << endl;
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

        reg[10] = 1024;
        data_mem[256] = ('o' << 24) | ('l' << 16) | ('l' << 8) | 'h';
        data_mem[257] = 0;

        for (int cycle = 1; cycle <= cycle_count; cycle++) {
            write_back(cycle);
            memory(cycle);
            execute(cycle);
            instruction_decode(cycle);
            instruction_fetch(cycle);
        }

        cout << "Final a0 (length): " << reg[10] << endl;
    } catch (const std::exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }

    return 0;
}