// RISC-V simulator for RV32I base integer instruction set
// 5-stage pipelined processor with forwarding


//need to make all the structs for all the latches
//we are making 4 latches and PC as a separate latch
//latch 1 - between IF and ID stage
//latch 2 - between ID and EX stage
//latch 3 - between EX and MEM stage
//latch 4 - between MEM and WB stage
//latch 5 / PC latch - between WB and IF stage or can say before the IF stage (to update PC)

//latch 1
typedef struct IF_ID
{
    int inst;
    int pc;
    bool valid;
} IF_ID;

//note that branches are to be decided after the ID stage
//latch 2
typedef struct ID_EX
{
    int inst;
    int pc;
    int rs1;
    int rs2;
    int rd;
    int imm; // after shifting or whatever
    int data1;
    int data2;
    int data1_ready;
    int data2_ready;
    int rd_ready;
    int alu_src;
    int alu_op;
    int branch;
    int mem_read;
    int mem_write;
    int mem_to_reg;
    int reg_write;
    int valid;
} ID_EX;

//latch 3
typedef struct EX_MEM
{
    int inst;
    int pc;
    int rd;
    int rd_val;
    int rd_ready;
    int rd_tag;
    int mem_op;
    int alu_op;
    int branch;
    int mem_read;
    int mem_write;
    int mem_to_reg;
    int reg_write;
    int valid;
} EX_MEM;

//latch 4
typedef struct MEM_WB
{
    int inst;
    int pc;
    int rd;
    int rd_val;
    int rd_ready;
    int rd_tag;
    int mem_op;
    int mem_to_reg;
    int reg_write;
    int alu_op;
    int valid;
} MEM_WB;

//latch 5
typedef struct PC
{
    int pc;
    int pc_src;
    int branch;
    int branch_addr;
    int valid;
} PC;

//control unit
typedef struct control_unit
{
    int reg_write;
    int mem_read;
    int mem_write;
    int mem_to_reg;
    int alu_op;
    int alu_src;
    int branch;
    int valid;
} control_unit;

//forwarding unit  (will be in the EX stage, bcz ALU MUXes are in this stage)
typedef struct forwarding_unit
{
    int fwdA; //0 - no fwd, 1 - fwd from EX/MEM, 2 - fwd from MEM/WB
    int fwdB; //0 - no fwd, 1 - fwd from EX/MEM, 2 - fwd from MEM/WB
    int fwdA_val;
    int fwdB_val;
    int valid;
} forwarding_unit;

//EX hazard
// if (EX/MEM.RegWrite
//     and (EX/MEM.RegisterRd ≠ 0)
//     and (EX/MEM.RegisterRd = ID/EX.RegisterRs1)) ForwardA = 1
//     if  (EX/MEM.RegWrite
//     and (EX/MEM.RegisterRd ≠ 0)
//     and (EX/MEM.RegisterRd = ID/EX.RegisterRs2)) ForwardB = 1
// This case forwards the result from the previous instruction to either
// input of the ALU. If the previous instruction is going to write to the
// register file, and the write register number matches the read register
// number of ALU inputs A or B, provided it is not register 0, then
// steer the multiplexor to pick the value instead from the pipeline
// register EX/MEM.

//MEM hazard
// if (MEM/WB.RegWrite
//     and (MEM/WB.RegisterRd ≠ 0)
//     and not(EX/MEM.RegWrite and (EX/MEM.RegisterRd ≠ 0)
//         and (EX/MEM.RegisterRd = ID/EX.RegisterRs1))
//     and (MEM/WB.RegisterRd = ID/EX.RegisterRs1)) ForwardA = 2
//     if (MEM/WB.RegWrite
//     and (MEM/WB.RegisterRd ≠ 0)
//     and not(EX/MEM.RegWrite and (EX/MEM.RegisterRd ≠ 0)
//   and (EX/MEM.RegisterRd = ID/EX.RegisterRs2))
//   and (MEM/WB.RegisterRd = ID/EX.RegisterRs2)) ForwardB = 2
// This case forwards the result from the previous instruction to either
// input of the ALU. If the previous instruction is going to write to the
// register file, and the write register number matches the read register
// number of ALU inputs A or B, provided it is not register 0, then
// steer the multiplexor to pick the value instead from the pipeline
// register MEM/WB.

//one case where forwarding cannot save the day is when an instruction tries to read a register following a load instruction that writes the same register

//hazard detection unit
typedef struct hazard_detection_unit
{
    int fwdA;
    int fwdB;
    int valid;
} hazard_detection_unit;

// Checking for load instructions, the control for the hazard detection
// unit is this single condition:
//   if (ID/EX.MemRead and
//     ((ID/EX.RegisterRd = IF/ID.RegisterRs1) or
//       (ID/EX.RegisterRd = IF/ID.RegisterRs2)))
//       stall the pipeline

// If the instruction in the ID stage is stalled, then the instruction in the IF stage 
//must also be stalled; otherwise, we would lose the fetched instruction. Preventing these 
//two instructions from making progress is accomplished simply by preventing the PC register 
//and the IF/ID pipeline register from changing. Provided these registers are preserved, 
//the instruction in the IF stage will continue to be read using the same PC, and the registers
// in the ID stage will continue to be read using the same instruction fields in the IF/ID pipeline register

// we see that deasserting all seven control signals (setting them to 0) in the EX, MEM, and WB stages 
// will create a “do nothing” or nop instruction. By identifying the hazard in the ID stage, we can 
// insert a bubble into the pipeline by changing the EX, MEM, and WB control fields of the ID/EX pipeline register to 0.

//control hazard:
//predict that the conditional branch will not be taken and thus continue execution down the sequential instruction
//stream. If the conditional branch is taken, the instructions that are being fetched and decoded must be discarded.
//Execution continues at the branch target. If conditional branches are untaken half the time, and if it costs 
//little to discard the instructions, this optimization halves the cost of control hazards.

//To discard instructions, we merely change the original control values to 0s, much as we did to stall for a load-use
//data hazard. The difference is that we must also change the three instructions in the IF, ID, and EX stages when 
//the branch reaches the MEM stage; for load-use stalls, we just change control to 0 in the ID stage and let them 
//percolate through the pipeline. Discarding instructions, then, means we must be able to flush instructions 
//in the IF, ID, and EX stages of the pipeline.

//1. During ID, we must decode the instruction, decide whether a bypass to the equality test unit is needed, and complete 
//the equality test so that if the instruction is a branch, we can set the PC to the branch target address. 
//Forwarding for the operand of branches was formerly handled by the ALU forwarding logic, but the introduction 
//of the equality test unit in ID will require new forwarding logic. Note that the bypassed source operands of a 
//branch can come from either the EX/MEM or MEM/WB pipeline registers.

//2. Because the value in a branch comparison is needed during ID but may be produced later in time, it is possible 
//that a data hazard can occur and a stall will be needed. For example, if an ALU instruction immediately preceding
//a branch produces the operand for the test in the conditional branch, a stall will be required, since the EX stage 
//for the ALU instruction will occur after the ID cycle of the branch. By extension, if a load is immediately followed 
//by a conditional branch that depends on the load result, two stall cycles will be needed, as the result from the 
//load appears at the end of the MEM cycle but is needed at the beginning of ID for the branch.

//To flush instructions in the IF stage, we add a control line, called IF.Flush, that zeros the instruction field of 
//the IF/ID pipeline register. Clearing the register transforms the fetched instruction into a nop, an instruction 
//that has no action and changes no state.