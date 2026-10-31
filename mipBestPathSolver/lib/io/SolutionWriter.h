#ifndef SOLUTION_WRITER_H
#define SOLUTION_WRITER_H

#include "../model/Solution.h"
#include <filesystem>
#include <ostream>
#include <string>

class SolutionWriter {
public:
    static void write(std::ostream& output, const Solution& solution);
    static void write(std::ostream& output, const GraphSolution& solution);
    static void write_file(const std::string& path, const Solution& solution);
    static void write_file(const std::string& path, const GraphSolution& solution);
    static std::filesystem::path write_output(
        const Solution& solution,
        int nodes,
        const std::filesystem::path& path = {},
        const std::string& filename_prefix = "min_dist");
    static std::filesystem::path write_output(
        const GraphSolution& solution,
        int nodes,
        const std::filesystem::path& path = {},
        const std::string& filename_prefix = "min_dist");
};

#endif
