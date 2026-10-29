#include <io/BestClusterPathInstanceReader.h>
#include <model/BestClusterPathInstance.h>

BestClusterPathInstance::BestClusterPathInstance(
    const std::string& graph_path,
    const std::string& turns_path,
    const std::string& in_out_path,
    const std::string& arc_clusters_path)
    : BestClusterPathInstance(BestClusterPathInstanceReader::read_files(
          graph_path, turns_path, in_out_path, arc_clusters_path)) {}
