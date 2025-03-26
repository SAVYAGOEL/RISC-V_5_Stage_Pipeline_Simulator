#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cstdlib>

using namespace std;

// Control signals struct
struct ControlSignals {
    bool rf_wen;
    bool mem_val;
    string br_type;
    ControlSignals() {
        rf_wen = false;
        mem_val = false;
        br_type = "BR_N";
    }
};

// Pipeline latch structs
struct IFID_Latch {
    int inst;
    bool valid;
    IFID_Latch() {
        inst = -1;
        valid = false;
    }
};

struct IDEX_Latch {
    int inst;
    bool valid;
    int rs1_addr;
    int rs2_addr;
    int wbaddr;
    ControlSignals ctrl;
    IDEX_Latch() {
        inst = -1;
        valid = false;
        rs1_addr = 0;
        rs2_addr = 0;
        wbaddr = 0;
    }
};

struct EXMEM_Latch {
    int inst;
    bool valid;
    int wbaddr;
    ControlSignals ctrl;
    EXMEM_Latch() {
        inst = -1;
        valid = false;
        wbaddr = 0;
    }
};

struct MEMWB_Latch {
    int inst;
    bool valid;
    int wbaddr;
    ControlSignals ctrl;
    MEMWB_Latch() {
        inst = -1;
        valid = false;
        wbaddr = 0;
    }
};

// Register file class
class RegisterFile {
    vector<int> regs;
public:
    RegisterFile() : regs(32, 0) {}
    int read(int addr) { return (addr == 0) ? 0 : regs[addr]; }
    void write(int addr, int data) { if (addr != 0) regs[addr] = data; }
};

// No Forwarding Processor
class NoForwardingProcessor {
    RegisterFile regfile;
    IFID_Latch ifid;
    IDEX_Latch idex;
    EXMEM_Latch exmem;
    MEMWB_Latch memwb;
    vector<string> pipeline_diagram;
    vector<int> instructions; // Simulated instruction sequence
    int cycle;
    int inst_count;
    int total_cycles;

    void printStage(int inst, string stage) {
        if (inst >= pipeline_diagram.size()) pipeline_diagram.resize(inst + 1, "");
        string& row = pipeline_diagram[inst];
        if (cycle >= row.size() / 3) row += string((cycle - row.size() / 3) * 3, ' ');
        row += stage + " ";
    }

    void loadInstructions(const string& filename) {
        ifstream infile(filename);
        if (!infile.is_open()) {
            cerr << "Warning: Could not open " << filename << ". Using dummy instructions.\n";
            // Simulate 5 instructions with dummy register dependencies
            instructions.push_back(0);
            instructions.push_back(1);
            instructions.push_back(2);
            instructions.push_back(3);
            instructions.push_back(4);
            return;
        }
        int inst;
        while (infile >> inst) {
            instructions.push_back(inst);
        }
        infile.close();
    }

public:
    NoForwardingProcessor(const string& filename, int cycles) {
        cycle = 0;
        inst_count = 0;
        total_cycles = cycles;
        pipeline_diagram.push_back("");
        loadInstructions(filename);
    }

    void fetch() {
        if (inst_count < instructions.size() && !idex.valid) { // Only fetch if no stall
            ifid.inst = inst_count++;
            ifid.valid = true;
            printStage(ifid.inst, "IF");
        }
    }

    void decode() {
        if (!ifid.valid) return;
        IDEX_Latch temp;
        temp.inst = ifid.inst;
        temp.valid = true;
        temp.rs1_addr = (temp.inst % 5) + 1;  // Simulate rs1 dependency (e.g., x1-x5)
        temp.rs2_addr = (temp.inst % 5) + 2;  // Simulate rs2 dependency (e.g., x2-x6)
        temp.wbaddr = (temp.inst % 5) + 3;    // Simulate wbaddr (e.g., x3-x7)
        temp.ctrl.rf_wen = true;              // Assume all instructions write back

        // RAW hazard check
        bool hazard = (temp.rs1_addr != 0 && (
            (idex.valid && idex.ctrl.rf_wen && idex.wbaddr == temp.rs1_addr) ||
            (exmem.valid && exmem.ctrl.rf_wen && exmem.wbaddr == temp.rs1_addr) ||
            (memwb.valid && memwb.ctrl.rf_wen && memwb.wbaddr == temp.rs1_addr))) ||
                      (temp.rs2_addr != 0 && (
            (idex.valid && idex.ctrl.rf_wen && idex.wbaddr == temp.rs2_addr) ||
            (exmem.valid && exmem.ctrl.rf_wen && exmem.wbaddr == temp.rs2_addr) ||
            (memwb.valid && memwb.ctrl.rf_wen && memwb.wbaddr == temp.rs2_addr)));

        if (hazard) {
            printStage(temp.inst, "ST");  // Stall
            idex.valid = false;           // Insert bubble into EX
        } else {
            idex = temp;
            printStage(idex.inst, "ID");
            ifid.valid = false;
        }
    }

    void execute() {
        if (!idex.valid) return;
        exmem.inst = idex.inst;
        exmem.valid = true;
        exmem.wbaddr = idex.wbaddr;
        exmem.ctrl = idex.ctrl;
        printStage(exmem.inst, "EX");
        idex.valid = false;
    }

    void memory() {
        if (!exmem.valid) return;
        memwb.inst = exmem.inst;
        memwb.valid = true;
        memwb.wbaddr = exmem.wbaddr;
        memwb.ctrl = exmem.ctrl;
        printStage(memwb.inst, "MEM");
        exmem.valid = false;
    }

    void writeback() {
        if (!memwb.valid) return;
        regfile.write(memwb.wbaddr, memwb.inst);  // Dummy write
        printStage(memwb.inst, "WB");
        memwb.valid = false;
    }

    void run() {
        for (cycle = 0; cycle < total_cycles; cycle++) {
            writeback();
            memory();
            execute();
            decode();
            fetch();
        }
        cout << "Cycle: ";
        for (int c = 0; c < total_cycles; c++) cout << c << "  ";
        cout << "\n";
        for (int i = 0; i < pipeline_diagram.size(); i++) {
            cout << "Inst" << i << ": " << pipeline_diagram[i] << "\n";
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <inputfile.txt> <cyclecount>\n";
        return 1;
    }
    string filename = argv[1];
    int cyclecount = atoi(argv[2]);
    if (cyclecount <= 0) {
        cerr << "Error: cyclecount must be positive\n";
        return 1;
    }

    NoForwardingProcessor proc(filename, cyclecount);
    proc.run();
    return 0;
}