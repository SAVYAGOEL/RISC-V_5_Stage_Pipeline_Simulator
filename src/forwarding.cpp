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
    int mem_read;
    int mem_write;
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
    int mem_read;
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
int data_mem[1024 * 1024] = {0};
IF_ID if_id = {bitset<32>(0), 0, false};
ID_EX id_ex = {bitset<32>(0), 0, 0, 0, 0, 0, 0, 0, 0, 0, false};
EX_MEM ex_mem = {bitset<32>(0), 0, 0, 0, 0, 0, 0, 0, 0, 0, false};
MEM_WB mem_wb = {bitset<32>(0), 0, 0, 0, 0, 0, 0, 0, 0, false};
WB_IF wb_if = {bitset<32>(0), 0, 0, 0, 0, 0, 0, 0, 0, false};
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
        for (int c = 1; c < cycle_count_global; c++) {
            // Check if the current stage is the same as the previous stage
            if (pipeline_stages[i][c] == pipeline_stages[i][c - 1] && 
                pipeline_stages[i][c] != "  " && // Ignore empty stages
                pipeline_stages[i][c] != " - ") {   // Ignore already marked stalls
                pipeline_stages[i][c] = " - ";      // Replace with a stall
            }
        }
    }
}

void print_pipeline() {
    // Step 1: Print the header row
    cout << "Inst                |";
    for (int c = 1; c <= cycle_count_global; c++) {
        // Each cycle column is 6 characters wide, centered
        cout << " " << setw(3) << c << " |";
    }
    cout << endl;

    // Step 2: Print the separator row
    cout << "--------------------|";
    for (int c = 0; c < cycle_count_global; c++) {
        cout << "-----|";
    }
    cout << endl;

    // Step 3: Print each instruction and its pipeline stages
    for (int i = 0; i < inst_count; i++) {
        // Mnemonic column: Fixed width of 20 characters, left-aligned
        cout << setw(20) << left << mnemonics[i] << "|";
        
        // Pipeline stages: Each stage is 6 characters wide, with the label padded to 3 characters
        for (int c = 0; c < cycle_count_global; c++) {
            string stage = pipeline_stages[i][c];
            // Pad the stage label to 3 characters
            if (stage == "  ") {
                stage = "   "; // Empty stage: 3 spaces
            } else if (stage == " - ") {
                stage = " - "; // Stall: Pad to 3 characters
            } else if (stage == "IF" || stage == "ID" || stage == "EX" || stage == "WB") {
                stage = stage + " "; // Pad 2-character labels to 3 characters
            } else if (stage == "MEM") {
                stage = "MEM"; // Already 3 characters
            }
            cout << " " << stage << " |";
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
        if(cycle == 6) cout << "Hi4" << endl;
        if(cycle == 6){
            cout << "if_id.pc : " << if_id.pc << " pc.pc : " << pc.pc << endl;
        }
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
    if (ex_jump && (cycle == prev_cycle + 1)) {
        // if(cycle == 8) cout << "Hayee" << endl;
        // if(cycle == 5) cout << "Hi3" << endl;
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
        // if(cycle == 8) cout << "Hayee" << endl;
        // if(cycle == 5) cout << "Hi2" << endl;
        ex_branch = false;
        if_id.inst = inst_mem[pc.pc / 4];
        if_id.pc = pc.pc;
        if_id.valid = true;
        if (pc.pc / 4 < inst_count) {
            pipeline_stages[pc.pc / 4][idx] = "IF";
        }
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
        // pc.pc += 4;
    }
    pc.branch = 0;
}

void instruction_decode(int cycle) {
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

    switch (opcode.to_ulong()) {
        case 0b0110011: // R-type
            id_ex.rd = rd;
            id_ex.rs1 = rs1;
            id_ex.rs2 = rs2;
            id_ex.reg_write = 1;
            break;
    
        case 0b0010011: // I-type (arithmetic)
            id_ex.rd = rd;
            id_ex.rs1 = rs1;
            id_ex.rs2 = 0;
            imm = (int)(inst.to_ulong() >> 20) & 0xFFF;
            if (imm & 0x800) imm |= 0xFFFFF000; // Sign extend
            id_ex.imm = imm;
            id_ex.reg_write = 1;
            break;
        case 0b0000011: // I-type (load)
            id_ex.rd = rd;
            id_ex.rs1 = rs1;
            id_ex.rs2 = 0;
            imm = (int)(inst.to_ulong() >> 20) & 0xFFF;
            if (imm & 0x800) imm |= 0xFFFFF000; // Sign extend
            id_ex.imm = imm;
            id_ex.reg_write = 1;
            id_ex.mem_read = 1;
            id_ex.mem_to_reg = 1;
            id_ex.valid = true;
            break;
    
        case 0b0100011: // S-type (store)
            id_ex.rs1 = rs1;
            id_ex.rs2 = rs2;
            id_ex.rd = 0;
            imm = ((int)((inst.to_ulong() >> 25) & 0b1111111) << 5) |
                  ((int)((inst.to_ulong() >> 7) & 0b11111));
            if (imm & 0x800) imm |= 0xFFFFF000; // Sign extend
            id_ex.imm = imm;
            id_ex.mem_write = 1;
            break;
    
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

    // if(cycle == 6) cout << " Haaayeee " << endl;

    if (id_ex.branch == 2 || id_ex.branch == 3) {
        ex_jump = true;
        new_addr = id_ex.pc + id_ex.imm;
        prev_cycle = cycle;
        if_id.valid = false;
        kill = true;
    }

    // if (id_ex.branch == 1) {
    //     // if(cycle == 6) cout << " Haaayeee "  << if_id.pc << endl;
    //     ex_branch = true;
    // }
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

    pipeline_stages[id_ex.pc / 4][idx] = "EX";
}

void memory(int cycle) {
    //DEBUG
    // cout<<"Memory"<<endl;
    int idx = cycle - 1;
    if (!ex_mem.valid) {
        mem_wb.inst = ex_mem.inst;
        mem_wb.pc = ex_mem.pc;
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

    wb_if.inst = mem_wb.inst;
    wb_if.pc = mem_wb.pc;
    wb_if.rd = mem_wb.rd;
    wb_if.rs1 = mem_wb.rs1;
    wb_if.rs2 = mem_wb.rs2;
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

    load_instructions(filename);

    for (int cycle = 1; cycle <= cycle_count_global; cycle++) {
        write_back(cycle);
        memory(cycle);
        execute(cycle);
        instruction_decode(cycle);
        instruction_fetch(cycle);
    }

    process_stalls(); // Ensure stalls are processed
    print_pipeline();
    return 0;
}