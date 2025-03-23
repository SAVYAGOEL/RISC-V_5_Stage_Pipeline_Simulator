#ifndef PROCESSOR_HPP
#define PROCESSOR_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <map>

const int NUM_REGS = 32;

extern std::vector<int> reg;
extern int mem[1024];

class IF {
public:
    uint32_t pc;
    std::vector<uint32_t>& instr_mem;

    IF(std::vector<uint32_t>& memory) : pc(0), instr_mem(memory) {}
    uint32_t fetch();
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
    uint32_t pc = 0;
    int alu_result = 0;
    int rs2_val = 0;
    int rs2 = 0;
    int rd = 0;
    ControlSignals control;
};

struct MEMWB {
    uint32_t pc = 0;
    int alu_result = 0;
    int rd = 0;
    bool reg_write = false;
};

class EX {
public:
    int op;
    int a1;
    int a2;

    int out();
};

class ID {
public:
    bool branch_decision;
    int a1;
    int a2;
    int dec; // beq=0, blt=1, ble=2, bgt=3, bge=4, beqz=5

    int out(int a1, int a2);
};

class MEM {
public:
    int reg_number;
    int op; // 0=no-op, 1=load byte, 2=store word
    int mem_addr;

    int putget();
};

class WB {
public:
    int op;
    int reg_number;
    int value;

    void write();
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
    std::map<uint32_t, std::string> instr_mnemonics; // PC -> mnemonic
    std::map<uint32_t, std::vector<std::string>> pipeline_history; // PC -> stages by cycle

    bool detectHazardID();
    bool detectHazardEX();
    bool detectHazardMEM();
    std::string decodeMnemonic(uint32_t inst);

public:
    NoForwardingProcessor(std::vector<uint32_t>& instructions);
    void runCycle();
    void printPipeline(int cycle);
};

#endif