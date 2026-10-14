#ifndef INSTANCE_READER_H
#define INSTANCE_READER_H

#include "../model/Instance.h"
#include <istream>
#include <string>

class InstanceReader {
public:
    static Instance read(std::istream& graph, std::istream& turns,
                         const std::string& graph_name = "grafo",
                         const std::string& turns_name = "giros");
    static Instance read(std::istream& graph, std::istream& turns,
                         std::istream& clusters,
                         const std::string& graph_name = "grafo",
                         const std::string& turns_name = "giros",
                         const std::string& cluster_name = "clusters");
    static Instance read_files(const std::string& graph_path, const std::string& turns_path);
    static Instance read_files(const std::string& graph_path,
                               const std::string& turns_path,
                               const std::string& cluster_path);
};

#endif
