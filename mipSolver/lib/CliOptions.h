#ifndef CLI_OPTIONS_H
#define CLI_OPTIONS_H

#include "ModelOptions.h"
#include <string>

struct CliOptions {
    bool help = false;
    std::string graph_path, turns_path;
    std::string output_path = "out.dat";
    double gap = 0;
    int cuts_mode = -1;
    ModelOptions model;

    static CliOptions parse(int argc, const char* const* argv);
};

#endif
