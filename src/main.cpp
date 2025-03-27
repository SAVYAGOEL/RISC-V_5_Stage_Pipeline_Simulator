#include "processor.hpp"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: ./noforward <inputfile> <cyclecount>" << std::endl;
        return 1;
    }

    std::string filename = argv[1];
    int cycles = 0;

    try {
        cycles = std::stoi(argv[2]);
        if (cycles <= 0) {
            std::cerr << "Error: Cycle count must be positive." << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: Invalid cycle count '" << argv[2] << "'." << std::endl;
        return 1;
    }

    try {
        Processor simulator;
        simulator.loadProgramFromFile(filename);
        simulator.runSimulation(cycles);
        simulator.displayPipeline();
    } catch (const std::exception& e) {
        std::cerr << "Runtime Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}