#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <bitset>
#include <vector>
#include <map>
#include <unordered_map>
using namespace std;

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
    int val1;
    int val2;
    int alu_op; 
    int rd_val;
    int alu_src; // 0 - rs2 , 1 - imm
    int branch;
    int mem_read;
    int mem_write;
    int mem_to_reg;
    int reg_write;
    int mem_op;
    bool valid;
} ID_EX;

typedef struct EX_MEM {
    bitset<32> inst;
    int pc;
    int rs1;
    int rs2;
    int rd;
    int rd_val;
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
    int branch;
    int mem_write;
    int mem_to_reg;
    int mem_data;
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
    int mem_read;
    int mem_data;
    int branch;
    int mem_write;
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

// Global variables
int reg[32] = {0};
// int data_mem[1024 * 1024] = {0};
map <int, int> data_mem;
IF_ID if_id = {bitset<32>(0), 0, false};
ID_EX id_ex = {bitset<32>(0), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false};
EX_MEM ex_mem = {bitset<32>(0), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false};
MEM_WB mem_wb = {bitset<32>(0), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false};
WB_IF wb_if = {bitset<32>(0), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false};
PC pc = {0, 0, 0, true};
// bitset<32> inst_mem[1024];
vector<bitset<32> > inst_mem;
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

        // Use stringstream to parse the line
        stringstream ss(line);
        int line_num;          // Line number (ignored for now)
        string hex_code;       // Hexadecimal instruction code
        string mnemonic;       // Rest of the line is the mnemonic
        string mnemonic_part;  // To build mnemonic from remaining tokens

        // Extract line number
        ss >> line_num;

        // Extract hex code
        ss >> hex_code;

        // Extract the mnemonic (everything after hex code)
        while (ss >> mnemonic_part) {
            if (mnemonic.empty()) {
                mnemonic = mnemonic_part;
            } else {
                mnemonic += " " + mnemonic_part;
            }
        }

        // Store the instruction in binary form
        inst_mem.push_back(bitset<32>(stoul(hex_code, nullptr, 16)));
        // Store the mnemonic as provided in the input
        mnemonics.push_back(mnemonic);
        inst_count++;
    }
    infile.close();

    // Initialize pipeline stages with spaces
    pipeline_stages.resize(inst_count, vector<string>(cycle_count_global, "  "));
}

void process_stalls() {
    for (int i = 0; i < inst_count; i++) {
        int c = 1;
        while(c < cycle_count_global){
            if(pipeline_stages[i][c] == pipeline_stages[i][c-1] && pipeline_stages[i][c] != "  " && pipeline_stages[i][c] != "  "){
                int j = c;
                while(j < cycle_count_global && pipeline_stages[i][j] == pipeline_stages[i][c-1]){
                    j++;
                }
                while(c < j){
                    pipeline_stages[i][c] = " - ";
                    c++;
                }
            }
            else c++;
        }
    }
}

// void print_pipeline() {
//     // Step 1: Print the header row
//     cout << "Inst                |";
//     for (int c = 1; c <= cycle_count_global; c++) {
//         // Each cycle column is 6 characters wide, centered
//         cout << " " << setw(3) << c << " |";
//     }
//     cout << endl;

//     // Step 2: Print the separator row
//     cout << "--------------------|";
//     for (int c = 0; c < cycle_count_global; c++) {
//         cout << "-----|";
//     }
//     cout << endl;

//     // Step 3: Print each instruction and its pipeline stages
//     for (int i = 0; i < inst_count; i++) {
//         // Mnemonic column: Fixed width of 20 characters, left-aligned
//         cout << setw(20) << left << mnemonics[i] << "|";
        
//         // Pipeline stages: Each stage is 6 characters wide, with the label padded to 3 characters
//         for (int c = 0; c < cycle_count_global; c++) {
//             string stage = pipeline_stages[i][c];
//             // Pad the stage label to 3 characters
//             if (stage == "  ") {
//                 stage = "   "; // Empty stage: 3 spaces
//             } else if (stage == " - ") {
//                 stage = " - "; // Stall: Pad to 3 characters
//             } else if (stage == "IF" || stage == "ID" || stage == "EX" || stage == "WB") {
//                 stage = stage + " "; // Pad 2-character labels to 3 characters
//             } else if (stage == "MEM") {
//                 stage = "MEM"; // Already 3 characters
//             }
//             cout << " " << stage << " |";
//         }
//         cout << endl;
//     }
// }

void print_pipeline() {
    for (int i = 0; i < inst_count; i++) {
        cout << mnemonics[i] << ";";
        for (int c = 0; c < cycle_count_global; c++) {
            string stage = pipeline_stages[i][c];
            if (stage == "  ") {
                cout << " "; 
            } else if (stage == " - ") {
                cout << "-"; 
            } else {
                cout << stage; 
            }
            if (c < cycle_count_global - 1) {
                cout << ";";
            }
        }
        cout << endl; 
    }
}

// void print_pipeline() {
//     for (int i = 0; i < inst_count; i++) {
//         cout << mnemonics[i];
//         for (int c = 0; c < cycle_count_global; c++) {
//             cout << ";" << pipeline_stages[i][c];
//         }
//         cout << endl;
//     }
// }

void instruction_fetch(int cycle) {
    //DEBUG
    // cout<<"Instruction Fetch"<<endl;
    int idx = cycle - 1;
    if (if_stall && pc.valid && pc.pc /4 < inst_count) {
        // if(cycle == 8) cout << "Hayee" << endl;
        // if(cycle == 6) cout << "Hi4" << endl;
        // if(cycle == 6){
        //     cout << "if_id.pc : " << if_id.pc << " pc.pc : " << pc.pc << endl;
        // }
        pc.pc -= 4;
        if (pc.valid && pc.pc / 4  < inst_count) {
            pipeline_stages[pc.pc / 4][idx] = "IF";
            if_id.inst = inst_mem[pc.pc / 4];
        }
        if_id.pc = pc.pc;
        pc.pc += 4;
        // if_id.valid = false;
        if_id.valid = true;
        if_stall = false;
        // if(cycle == 5){
        //     //print all latches pc values
        //     cout << "IF/ID: " << if_id.pc << endl;
        //     cout << "ID/EX: " << id_ex.pc << endl;
        //     cout << "EX/MEM: " << ex_mem.pc << endl;
        //     cout << "MEM/WB: " << mem_wb.pc << endl;
        //     cout << "PC: " << pc.pc << endl;
        //     cout << "stall count: " << stall_count << endl;
        //     cout << "stall: " << stall << endl;
        //     cout << "if_stall: " << if_stall << endl;
        // }
        return; // Previous IF remains
    }
    if (ex_jump && (cycle == prev_cycle + 1) && new_addr / 4 < inst_count) {
        // if(cycle == 8) cout << "Hayee" << endl;
        // if(cycle == 5) cout << "Hi3" << endl;
        pc.pc = new_addr;
        ex_jump = false;
        if_id.inst = inst_mem[pc.pc / 4];
        if_id.pc = pc.pc;
        if_id.valid = true;
        if (pc.pc / 4 < inst_count) {
            pipeline_stages[pc.pc / 4][idx] = "IF";
        }
        pc.pc += 4;
        return;
    }
    if (ex_branch && (cycle == prev_cycle + 1) && new_addr / 4 < inst_count) {
        // if(cycle == 8) cout << "Hayee" << endl;
        // if(cycle == 5) cout << "Hi2" << endl;
        ex_branch = false;
        pc.pc = new_addr;
        if_id.inst = inst_mem[pc.pc / 4];
        if_id.pc = pc.pc;
        if_id.valid = true;
        if (pc.pc / 4 < inst_count) {
            pipeline_stages[pc.pc / 4][idx] = "IF";
        }
        pc.pc += 4;
        return;
    }
    if (pc.valid && pc.pc / 4 < inst_count) {
        // if(cycle == 5) cout << "Hi1" << endl;
        if_id.inst = inst_mem[pc.pc / 4];
        if_id.pc = pc.pc;
        if_id.valid = true;
        pc.pc += 4;
        pipeline_stages[pc.pc / 4 - 1][idx] = "IF";
    } else {
        // if(cycle == 5) cout << "Hi1" << endl;
        // if(cycle == 5){
        //     //print all latches pc values
        //     cout << "IF/ID: " << if_id.pc << endl;
        //     cout << "ID/EX: " << id_ex.pc << endl;
        //     cout << "EX/MEM: " << ex_mem.pc << endl;
        //     cout << "MEM/WB: " << mem_wb.pc << endl;
        //     cout << "PC: " << pc.pc << endl;
        //     cout << "stall count: " << stall_count << endl;
        //     cout << "stall: " << stall << endl;
        //     cout << "if_stall: " << if_stall << endl;
        // }
        if_id.valid = false;
        if_id.pc = pc.pc;
        kill = false;
        // pc.pc += 4;
    }
    pc.branch = 0;
}

void instruction_decode(int cycle) {
    // if(cycle == 7) cout << reg[6] << endl;
    //DEBUG
    // cout<<"Instruction Decode"<<endl;
    int idx = cycle - 1;
    if (!if_id.valid && !stall) {
        id_ex.valid = false;
        return;
    }
    // if(cycle == 6) cout << "Hi" << endl;
    // if(cycle == 6){
    //     //print all latches pc values
    //     cout << "IF/ID: " << if_id.pc << endl;
    //     cout << "ID/EX: " << id_ex.pc << endl;
    //     cout << "EX/MEM: " << ex_mem.pc << endl;
    //     cout << "MEM/WB: " << mem_wb.pc << endl;
    //     cout << "PC: " << pc.pc << endl;
    //     cout << "stall count: " << stall_count << endl;
    //     cout << "stall: " << stall << endl;
    //     cout << "if_stall: " << if_stall << endl;
    // }

    bitset<32> inst;
    if (stall) {
        stall_count--;
        if (stall_count > 0) {
            if (id_ex.pc / 4 < inst_count) {
                pipeline_stages[id_ex.pc / 4][idx] = "ID"; // Stall keeps it in ID
            }
            id_ex.valid = false;
            if_stall = true;
            return;
        } else {
            id_ex.valid = true;
            stall = false;
            if_stall = true;
            inst = id_ex.inst;
            // if(cycle == 6) cout << "rs1: " << (int)((inst.to_ulong() >> 15) & 0b11111) << "rs2: " << (int)((inst.to_ulong() >> 20) & 0b11111) << endl;
        }
    }
    else {
        inst = if_id.inst;
        id_ex.pc = if_id.pc;
    }

    bitset<7> opcode(inst.to_ulong() & 0b1111111);
    bitset<3> funct3((inst.to_ulong() >> 12) & 0b111);
    bitset<7> funct7((inst.to_ulong() >> 25) & 0b1111111);
    int rs1 = (int)((inst.to_ulong() >> 15) & 0b11111);
    int rs2 = (int)((inst.to_ulong() >> 20) & 0b11111);
    int rd = (int)((inst.to_ulong() >> 7) & 0b11111);

    // if(cycle == 6) cout << "rs1: " << rs1 << " rs2: " << rs2 << " rd: " << rd << endl;
    if (kill) {
        id_ex.inst = inst;
        id_ex.pc = if_id.pc;
        id_ex.rs1 = 0;
        id_ex.rs2 = 0;
        id_ex.rd = 0;
        id_ex.imm = 0;
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
    // if(cycle == 6) cout << "hello" << endl;

    int imm = 0;
    id_ex.inst = inst;
    // id_ex.pc = if_id.pc;
    id_ex.rs1 = 0;
    id_ex.rs2 = 0;
    id_ex.rd = 0;
    id_ex.imm = 0;
    id_ex.branch = 0;
    id_ex.mem_read = 0;
    id_ex.mem_write = 0;
    id_ex.mem_to_reg = 0;
    id_ex.reg_write = 0;
    id_ex.valid = true;

    int branch_op;

    switch (opcode.to_ulong()) {
        case 0b0110011: // R-type
            id_ex.rd = rd;
            id_ex.rs1 = rs1;
            id_ex.rs2 = rs2;
            id_ex.reg_write = 1;
            id_ex.alu_src = 0;
            //based on funct3 and funct7 values, set alu_op
            if (funct3 == 0b000 && funct7 == 0b0000000) id_ex.alu_op = 0; // ADD
            else if (funct3 == 0b000 && funct7 == 0b0100000) id_ex.alu_op = 1; // SUB
            else if (funct3 == 0b100 && funct7 == 0b0000000) id_ex.alu_op = 2; // XOR
            else if (funct3 == 0b110 && funct7 == 0b0000000) id_ex.alu_op = 4; // OR
            else if (funct3 == 0b111 && funct7 == 0b0000000) id_ex.alu_op = 3; // AND
            else if (funct3 == 0b001 && funct7 == 0b0000000) id_ex.alu_op = 5; // SLL
            else if (funct3 == 0b101 && funct7 == 0b0000000) id_ex.alu_op = 6; // SRL
            else if (funct3 == 0b101 && funct7 == 0b0100000) id_ex.alu_op = 7; // SRA 
            else if (funct3 == 0b010 && funct7 == 0b0000000) id_ex.alu_op = 8; // SLT
            else if (funct3 == 0b011 && funct7 == 0b0000000) id_ex.alu_op = 9; // SLTU
            break;
    
        case 0b0010011: // I-type arithmetic instructions including shifts
            id_ex.rd = rd;
            id_ex.rs1 = rs1;
            id_ex.rs2 = 0;
            id_ex.reg_write = 1;
            id_ex.alu_src = 1;
            // Distinguish between shift instructions and other arithmetic immediates
            if (funct3.to_ulong() == 0b001) { // SLLI
                // Only lower 5 bits are used; no sign extension
                imm = (int)((inst.to_ulong() >> 20) & 0x1F);
                id_ex.imm = imm;
                id_ex.alu_op = 5; // SLL
            } else if (funct3.to_ulong() == 0b101) {
                if (funct7.to_ulong() == 0b0000000) { // SRLI
                    imm = (int)((inst.to_ulong() >> 20) & 0x1F);
                    id_ex.imm = imm;
                    id_ex.alu_op = 6; // SRL
                } else if (funct7.to_ulong() == 0b0100000) { // SRAI
                    imm = (int)((inst.to_ulong() >> 20) & 0x1F);
                    id_ex.imm = imm;
                    id_ex.alu_op = 7; // SRA
                }
            } else {
                // Regular arithmetic immediate instructions (e.g., ADDI, SLTI, XORI, ORI, ANDI)
                imm = (int)(inst.to_ulong() >> 20) & 0xFFF;
                if (imm & 0x800) imm |= 0xFFFFF000; // Sign extend 12-bit immediate
                id_ex.imm = imm;
                if (funct3.to_ulong() == 0b000)
                    id_ex.alu_op = 0; // ADDI
                else if (funct3.to_ulong() == 0b010)
                    id_ex.alu_op = 8; // SLTI
                else if (funct3.to_ulong() == 0b011)
                    id_ex.alu_op = 9; // SLTIU
                else if (funct3.to_ulong() == 0b100)
                    id_ex.alu_op = 2; // XORI
                else if (funct3.to_ulong() == 0b110)
                    id_ex.alu_op = 4; // ORI
                else if (funct3.to_ulong() == 0b111)
                    id_ex.alu_op = 3; // ANDI
            }
            break;
        case 0b0000011: { // I-type loads
            id_ex.rd = rd;
            id_ex.rs1 = rs1;
            id_ex.rs2 = 0;
            imm = (int)(inst.to_ulong() >> 20) & 0xFFF;
            if (imm & 0x800) 
                imm |= 0xFFFFF000; // Sign extend 12-bit immediate
            id_ex.imm = imm;
            id_ex.reg_write = 1;
            id_ex.mem_read = 1;
            id_ex.mem_to_reg = 1;
            // Determine load type based on funct3
            switch (funct3.to_ulong()) {
                case 0b000: // LB
                    id_ex.mem_op = 0;
                    break;
                case 0b001: // LH
                    id_ex.mem_op = 1;
                    break;
                case 0b010: // LW
                    id_ex.mem_op = 2;
                    break;
                case 0b100: // LBU
                    id_ex.mem_op = 3;
                    break;
                case 0b101: // LHU
                    id_ex.mem_op = 4;
                    break;
                default:
                    id_ex.mem_op = 2; // Default to LW
            }
            break;
        }
    
        case 0b0100011: { // S-type stores
            id_ex.rs1 = rs1;
            id_ex.rs2 = rs2;
            id_ex.rd = 0;
            imm = (((int)((inst.to_ulong() >> 25) & 0b1111111)) << 5) |
                   ((int)((inst.to_ulong() >> 7) & 0b11111));
            if (imm & 0x800)
                imm |= 0xFFFFF000; // Sign extend
            id_ex.imm = imm;
            id_ex.mem_write = 1;
            // Determine store type based on funct3
            switch (funct3.to_ulong()) {
                case 0b000: // SB
                    id_ex.mem_op = 0;
                    break;
                case 0b001: // SH
                    id_ex.mem_op = 1;
                    break;
                case 0b010: // SW
                    id_ex.mem_op = 2;
                    break;
                default:
                    id_ex.mem_op = 2; // Default to SW
            }
            break;
        }
    
        case 0b1100011: // SB-type (branch)
            id_ex.rs1 = rs1;
            id_ex.rs2 = rs2;
            id_ex.rd = 0;
            imm = ((inst[31] << 12) |
                   ((inst.to_ulong() >> 25 & 0b111111) << 5) |
                   ((inst.to_ulong() >> 8 & 0b1111) << 1) |
                   ((inst.to_ulong() >> 7 & 0b1) << 11));
            if (inst[31]) imm |= 0xFFFFF000; // Sign extend
            id_ex.imm = imm;
            id_ex.branch = 1;
            //set branchop according to funct3
            if (funct3 == 0b000) branch_op = 0; // BEQ
            else if (funct3 == 0b001) branch_op = 1; // BNE
            else if (funct3 == 0b100) branch_op = 2; // BLT
            else if (funct3 == 0b101) branch_op = 3; // BGE
            else if (funct3 == 0b110) branch_op = 4; // BLTU
            else if (funct3 == 0b111) branch_op = 5; // BGEU
            break;
    
        case 0b1101111: // UJ-type (jal)
            id_ex.rs1 = 0;
            id_ex.rs2 = 0;
            id_ex.rd = rd;
            imm = ((inst[31] << 20) |
                   ((inst.to_ulong() >> 21 & 0x3FF) << 1) |
                   ((inst[20] & 1) << 11) |
                   ((inst.to_ulong() >> 12 & 0xFF) << 12));
            if (inst[31]) imm |= 0xFFF00000; // Sign extend
            id_ex.imm = imm;
            id_ex.reg_write = 1;
            id_ex.branch = 2;
            break;
    
        case 0b1100111: // I-type (jalr)
            id_ex.rd = rd;
            id_ex.rs1 = rs1;
            id_ex.rs2 = 0;
            imm = (int)(inst.to_ulong() >> 20) & 0xFFF;
            if (imm & 0x800) imm |= 0xFFFFF000; // Sign extend
            id_ex.imm = imm;
            id_ex.reg_write = 1;
            id_ex.branch = 3;
            break;
    
        default:
            break;
    }

    // Hazard detection
    if (opcode.to_ulong() == 0b1100011) {  // Branch (B-type)
        if (ex_mem.valid && ex_mem.mem_read && ex_mem.rd != 0 && (ex_mem.rd == rs1 || ex_mem.rd == rs2)) {
            // if(cycle == 6) cout << "Hello from branch just after load" << endl;
            stall = true;
            stall_count = 2;
            id_ex.valid = false;
            pipeline_stages[id_ex.pc / 4][idx] = "ID";
            id_ex.pc = if_id.pc;
            id_ex.inst = inst;
            return;
        }
        if (mem_wb.valid && mem_wb.mem_read && mem_wb.rd != 0 && (mem_wb.rd == rs1 || mem_wb.rd == rs2)) {
            // if(cycle == 6) cout << "Hello from branch 2 instructions after load" << endl;
            stall = true;
            stall_count = 1;
            id_ex.valid = false;
            pipeline_stages[id_ex.pc / 4][idx] = "ID";
            id_ex.pc = if_id.pc;
            id_ex.inst = inst;
            return;
        }
        if (ex_mem.valid && ex_mem.reg_write && !ex_mem.mem_read && ex_mem.rd != 0 && (ex_mem.rd == rs1 || ex_mem.rd == rs2)) {
            // if(cycle == 6) cout << "Hello from branch just after R-type" << endl;
            stall = true;
            stall_count = 1;
            id_ex.valid = false;
            pipeline_stages[if_id.pc / 4][idx] = "ID";
            id_ex.pc = if_id.pc;
            id_ex.inst = inst;
            return;
        }
    } else if (opcode.to_ulong() != 0b0100011){  // Non-branch
        // cout << "Hi, non branch instruction" << endl;
        //print info about previous instruction for debugging, like what is the instruction, wether it is valid, etc.
        // cout << "ex_mem.valid: " << ex_mem.valid << " ex_mem.mem_read: " << ex_mem.mem_read << " ex_mem.rd: " << ex_mem.rd << " rs1: " << rs1 << " rs2: " << rs2 << endl;
        // cout << "ex_mem.inst: " << ex_mem.inst << " ex_mem.pc: " << ex_mem.pc << " ex_mem.rs1: " << ex_mem.rs1 << " ex_mem.rs2: " << ex_mem.rs2 << " ex_mem.rd: " << ex_mem.rd << endl;
        // cout << "ex_mem.valid: " << ex_mem.valid << " ex_mem.mem_read: " << ex_mem.mem_read << " ex_mem.rd: " << ex_mem.rd << " rs1: " << rs1 << " rs2: " << rs2 << endl;
        if (ex_mem.valid && ex_mem.mem_read && ex_mem.rd != 0 && (ex_mem.rd == rs1 || ex_mem.rd == rs2)) {
            // cout << "Hello from non-branch just after load" << endl;
            stall = true;
            stall_count = 1;
            id_ex.valid = false;
            pipeline_stages[if_id.pc / 4][idx] = "ID";
            id_ex.pc = if_id.pc;
            id_ex.inst = inst;
            return;
        }
        else{
            if (ex_mem.valid && ex_mem.mem_read && ex_mem.rd != 0 && (ex_mem.rd == rs2)) {
                // cout << "Hello from non-branch just after load" << endl;
                stall = true;
                stall_count = 1;
                id_ex.valid = false;
                pipeline_stages[if_id.pc / 4][idx] = "ID";
                id_ex.pc = if_id.pc;
                id_ex.inst = inst;
                return;
            }
        }
    }
    // if(cycle == 7) cout << "hello from ID" << endl;
    // cout << reg[6] << endl;
    bool v1 = false;
    bool v2 = false;
    //check for forwarding and assign values to id_ex.val1 and id_ex.val2
    // if(cycle == 5) cout << "ex_mem.valid: " << ex_mem.valid << " ex_mem.reg_write: " << ex_mem.reg_write << " ex_mem.rd: " << ex_mem.rd << " rs1: " << rs1 << " rs2: " << rs2 << endl;
    // if(cycle == 5) cout << "mem_wb.valid: " << mem_wb.valid << " mem_wb.reg_write: " << mem_wb.reg_write << " mem_wb.rd: " << mem_wb.rd << " rs1: " << rs1 << " rs2: " << rs2 << endl;
    if (ex_mem.valid && ex_mem.reg_write && ex_mem.rd != 0 && (ex_mem.rd == rs1 || ex_mem.rd == rs2)){
        if (ex_mem.rd == rs1) {
            id_ex.val1 = ex_mem.rd_val;
            v1 = true;
        } else if (ex_mem.rd == rs2) {
            // if(cycle == 5) cout << " hi " << endl;
            id_ex.val2 = ex_mem.rd_val;
            v2 = true;
        }
    }
    // if(cycle == 5) cout << "ex_mem.rd: " << ex_mem.rd << " ex_mem.rd_val: " << ex_mem.rd_val << endl;
    else if(mem_wb.valid && mem_wb.reg_write && mem_wb.rd != 0 && (mem_wb.rd == rs1 || mem_wb.rd == rs2)){
        if (mem_wb.rd == rs1){
            id_ex.val1 = mem_wb.rd_val;
            v1 = true;
        }
        else if (mem_wb.rd == rs2){
            id_ex.val2 = mem_wb.rd_val;
            v2 = true;
        }
    }


    if(!v1) id_ex.val1 = reg[rs1];
    if(!v2) id_ex.val2 = reg[rs2];

    int val1 = id_ex.val1;
    int val2 = id_ex.val2;

    // cout << "cycle: " << cycle << " ID stage " <<" : val1: " << val1 << ", val2: " << val2 << " value in reg " << rs1 << ": " << reg[rs1] << ", value in reg " << rs2 << ": " << reg[rs2] << endl;

    if (id_ex.branch == 2) {
        ex_jump = true;
        new_addr = id_ex.pc + id_ex.imm;
        prev_cycle = cycle;
        if_id.valid = false;
        kill = true;
        id_ex.rd_val = id_ex.pc + 4;
        id_ex.rd = 1;
    }
    else if (id_ex.branch == 3) {
        ex_jump = true;
        new_addr = reg[rs1] + id_ex.imm;
        prev_cycle = cycle;
        if_id.valid = false;
        kill = true;
        id_ex.rd_val = id_ex.pc + 4;
        id_ex.rd = 1;
    }

    if (id_ex.branch == 1) {
        ex_branch = true;
        new_addr = id_ex.pc + id_ex.imm;
        prev_cycle = cycle;
        //based on branchop, check whether to branch or not
        if (branch_op == 0) { // BEQ
            if (val1 == val2) {
                new_addr = id_ex.pc + id_ex.imm;
                prev_cycle = cycle;
                ex_branch = true;
                kill = true;
            } else ex_branch = false;

        } else if (branch_op == 1) { // BNE
            if (val1 != val2) {
                new_addr = id_ex.pc + id_ex.imm;
                prev_cycle = cycle;
                ex_branch = true;
                kill = true;
            } else ex_branch = false;

        } else if (branch_op == 2) { // BLT
            // if(cycle == 5) cout << " val1: " << val1 << ",  val2 : " << val2 << endl;
            if (val1 < val2) {
                new_addr = id_ex.pc + id_ex.imm;
                prev_cycle = cycle;
                ex_branch = true;
                kill = true;
            } else ex_branch = false;

        } else if (branch_op == 3) { // BGE
            if (val1 >= val2) {
                new_addr = id_ex.pc + id_ex.imm;
                prev_cycle = cycle;
                ex_branch = true;
                kill = true;
            } else ex_branch = false;

        } else if (branch_op == 4) { // BLTU
            if ((unsigned int)val1 < (unsigned int)val2) {
                new_addr = id_ex.pc + id_ex.imm;
                prev_cycle = cycle;
                ex_branch = true;
                kill = true;
            } else ex_branch = false;

        } else if (branch_op == 5) { // BGEU
            if ((unsigned int)reg[rs1] >= (unsigned int)reg[rs2]) {
                new_addr = id_ex.pc + id_ex.imm;
                prev_cycle = cycle;
                ex_branch = true;
                kill = true;
            } else ex_branch = false;
        }
    }
    // if(cycle == 6) cout << " Haaayeee "  << if_id.pc << endl;
    pipeline_stages[id_ex.pc / 4][idx] = "ID";
}

void execute(int cycle) {
    //DEBUG
    // cout<<"Execute"<<endl;
    int idx = cycle - 1;
    if (!id_ex.valid) {
        ex_mem.valid = false;
        ex_mem.pc = id_ex.pc;
        ex_mem.inst = id_ex.inst;
        return; // Previous stage (EX) remains
    }

    //write code for ALU operations here (currently only add and addi)
    bitset<32> inst = id_ex.inst;
    bitset<7> opcode(inst.to_ulong() & 0b1111111);
    int rs1 = id_ex.rs1;
    int rs2 = id_ex.rs2;

    bool v1 = false;
    bool v2 = false;
    int val1;
    int val2;
    //first check whether value needs to be forwarded or not, from ex_mem or mem_wb and also check val2 to be taken from imm or rs2
    if (ex_mem.valid && ex_mem.reg_write && !ex_mem.mem_read && ex_mem.rd != 0 && (ex_mem.rd == rs1 || ex_mem.rd == rs2)) {
        if (ex_mem.rd == rs1) {
            val1 = ex_mem.rd_val;
            v1 = true;
        } else if (ex_mem.rd == rs2 && id_ex.alu_src != 0) {
            val2 = ex_mem.rd_val;
            v2 = true;
        }
    } else if (mem_wb.valid && mem_wb.mem_read && mem_wb.rd != 0 && (mem_wb.rd == rs1 || mem_wb.rd == rs2)) {
        if (mem_wb.rd == rs1) {
            val1 = mem_wb.mem_data;
            v1 = true;
        } else if (mem_wb.rd == rs2 && id_ex.alu_src != 0) {
            val2 = mem_wb.mem_data;
            v2 = true;
        }
    }

    if (!v1) val1 = reg[rs1];
    if (!v2) {
        if (id_ex.alu_src == 0) val2 = reg[rs2];
        else val2 = id_ex.imm;
    }

    int result;
    //based on alu_op control signal of id_ex stage, perform the alu operations and store in result:
    switch (id_ex.alu_op) {
        case 0: // ADD
            result = val1 + val2;
            break;
        case 1: // SUB
            result = val1 - val2;
            break;
        case 2: // XOR
            result = val1 ^ val2;
            break;
        case 3: // AND
            result = val1 & val2;
            break;
        case 4: // OR
            result = val1 | val2;
            break;
        case 5: // SLL
            result = val1 << (val2 & 0x1F);
            break;
        case 6: // SRL
            result = (unsigned int)val1 >> (val2 & 0x1F);
            break;
        case 7: // SRA
            result = val1 >> (val2 & 0x1F);
            break;
        case 8: // SLT
            result = (val1 < val2) ? 1 : 0;
            break;
        case 9: // SLTU
            result = ((unsigned int)val1 < (unsigned int)val2) ? 1 : 0;
            break;
    }
    
    ex_mem.inst = id_ex.inst;
    ex_mem.pc = id_ex.pc;
    ex_mem.rs1 = id_ex.rs1;
    ex_mem.rs2 = id_ex.rs2;
    ex_mem.rd = id_ex.rd;
    ex_mem.rd_val = result;
    ex_mem.branch = id_ex.branch;
    ex_mem.mem_read = id_ex.mem_read;
    ex_mem.mem_write = id_ex.mem_write;
    ex_mem.mem_to_reg = id_ex.mem_to_reg;
    ex_mem.reg_write = id_ex.reg_write;
    ex_mem.valid = true;


    // cout << "cycle " << cycle << " EX stage calculated value : " << ex_mem.rd_val << endl;

    pipeline_stages[id_ex.pc / 4][idx] = "EX";
}

void memory(int cycle) {
    int idx = cycle - 1;
    // If there is no valid instruction in EX_MEM, pass along an invalid WB stage.
    if (!ex_mem.valid) {
        mem_wb.inst = ex_mem.inst;
        mem_wb.pc = ex_mem.pc;
        mem_wb.valid = false;
        return;
    }
    
    int effective_addr = ex_mem.rd_val;
    
    int store_val = 0;
    if (ex_mem.mem_write) {
        store_val = reg[ex_mem.rs2];
        // forwarding
        if (mem_wb.valid && mem_wb.reg_write && mem_wb.rd != 0 && mem_wb.rd == ex_mem.rs2) {
            // If previous instruction was a load, use its loaded value, otherwise ALU result.
            store_val = mem_wb.mem_read ? mem_wb.mem_data : mem_wb.rd_val;
        }
    }
    
    int mem_result = 0;
    //load 
    if (ex_mem.mem_read) {
        // data memory is word-addressed via key = effective_addr / 4.
        int word_addr = effective_addr / 4;
        int word_val = 0;
        if (data_mem.find(word_addr) != data_mem.end())
            word_val = data_mem[word_addr];
        
        switch(id_ex.mem_op) {
            case 0: { //load byte, sign-extended
                int offset = effective_addr % 4;
                int byte_val = (word_val >> (offset * 8)) & 0xFF;
                if (byte_val & 0x80) byte_val |= 0xFFFFFF00;
                mem_result = byte_val;
                break;
            }
            case 1: { //load halfword, sign-extended
                int offset = effective_addr % 4;
                int half_val = (word_val >> (offset * 8)) & 0xFFFF;
                if (half_val & 0x8000) half_val |= 0xFFFF0000;
                mem_result = half_val;
                break;
            }
            case 2: { //load word
                mem_result = word_val;
                break;
            }
            case 3: { //load byte, zero-extended
                int offset = effective_addr % 4;
                mem_result = (word_val >> (offset * 8)) & 0xFF;
                break;
            }
            case 4: { //load halfword, zero-extended
                int offset = effective_addr % 4;
                mem_result = (word_val >> (offset * 8)) & 0xFFFF;
                break;
            }
            default:
                mem_result = word_val;
        }
        mem_wb.mem_data = mem_result;
    }
    
    // store instructions
    if (ex_mem.mem_write) {
        int word_addr = effective_addr / 4;
        int current_word = 0;
        if (data_mem.find(word_addr) != data_mem.end())
            current_word = data_mem[word_addr];
        switch(id_ex.mem_op) {
            case 0: { //store byte
                int offset = effective_addr % 4;
                current_word &= ~(0xFF << (offset * 8)); // Clear target byte.
                current_word |= ((store_val & 0xFF) << (offset * 8));
                data_mem[word_addr] = current_word;
                break;
            }
            case 1: { // store halfword
                int offset = effective_addr % 4;
                current_word &= ~(0xFFFF << (offset * 8)); // Clear target halfword.
                current_word |= ((store_val & 0xFFFF) << (offset * 8));
                data_mem[word_addr] = current_word;
                break;
            }
            case 2: { //store word
                data_mem[word_addr] = store_val;
                break;
            }
            default:
                data_mem[word_addr] = store_val;
        }
    }
    
    // cout << "cycle " << cycle << " MEM stage : " << " mem data : " << mem_result << endl;

    // Prepare the MEM_WB pipeline latch.
    mem_wb.inst = ex_mem.inst;
    mem_wb.pc = ex_mem.pc;
    mem_wb.rd = ex_mem.rd;
    mem_wb.rs1 = ex_mem.rs1;
    mem_wb.rs2 = ex_mem.rs2;
    mem_wb.rd_val = ex_mem.rd_val;
    mem_wb.branch = ex_mem.branch;
    mem_wb.mem_data = ex_mem.mem_read ? mem_result: 0;
    mem_wb.mem_read = ex_mem.mem_read;
    mem_wb.mem_to_reg = ex_mem.mem_to_reg;
    mem_wb.reg_write = ex_mem.reg_write;
    mem_wb.valid = true;
    
    pipeline_stages[ex_mem.pc / 4][idx] = "MEM";
}

void write_back(int cycle) {
    //DEBUG
    // cout<<"Write Back"<<endl;
    int idx = cycle - 1;
    if (!mem_wb.valid) {
        return; // Previous stage (WB) remains
    }

    // if the instruction is load type then load the mem data into the register, else if it is R-type then load the rd_val in the register, else do nothing
    if (mem_wb.reg_write){ 
        if (mem_wb.mem_read) {
            reg[mem_wb.rd] = mem_wb.mem_data;
        } else if (mem_wb.reg_write && mem_wb.rd != 0) {
            reg[mem_wb.rd] = mem_wb.rd_val;
        }
        // cout << "cycle " << cycle << " WB stage wrote " << reg[mem_wb.rd] << " to reg " << mem_wb.rd << endl;
    }
    else{
        // cout << "cycle " << cycle << " WB stage did nothing" << endl;
    }

    if(mem_wb.branch == 2 || mem_wb.branch == 3){
        reg[1] = mem_wb.rd_val;
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

    pipeline_stages[mem_wb.pc / 4][idx] = "WB";
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cout << "Usage: ./forward ../inputfiles/filename.txt cyclecount" << endl;
        return 1;
    }

    string filename = argv[1];
    cycle_count_global = stoi(argv[2]);

    load_instructions(filename);


    //write a string at some location in data memory and provide its base address in x10, to check the program (string length program)

    for (int cycle = 1; cycle <= cycle_count_global; cycle++) {
        write_back(cycle);
        memory(cycle);
        execute(cycle);
        instruction_decode(cycle);
        instruction_fetch(cycle);
    }

    process_stalls(); // Ensure stalls are processed
    print_pipeline();
    
    // print all registers
    // cout << "Registers:" << endl;
    // for (int i = 0; i < 32; i++) {
    //     cout << "x" << i << ": " << reg[i] << endl;
    // }

    //testing strcopy function, check whether data copied at location data_mem[260] or not
    // cout << (data_mem[256] == data_mem[260]) << endl;

    return 0;
}