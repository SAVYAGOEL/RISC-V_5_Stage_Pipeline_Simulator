#include <iostream>
#include <vector>
#include <string>
#include <iomanip>

using namespace std;

// Structure for pipeline registers between stages
struct PipelineLatch {
    bool valid = false;
    uint32_t pc = 0;
    uint32_t inst = 0x00000013; // NOP instruction
    uint32_t rs1_addr = 0;
    uint32_t rs2_addr = 0;
    uint32_t wb_addr = 0;
    bool wb_en = false;
};

// Control signals for each stage
struct ControlSignals {
    bool stall = false;
    bool kill = false;
};

class PipelineStage {
protected:
    string name;
public:
    PipelineStage(string n) : name(n) {}
    virtual void execute(PipelineLatch& in, PipelineLatch& out, vector<string>& diagram, int cycle) = 0;
    void print(int cycle, vector<string>& diagram) {
        if (cycle < diagram.size()) {
            diagram[cycle] += name + "\t";
        }
    }
};

class IF_Stage : public PipelineStage {
public:
    IF_Stage() : PipelineStage("IF") {}
    void execute(PipelineLatch& in, PipelineLatch& out, vector<string>& diagram, int cycle) override {
        if (!in.valid) {
            out.valid = false;
            return;
        }
        out.valid = true;
        out.pc = in.pc;
        out.inst = in.inst;
        print(cycle, diagram);
    }
};

class ID_Stage : public PipelineStage {
public:
    ID_Stage() : PipelineStage("ID") {}
    void execute(PipelineLatch& in, PipelineLatch& out, vector<string>& diagram, int cycle) override {
        if (!in.valid) {
            out.valid = false;
            return;
        }
        out.valid = true;
        out.pc = in.pc;
        out.inst = in.inst;
        out.rs1_addr = (in.inst >> 15) & 0x1F;
        out.rs2_addr = (in.inst >> 20) & 0x1F;
        out.wb_addr = (in.inst >> 7) & 0x1F;
        out.wb_en = true; // Assuming write-back for simplicity
        print(cycle, diagram);
    }
};

class EX_Stage : public PipelineStage {
    PipelineLatch* mem_latch;
    PipelineLatch* wb_latch;
public:
    EX_Stage(PipelineLatch* m, PipelineLatch* w) : PipelineStage("EX"), mem_latch(m), wb_latch(w) {}
    void execute(PipelineLatch& in, PipelineLatch& out, vector<string>& diagram, int cycle) override {
        if (!in.valid) {
            out.valid = false;
            return;
        }
        // Check for hazards with forwarding
        bool stall = ((mem_latch->valid && mem_latch->wb_en && 
                      (mem_latch->wb_addr == in.rs1_addr || mem_latch->wb_addr == in.rs2_addr)) ||
                      (wb_latch->valid && wb_latch->wb_en && 
                      (wb_latch->wb_addr == in.rs1_addr || wb_latch->wb_addr == in.rs2_addr)));
        
        if (stall) {
            out.valid = false;
            diagram[cycle] += "STALL\t";
        } else {
            out.valid = true;
            out.pc = in.pc;
            out.inst = in.inst;
            out.wb_addr = in.wb_addr;
            out.wb_en = in.wb_en;
            print(cycle, diagram);
        }
    }
};

class MEM_Stage : public PipelineStage {
public:
    MEM_Stage() : PipelineStage("MEM") {}
    void execute(PipelineLatch& in, PipelineLatch& out, vector<string>& diagram, int cycle) override {
        if (!in.valid) {
            out.valid = false;
            return;
        }
        out.valid = true;
        out.pc = in.pc;
        out.inst = in.inst;
        out.wb_addr = in.wb_addr;
        out.wb_en = in.wb_en;
        print(cycle, diagram);
    }
};

class WB_Stage : public PipelineStage {
public:
    WB_Stage() : PipelineStage("WB") {}
    void execute(PipelineLatch& in, PipelineLatch& out, vector<string>& diagram, int cycle) override {
        if (!in.valid) {
            out.valid = false;
            return;
        }
        out.valid = true;
        out.pc = in.pc;
        out.inst = in.inst;
        out.wb_addr = in.wb_addr;
        out.wb_en = in.wb_en;
        print(cycle, diagram);
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cout << "Usage: ./forward <inputfile> <cyclecount>" << endl;
        return 1;
    }

    int cycle_count = stoi(argv[2]);
    vector<string> pipeline_diagram(cycle_count, "");

    // Pipeline latches
    PipelineLatch if_id, id_ex, ex_mem, mem_wb;
    if_id.valid = true; // Simulate instruction fetch

    // Pipeline stages
    IF_Stage if_stage;
    ID_Stage id_stage;
    EX_Stage ex_stage(&ex_mem, &mem_wb);
    MEM_Stage mem_stage;
    WB_Stage wb_stage;

    // Simulate pipeline for given cycles
    for (int cycle = 0; cycle < cycle_count; ++cycle) {
        PipelineLatch next_if_id, next_id_ex, next_ex_mem, next_mem_wb;

        wb_stage.execute(mem_wb, next_mem_wb, pipeline_diagram, cycle);
        mem_stage.execute(ex_mem, mem_wb, pipeline_diagram, cycle);
        ex_stage.execute(id_ex, ex_mem, pipeline_diagram, cycle);
        id_stage.execute(if_id, id_ex, pipeline_diagram, cycle);
        if_stage.execute(if_id, next_if_id, pipeline_diagram, cycle);

        if_id = next_if_id;
        id_ex = next_id_ex;
        ex_mem = next_ex_mem;
        mem_wb = next_mem_wb;

        if_id.pc += 4; // Simulate PC increment
    }

    // Print pipeline diagram
    for (int i = 0; i < cycle_count; ++i) {
        cout << "Cycle " << i << ":\t" << pipeline_diagram[i] << endl;
    }

    return 0;
}