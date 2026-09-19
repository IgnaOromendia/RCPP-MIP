#include "lib/SolutionWriter.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        Solution solution;
        solution.objective = 7.25;
        solution.service = {{0, 1, 1, 1}};
        solution.traversals = {{0, 1, 1, 0}, {2, 3, 2, 2}};
        solution.deposit_traversals = {{-1, 0, 1, 1}, {1, -1, 1, 1}};
        solution.flow = {{0, 1, 1, 0.25}};
        solution.deposit_flow = {{-1, 0, 1, 0.25}};
        const std::string expected =
            "OBJ: 7.25\n\n---- X ----\nX_1_2_1 = 1\n"
            "\n---- Y ----\nY_1_2_1 = 0\nY_3_4_2 = 2\n"
            "\n---- YDK & YKD ----\nY_D_1_1 = 1\nY_2_D_1 = 1\n"
            "\n---- F ----\nF_1_2_1 = 0.25\n\n---- FDK ----\nF_D_1_1 = 0.25\n";
        std::ostringstream output;
        SolutionWriter::write(output, solution);
        check(output.str() == expected, "Output format and indexing changed");
        std::filesystem::create_directory("custom output");
        SolutionWriter::write_file("custom output/solution.dat", solution);
        std::ifstream file("custom output/solution.dat");
        const std::string contents{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        check(contents == expected && !std::filesystem::exists("out.dat"), "Custom output path");
        std::ostringstream failed;
        failed.setstate(std::ios::badbit);
        bool caught = false;
        try { SolutionWriter::write(failed, solution); }
        catch (const std::runtime_error&) { caught = true; }
        check(caught, "Stream errors must propagate");
        caught = false;
        try { SolutionWriter::write_file("custom output", solution); }
        catch (const std::ios_base::failure&) { caught = true; }
        check(caught, "File errors must propagate");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
