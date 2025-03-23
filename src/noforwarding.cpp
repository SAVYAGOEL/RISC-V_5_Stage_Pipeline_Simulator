#include <cstdint>
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>

const int NUM_REGS = 32;

using namespace std;

vector<int> reg(NUM_REGS, 0); // Initialize 32 registers to 0
int mem[1024] = {0};          // Memory array (1024 words)

class IF {
public:
    uint32_t pc;              // Program counter
    vector<uint32_t>& instr_mem; // Reference to instruction memory

    IF(vector<uint32_t>& memory) : pc(0), instr_mem(memory) {} // Constructor

    uint32_t fetch() {
        if (pc < instr_mem.size() * 4) { // Check bounds (4-byte instructions)
            uint32_t instruction = instr_mem[pc / 4];
            pc += 4;                     // Increment PC
            return instruction;
        }
        return 0; // Return NOP (0) if beyond memory
    }
};

struct ControlSignals {
    bool reg_write = false;
    bool mem_read = false;
    bool mem_write = false;
    bool branch = false;
    int alu_op = 0; // 0=ADD, 1=SUB, 2=AND, 3=OR, 4=SLL, 5=NOT
};

struct IFID {
    uint32_t instruction = 0;
    uint32_t pc = 0;
};

struct IDEX {
    uint32_t pc = 0;
    int rs1_val = 0, rs2_val = 0;
    int rs1 = 0, rs2 = 0, rd = 0;
    uint32_t imm = 0;
    ControlSignals control;
};

struct EXMEM {
    int alu_result = 0;
    int rs2_val = 0; // Value to be stored (for sw)
    int rs2 = 0;     // Register index of rs2 (for hazard detection)
    int rd = 0;
    ControlSignals control;
};

struct MEMWB {
    int alu_result = 0;
    int rd = 0;
    bool reg_write = false;
};

class EX {
public:
    int op;  // 0=ADD, 1=SUB, 2=AND, 3=OR, 4=SLL, 5=NOT
    int a1;  // First operand (rs1_val)
    int a2;  // Second operand (rs2_val or imm)

    int out() {
        switch (op) {
            case 0: return a1 + a2;        // ADD
            case 1: return a1 - a2;        // SUB
            case 2: return a1 & a2;        // AND
            case 3: return a1 | a2;        // OR
            case 4: return a1 << (a2 & 0x1F); // SLL (mask to 5 bits)
            case 5: return ~a1;            // NOT (unary, ignores a2)
            default: return 0;             // Undefined
        }
    }
};

class ID {
public:
    bool branch_decision;
    int a1;
    int a2;
    int dec; // beq=0, blt=1, ble=2, bgt=3, bge=4, beqz=5

    int out(int a1, int a2) {
        if (!branch_decision) {
            return 0; // No branch if branch_decision is false
        }

        if (dec == 0) { // beq
            return (a1 == a2) ? 1 : 0;
        }
        else if (dec == 1) { // blt
            return (a1 < a2) ? 1 : 0;
        }
        else if (dec == 2) { // ble
            return (a1 <= a2) ? 1 : 0;
        }
        else if (dec == 3) { // bgt
            return (a1 > a2) ? 1 : 0;
        }
        else if (dec == 4) { // bge
            return (a1 >= a2) ? 1 : 0;
        }
        else if (dec == 5) { // beqz
            return (a1 == 0) ? 1 : 0;
        }

        return 0; // Default: no branch
    }
};

class MEM {
public:
    int reg_number;
    int op; // 0=no-op, 1=load word, 2=store word
    int mem_addr;

    int putget() { // Return value for load, 0 for store/no-op
        if (op == 1) { // Load word
            reg[reg_number] = mem[mem_addr];
            return reg[reg_number];
        }
        else if (op == 2) { // Store word
            mem[mem_addr] = reg[reg_number];
            return 0;
        }
        return 0; // No-op
    }
};

class WB {
public:
    int op;       // 0=no-op, 1=write back
    int reg_number;
    int value;    // Value to write back

    void write() {
        if (op == 1 && reg_number != 0) { // Write back (x0 is read-only)
            reg[reg_number] = value;
        }
    }
};

class NoForwardingProcessor {
private:
    IF if_stage;
    ID id_stage;
    EX ex_stage;
    MEM mem_stage;
    WB wb_stage;
    IFID if_id;
    IDEX id_ex;
    EXMEM ex_mem;
    MEMWB mem_wb;
    bool stall;

    bool detectHazardID() {
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

    bool detectHazardEX() {
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

    bool detectHazardMEM() {
        if (ex_mem.control.mem_write && ex_mem.rs2 != 0 && 
            (mem_wb.reg_write && mem_wb.rd == ex_mem.rs2)) {
            return true;
        }
        return false;
    }

public:
    NoForwardingProcessor(vector<uint32_t>& instructions)
        : if_stage(instructions), stall(false) {}

    void runCycle() {
        // Writeback Stage
        wb_stage.op = mem_wb.reg_write ? 1 : 0;
        wb_stage.reg_number = mem_wb.rd;
        wb_stage.value = mem_wb.alu_result;
        wb_stage.write();

        // Memory Stage
        if (!detectHazardMEM()) {
            mem_stage.reg_number = ex_mem.rd;
            mem_stage.op = ex_mem.control.mem_read ? 1 : (ex_mem.control.mem_write ? 2 : 0);
            mem_stage.mem_addr = ex_mem.alu_result; // Address from ALU
            mem_wb.alu_result = (mem_stage.op == 1) ? mem_stage.putget() : ex_mem.alu_result;
            mem_wb.rd = ex_mem.rd;
            mem_wb.reg_write = ex_mem.control.reg_write;
        } else {
            mem_wb = MEMWB(); // Stall: Insert bubble
        }

        // Execute Stage
        if (!detectHazardEX() && !detectHazardMEM()) {
            ex_stage.op = id_ex.control.alu_op;
            ex_stage.a1 = id_ex.rs1_val;
            ex_stage.a2 = (id_ex.control.alu_op == 5 || id_ex.rs2 == 0) ? id_ex.imm : id_ex.rs2_val; // NOT or I-type
            ex_mem.alu_result = ex_stage.out();
            ex_mem.rs2_val = id_ex.rs2_val; // Pass store value
            ex_mem.rs2 = id_ex.rs2;         // Pass store register index
            ex_mem.rd = id_ex.rd;
            ex_mem.control = id_ex.control;
        } else {
            ex_mem = EXMEM(); // Stall: Insert bubble
        }

        // Decode Stage
        if (!detectHazardID() && !detectHazardEX() && !detectHazardMEM()) {
            uint32_t inst = if_id.instruction;
            int opcode = inst & 0x7F;
            int funct3 = (inst >> 12) & 0x7;
            int funct7 = (inst >> 25) & 0x7F;
            id_ex.rd = (inst >> 7) & 0x1F;
            id_ex.rs1 = (inst >> 15) & 0x1F;
            id_ex.rs2 = (inst >> 20) & 0x1F;
            id_ex.imm = (opcode == 0x13) ? (inst >> 20) : 0; // Simplified I-type imm

            // Control signals
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
                else if (funct3 == 0x4 && id_ex.imm == 0xFFF) id_ex.control.alu_op = 5; // NOT (xori -1)
            } else if (opcode == 0x63) { // Branch
                id_ex.control.branch = true;
                id_stage.branch_decision = true;
                id_stage.dec = funct3; // beq=0, blt=1, etc.
                id_stage.a1 = reg[id_ex.rs1];
                id_stage.a2 = reg[id_ex.rs2];
                if (id_stage.out(id_stage.a1, id_stage.a2)) {
                    if_stage.pc = if_id.pc + id_ex.imm; // Branch taken
                }
            } else if (opcode == 0x23) { // Store (sw)
                id_ex.control.mem_write = true;
            } else if (opcode == 0x03) { // Load (lw)
                id_ex.control.mem_read = true;
                id_ex.control.reg_write = true;
            }

            id_ex.rs1_val = reg[id_ex.rs1];
            id_ex.rs2_val = reg[id_ex.rs2];
            id_ex.pc = if_id.pc;
        } else {
            id_ex = IDEX(); // Stall: Insert bubble
            stall = true;
        }

        // Fetch Stage
        if (!stall) {
            if_id.instruction = if_stage.fetch();
            if_id.pc = if_stage.pc - 4; // PC before increment
        }
        stall = false; // Reset stall flag
    }

    void printPipeline(int cycle) {
        cout << "Cycle " << cycle << ":\n"
             << "IF: 0x" << hex << setw(8) << setfill('0') << if_id.instruction << "\n"
             << "ID: rs1=" << dec << id_ex.rs1 << ", rs2=" << id_ex.rs2 << ", rd=" << id_ex.rd << "\n"
             << "EX: alu=" << ex_mem.alu_result << "\n"
             << "MEM: res=" << mem_wb.alu_result << "\n"
             << "WB: x" << mem_wb.rd << "=" << reg[mem_wb.rd] << "\n\n";
    }
};

int main() {
    vector<uint32_t> instructions = {
        0x00000293, // addi x5, x0, 0
        0x00a28333, // add x6, x5, x10
        0x00030663, // beq x6, x0, end
        0x00532023  // sw x5, 0(x6)
    };
    NoForwardingProcessor proc(instructions);
    for (int i = 0; i < 10; ++i) {
        proc.runCycle();
        proc.printPipeline(i + 1);
    }
    return 0;
}