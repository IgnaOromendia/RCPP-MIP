#ifndef BEST_CLUSTER_PATH_INSTANCE_READER_H
#define BEST_CLUSTER_PATH_INSTANCE_READER_H

#include <model/BestClusterPathInstance.h>
#include <istream>
#include <string>

class BestClusterPathInstanceReader {
public:
    static BestClusterPathInstance read(
        std::istream& graph,
        std::istream& turns,
        std::istream& in_out,
        std::istream& record_clusters,
        const std::string& graph_name = "grafo",
        const std::string& turns_name = "giros",
        const std::string& in_out_name = "aristas in/out",
        const std::string& record_clusters_name = "clusters de registros");

    static BestClusterPathInstance read_files(
        const std::string& graph_path,
        const std::string& turns_path,
        const std::string& in_out_path,
        const std::string& record_clusters_path);
};

#endif
