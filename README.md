# RISC-V_5_Stage_Pipeline_Simulator
5-stage pipelined processor (with forwarding and without forwarding)


For non-forwarding


Design decisions for non forwarding:

1. Data hazards (RAW) are resolved only by stalling the pipeline. Data needed in the Decode stage must wait until the instruction producing it completes the Writeback stage.
2. Register writes occur in the first half of the WB stage, and register reads occur in the second half of the ID stage.
3. Branch assumed to be not taken: so next instruction is in pipeline directly itself. So after resolution in ID phase, there is no need for flushing the instruction.
4. Unconditional jumps (jal, jalr) calculate target addresses and determine the outcome (taken/not taken) in the EX stage. Instructions fetched after the branch/jump but before it resolves in EX need to be flushed since it's an unconditional jump.
5. Flush Mechanism: When EX detects a taken branch or an unconditional jump, it signals a flush. The Decode stage, in the next cycle, acts on this signal to invalidate the instruction currently entering ID (fetched incorrectly) and insert a bubble into the ID/EX latch.
6. IF starts only when the ID of the previous instruction can be executed

Issues with non forwarding:
1. Could not extend the logic to register and memory storage.
2. Does not support the following instructions
3. The code does not support ecall, ebreak and fence.



Sources:
1. Berkeley repo for reference for some logic
2. I used AI for generating the makefile
3. Since I am not familiar with C++, I used grok.com for some help on implementation part.  I used Gemini AI for  C++ syntax fixing and knowing how to use standard libraries (fstream, stoi, etc.). I used AI for the makefile. I also used it to explain to me the code in the scala files. The actual coding of the pipeline, hazard, stalling/flushing was my part



For forwarding


Design decisions for forwarding:

1. Used forwarding/bypassing concept to reduce the number of stalls in the pipeline.
2. Branches and jump instructions are resolved in the ID stage, meaning that any value which is needed in the ID stage to resolve these instructions, is waited for and hence the instruction is appropriately stalled in the same stage for as long as the value in unavailable.
3. Assumed that branch is always not taken so fetched the next instruction in the pipeline after the branch or jump instruction, but based on the result of the branch or jump, the instruction is either killed or continued in the pipeline and in the next cycle the correct instruction is fetched.
4. Used C++ inbuilt map to model the data memory, an integer vector to model the registers and a vector to model the Instruction memory
5. Fully functional forwarding has been implemented, which checks for all possible latches from where the data can be forwarded, effectively avoiding data hazards as well as reducing the unnecessary number of stalls in the pipeline.


Issues with forwarding:
None encountered till now but does not support ecall, ebreak etc. All other common instructions have been supported and provided operations for.

Sources:
1. Used the course textbook to look for some forwarding and stalling logic, also for some opcodes.
2. Used the following link to see all the instructions in the architecture :
https://www.cs.sfu.ca/~ashriram/Courses/CS295/assets/notebooks/RISCV/RISCV_CARD.pdf
3. Used ChatGPT for help in parsing input and writing to data memory.
