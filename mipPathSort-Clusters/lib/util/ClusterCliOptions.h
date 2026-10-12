#ifndef CLUSTER_CLI_OPTIONS_H
#define CLUSTER_CLI_OPTIONS_H

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>

struct ClusterOutputPaths {
    std::filesystem::path directory;
    std::string order;
    std::string segments;
    std::string hierholzer_segments;
    bool uses_default_directory;
};

class ClusterCliUsageError : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};

struct ClusterCliOptions {
    std::string graph_path;
    std::string turns_path;
    std::string solution_path;
    std::string clusters_path;
    std::optional<std::string> order_path;
    std::optional<std::string> segments_path;
    int lookahead = 4;
    int branch_width = 8;

    static ClusterCliOptions parse(int argc, const char* const* argv);
    static const char* usage();
    ClusterOutputPaths output_paths(int original_nodes) const;
};

#endif
