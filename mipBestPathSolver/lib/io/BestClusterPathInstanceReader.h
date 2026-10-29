#ifndef BEST_CLUSTER_PATH_INSTANCE_READER_H
#define BEST_CLUSTER_PATH_INSTANCE_READER_H
#include <model/Instance.h>
#include <istream>
#include <string>

class BestClusterPathInstanceReader {
public:
    static Instance read(std::istream& graph, std::istream& turns,
                         std::istream& clusters, std::istream* neutral_nodes = nullptr);
    static Instance read_files(const std::string& graph_path,
                               const std::string& turns_path,
                               const std::string& clusters_path,
                               const std::string& neutral_path = "");
};
#endif
