#ifndef CLI_OPTIONS_H
#define CLI_OPTIONS_H

#include <string>

struct CliOptions {
    bool help = false;
    std::string graph_path, turns_path;

    static CliOptions parse(int argc, const char* const* argv);
};

#endif
