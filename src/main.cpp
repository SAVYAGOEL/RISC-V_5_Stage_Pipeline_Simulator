#include "noforwarding.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

using namespace std;

vector<uint32_t> loadInstructions(const string& filename) {
    vector<uint32_t> instructions;
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Could not open " << filename << endl;
        return instructions;
    }
    string line;
    while (getline(file, line)) {
        uint32_t inst;
        stringstream ss(line);
        ss >> hex >> inst;
        instructions.push_back(inst);
    }
    file.close();
    return instructions;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: ./noforward <inputfile> <cyclecount>" << endl;
        return 1;
    }

    string inputFile = argv[1];
    int cycles = stoi(argv[2]);
    vector<uint32_t> instructions = loadInstructions(inputFile);
    if (instructions.empty()) {
        cerr << "Error: No instructions loaded from " << inputFile << endl;
        return 1;
    }

    NoForwardingProcessor proc(instructions);
    for (int i = 0; i < cycles; ++i) {
        proc.runCycle();
        proc.printPipeline(i + 1);
    }

    return 0;
}