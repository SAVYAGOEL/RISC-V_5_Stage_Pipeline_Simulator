#include "processor.hpp"
#include <iostream>
#include <string>
#include <stdexcept>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: ./noforward <inputfile> <cyclecount>" << std::endl;
        return 1;
    }

    std::string filename = argv[1];
    int cyclecount = 0;
    try {
        cyclecount = std::stoi(argv[2]);
        if (cyclecount <= 0) {
            throw std::invalid_argument("Cycle count must be positive.");
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: Invalid cycle count '" << argv[2] << "'. " << e.what() << std::endl;
        return 1;
    }

    try {
        Processor risc_v_sim;
        risc_v_sim.loadProgram(filename);
        risc_v_sim.run(cyclecount);
        risc_v_sim.printPipelineDiagram();
    } catch (const std::exception& e) {
        std::cerr << "Runtime Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}