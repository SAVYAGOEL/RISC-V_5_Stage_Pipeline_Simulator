# COL216-Assignment2-Processor
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



