#ifndef SOLUTION_WRITER_H
#define SOLUTION_WRITER_H

#include "../model/Solution.h"
#include <ostream>
#include <string>

class SolutionWriter {
public:
    static void write(std::ostream& output, const Solution& solution);
    static void write_file(const std::string& path, const Solution& solution);
};

#endif
