#include <iostream>
#include <fstream>
#include <string>
#include <bitset>
#include <vector>
using namespace std;

// Pipeline register structs (unchanged)
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
bool stall = false;
int stall_count = 0;
bool if_stall = false;
bool ex_jump = false;
int new_addr;
int prev_cycle;
bool kill = false;
bool ex_branch = false;
vector<string> mnemonics;
vector<vector<string> > pipeline_stages;
int cycle_count_global;

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
    while (getline(infile, line) && inst_count < 1024) {
        if (line.empty()) continue;
        inst_mem[inst_count] = bitset<32>(stoul(line, nullptr, 16));
        bitset<32> inst = inst_mem[inst_count];
        bitset<7> opcode(inst.to_ulong() & 0b1111111);
        int rd = (int)((inst.to_ulong() >> 7) & 0b11111);
        int rs1 = (int)((inst.to_ulong() >> 15) & 0b11111);
        int rs2 = (int)((inst.to_ulong() >> 20) & 0b11111);
        string mnemonic;
        switch (opcode.to_ulong()) {
            case 0b0110011: mnemonic = "add x" + to_string(rd) + " x" + to_string(rs1) + " x" + to_string(rs2); break;
            case 0b0010011: {
                int imm = (int)((inst.to_ulong() >> 20) & 0b111111111111);
                if (imm & 0x800) imm |= 0xFFFFF000;
                mnemonic = "addi x" + to_string(rd) + " x" + to_string(rs1) + " " + to_string(imm);
                break;
            }
            case 0b0000011: {
                int imm = (int)((inst.to_ulong() >> 20) & 0b111111111111);
                if (imm & 0x800) imm |= 0xFFFFF000;
                mnemonic = "lb x" + to_string(rd) + " " + to_string(imm) + "(x" + to_string(rs1) + ")";
                break;
            }
            case 0b1100011: {
                int imm = ((inst[31] << 12) | (((inst.to_ulong() >> 25) & 0b111111) << 5) |
                           (((inst.to_ulong() >> 8) & 0b1111) << 1) | (inst[7] << 11));
                if (inst[31]) imm |= 0xFFFFF000;
                mnemonic = "beq x" + to_string(rs1) + " x" + to_string(rs2) + " " + to_string(imm);
                break;
            }
            case 0b1101111: {
                int imm = (inst[31] << 20) | ((inst.to_ulong() >> 21 & 0b1111111111) << 1) |
                          (inst[20] << 11) | ((inst.to_ulong() >> 12 & 0b11111111) << 12);
                if (inst[31]) imm |= 0xFFF00000;
                mnemonic = "jal x" + to_string(rd) + " " + to_string(imm);
                break;
            }
            case 0b1100111: {
                int imm = (int)((inst.to_ulong() >> 20) & 0b111111111111);
                if (imm & 0x800) imm |= 0xFFFFF000;
                mnemonic = "jalr x" + to_string(rd) + " " + to_string(imm) + "(x" + to_string(rs1) + ")";
                break;
            }
            default: mnemonic = "unknown"; break;
        }
        mnemonics.push_back(mnemonic);
        inst_count++;
    }
    infile.close();

    // Initialize with spaces instead of "-"
    pipeline_stages.resize(inst_count, vector<string>(cycle_count_global, " "));
}

void process_stalls() {
    for (int i = 0; i < inst_count; i++) {
        for (int c = 1; c < cycle_count_global; c++) {
            if (pipeline_stages[i][c] == pipeline_stages[i][c - 1] && pipeline_stages[i][c] != " ") {
                pipeline_stages[i][c] = "-";
            }
        }
    }
}

void print_pipeline() {
    for (int i = 0; i < inst_count; i++) {
        cout << mnemonics[i];
        for (int c = 0; c < cycle_count_global; c++) {
            cout << ";" << pipeline_stages[i][c];
        }
        cout << endl;
    }
}

void instruction_fetch(int cycle) {
    int idx = cycle - 1;
    if (if_stall) {
        if (if_id.pc / 4 + 1 < inst_count) {
            pipeline_stages[if_id.pc / 4 + 1][idx] = "IF";
        }
        return; // Previous IF remains
    }
    if (ex_jump && (cycle == prev_cycle + 1)) {
        pc.pc = new_addr;
        ex_jump = false;
        if_id.inst = inst_mem[pc.pc / 4];
        if_id.pc = pc.pc;
        if_id.valid = true;
        pc.pc += 4;
        if (pc.pc / 4 - 1 < inst_count) {
            pipeline_stages[pc.pc / 4 - 1][idx] = "IF";
        }
        return;
    }
    if (ex_branch) {
        ex_branch = false;
        if_id.inst = inst_mem[pc.pc / 4];
        if_id.pc = pc.pc;
        if_id.valid = false;
        if (pc.pc / 4 < inst_count) {
            pipeline_stages[pc.pc / 4][idx] = "IF";
        }
        return;
    }
    if (pc.valid && pc.pc / 4 < inst_count) {
        if_id.inst = inst_mem[pc.pc / 4];
        if_id.pc = pc.pc;
        if_id.valid = true;
        pc.pc += 4;
        pipeline_stages[pc.pc / 4 - 1][idx] = "IF";
    } else {
        if_id.valid = false;
    }
    pc.branch = 0;
}

void instruction_decode(int cycle) {
    int idx = cycle - 1;
    if (!if_id.valid) {
        id_ex.valid = false;
        return;
    }

    if (stall) {
        id_ex.valid = false;
        if (if_id.pc / 4 < inst_count) {
            pipeline_stages[if_id.pc / 4][idx] = "ID"; // Stall keeps it in ID
        }
        if (stall_count > 0) {
            stall_count--;
            if_stall = true;
            pc.pc += 4;
            return;
        } else {
            stall = false;
            if_stall = false;
        }
    }

    bitset<32> inst = if_id.inst;
    bitset<7> opcode(inst.to_ulong() & 0b1111111);
    int rs1 = (int)((inst.to_ulong() >> 15) & 0b11111);
    int rs2 = (int)((inst.to_ulong() >> 20) & 0b11111);
    int rd = (int)((inst.to_ulong() >> 7) & 0b11111);

    if (kill) {
        id_ex.inst = inst;
        id_ex.pc = if_id.pc;
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
        if (if_id.pc / 4 < inst_count) {
            // pipeline_stages[if_id.pc / 4][idx] = "ID";
        }
        kill = false;
        return;
    }

    // Hazard detection
    if (opcode.to_ulong() == 0b1100011) {  // Branch (SB-type)
        if (mem_wb.valid && mem_wb.mem_read && mem_wb.rd != 0 && 
            (mem_wb.rd == rs1 || mem_wb.rd == rs2)) {
            stall = true;
            stall_count = 2;
            id_ex.valid = false;
            pc.pc -= 4;
            if_stall = true;
            pipeline_stages[if_id.pc / 4][idx] = "ID";
            return;
        }
        if (ex_mem.valid && ex_mem.mem_read && ex_mem.rd != 0 && 
            (ex_mem.rd == rs1 || ex_mem.rd == rs2)) {
            stall = true;
            stall_count = 1;
            id_ex.valid = false;
            pc.pc -= 4;
            if_stall = true;
            pipeline_stages[if_id.pc / 4][idx] = "ID";
            return;
        }
        if (ex_mem.valid && ex_mem.reg_write && !ex_mem.mem_read && ex_mem.rd != 0 && 
            (ex_mem.rd == rs1 || ex_mem.rd == rs2)) {
            stall = true;
            stall_count = 1;
            id_ex.valid = false;
            pc.pc -= 4;
            if_stall = true;
            pipeline_stages[if_id.pc / 4][idx] = "ID";
            return;
        }
    } else {  // Non-branch
        if (ex_mem.valid && ex_mem.mem_read && ex_mem.rd != 0 && 
            (ex_mem.rd == rs1 || ex_mem.rd == rs2)) {
            stall = true;
            stall_count = 1;
            id_ex.valid = false;
            pc.pc -= 4;
            if_stall = true;
            pipeline_stages[if_id.pc / 4][idx] = "ID";
            return;
        }
    }

    int imm = 0;
    id_ex.inst = inst;
    id_ex.pc = if_id.pc;
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
    id_ex.valid = true;

    switch (opcode.to_ulong()) {
        case 0b0110011: // R-type
            id_ex.rd = rd;
            id_ex.rs1 = rs1;
            id_ex.rs2 = rs2;
            id_ex.reg_write = 1;
            id_ex.alu_op = 2;
            break;

        case 0b0010011: // I-type (arithmetic)
            id_ex.rd = rd;
            id_ex.rs1 = rs1;
            imm = (int)((inst.to_ulong() >> 20) & 0b111111111111);
            if (imm & 0x800) imm |= 0xFFFFF000;
            id_ex.imm = imm;
            id_ex.reg_write = 1;
            id_ex.alu_src = 1;
            break;

        case 0b0000011: // I-type (load)
            id_ex.rd = rd;
            id_ex.rs1 = rs1;
            imm = (int)((inst.to_ulong() >> 20) & 0b111111111111);
            if (imm & 0x800) imm |= 0xFFFFF000;
            id_ex.imm = imm;
            id_ex.reg_write = 1;
            id_ex.mem_read = 1;
            id_ex.mem_to_reg = 1;
            id_ex.alu_src = 1;
            break;

        case 0b0100011: // S-type (store)
            id_ex.rs1 = rs1;
            id_ex.rs2 = rs2;
            imm = ((int)((inst.to_ulong() >> 25) & 0b1111111) << 5) |
                  ((int)((inst.to_ulong() >> 7) & 0b11111));
            if (imm & 0x800) imm |= 0xFFFFF000;
            id_ex.imm = imm;
            id_ex.mem_write = 1;
            id_ex.alu_src = 1;
            break;

        case 0b1100011: // SB-type (branch)
            id_ex.rs1 = rs1;
            id_ex.rs2 = rs2;
            imm = ((inst[31] << 12) | (((inst.to_ulong() >> 25) & 0b111111) << 5) |
                   (((inst.to_ulong() >> 8) & 0b1111) << 1) | (inst[7] << 11));
            if (inst[31]) imm |= 0xFFFFF000;
            id_ex.imm = imm;
            id_ex.branch = 1;
            id_ex.alu_op = 1;
            break;

        case 0b1101111: // UJ-type (jal)
            id_ex.rd = rd;
            imm = (inst[31] << 20) | 
                  ((inst.to_ulong() >> 21 & 0b1111111111) << 1) | 
                  (inst[20] << 11) | 
                  ((inst.to_ulong() >> 12 & 0b11111111) << 12);
            if (inst[31]) imm |= 0xFFF00000;
            id_ex.imm = imm;
            id_ex.reg_write = 1;
            id_ex.branch = 2;
            break;

        case 0b1100111: // I-type (jalr)
            id_ex.rd = rd;
            id_ex.rs1 = rs1;
            imm = (int)((inst.to_ulong() >> 20) & 0b111111111111);
            if (imm & 0x800) imm |= 0xFFFFF000;
            id_ex.imm = imm;
            id_ex.reg_write = 1;
            id_ex.branch = 3;
            break;

        default:
            break;
    }

    id_ex.data1 = reg[id_ex.rs1];
    id_ex.data2 = reg[id_ex.rs2];

    if (id_ex.branch == 2 || id_ex.branch == 3) {
        int target_pc;
        if (id_ex.branch == 2) {  // jal
            target_pc = id_ex.pc + id_ex.imm;
        } else {  // jalr
            target_pc = reg[id_ex.rs1] + id_ex.imm;
        }
        ex_jump = true;
        new_addr = target_pc;
        prev_cycle = cycle;
        if_id.valid = false;
        kill = true;
    }

    if (id_ex.branch == 1) {
        ex_branch = true;
    }

    pipeline_stages[if_id.pc / 4][idx] = "ID";
}

void execute(int cycle) {
    int idx = cycle - 1;
    if (!id_ex.valid) {
        ex_mem.valid = false;
        return; // Previous stage (EX) remains
    }

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
    ex_mem.valid = true;

    fwd_unit.fwdA = 0;
    fwd_unit.fwdB = 0;
    if (ex_mem.valid && ex_mem.reg_write && ex_mem.rd != 0 && ex_mem.rd == id_ex.rs1)
        fwd_unit.fwdA = 1;
    if (mem_wb.valid && mem_wb.reg_write && mem_wb.rd != 0 && mem_wb.rd == id_ex.rs1)
        fwd_unit.fwdA = 2;
    if (ex_mem.valid && ex_mem.reg_write && ex_mem.rd != 0 && ex_mem.rd == id_ex.rs2)
        fwd_unit.fwdB = 1;
    if (mem_wb.valid && mem_wb.reg_write && mem_wb.rd != 0 && mem_wb.rd == id_ex.rs2)
        fwd_unit.fwdB = 2;

    ex_mem.rd_val = 0;
    ex_mem.data2 = id_ex.data2;

    pipeline_stages[id_ex.pc / 4][idx] = "EX";
}

void memory(int cycle) {
    int idx = cycle - 1;
    if (!ex_mem.valid) {
        mem_wb.valid = false;
        return; // Previous stage (MEM) remains
    }

    mem_wb.inst = ex_mem.inst;
    mem_wb.pc = ex_mem.pc;
    mem_wb.rd = ex_mem.rd;
    mem_wb.rs1 = ex_mem.rs1;
    mem_wb.rs2 = ex_mem.rs2;
    mem_wb.mem_read = ex_mem.mem_read;
    mem_wb.mem_to_reg = ex_mem.mem_to_reg;
    mem_wb.reg_write = ex_mem.reg_write;
    mem_wb.rd_val = ex_mem.rd_val;
    mem_wb.mem_data = 0;
    mem_wb.valid = true;

    pipeline_stages[ex_mem.pc / 4][idx] = "MEM";
}

void write_back(int cycle) {
    int idx = cycle - 1;
    if (!mem_wb.valid) {
        return; // Previous stage (WB) remains
    }

    wb_if.inst = mem_wb.inst;
    wb_if.pc = mem_wb.pc;
    wb_if.rd = mem_wb.rd;
    wb_if.rs1 = mem_wb.rs1;
    wb_if.rs2 = mem_wb.rs2;
    wb_if.rd_val = mem_wb.rd_val;
    wb_if.mem_data = mem_wb.mem_data;
    wb_if.mem_to_reg = mem_wb.mem_to_reg;
    wb_if.reg_write = mem_wb.reg_write;
    wb_if.valid = true;

    if (mem_wb.reg_write && mem_wb.rd != 0) {
        reg[mem_wb.rd] = 0;
    }

    pipeline_stages[mem_wb.pc / 4][idx] = "WB";
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cout << "Usage: ./forward ../inputfiles/filename.txt cyclecount" << endl;
        return 1;
    }

    string filename = argv[1];
    cycle_count_global = stoi(argv[2]);

    initialize_memory();
    load_instructions(filename);

    for (int cycle = 1; cycle <= cycle_count_global; cycle++) {
        write_back(cycle);
        memory(cycle);
        execute(cycle);
        instruction_decode(cycle);
        instruction_fetch(cycle);
    }

    // process_stalls(); // Post-process to replace consecutive identical stages with "-"
    print_pipeline();
    return 0;
}