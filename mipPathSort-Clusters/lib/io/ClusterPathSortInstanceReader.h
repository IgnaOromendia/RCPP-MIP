#ifndef CLUSTER_PATH_SORT_INSTANCE_READER_H
#define CLUSTER_PATH_SORT_INSTANCE_READER_H

#include <model/ClusterPathSortInstance.h>
#include <iosfwd>
#include <string>

class ClusterPathSortInstanceReader {
public:
    static ClusterPathSortInstance read(
        PathSortInstance instance,
        std::istream& clusters,
        const std::string& cluster_name = "clusters");
    static ClusterPathSortInstance read_file(
        PathSortInstance instance,
        const std::string& cluster_path);
    static ClusterPathSortInstance read_files(
        const std::string& graph_path,
        const std::string& turns_path,
        const std::string& solution_path,
        const std::string& cluster_path);
};

#endif
