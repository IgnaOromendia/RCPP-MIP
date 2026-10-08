#ifndef PATH_ORDER_WRITER_H
#define PATH_ORDER_WRITER_H

#include <model/OrderedPass.h>
#include <iosfwd>
#include <string>
#include <vector>

class PathOrderWriter {
public:
    static void write(std::ostream& output, const std::vector<OrderedPass>& order,
                      int deposit);
    static void write_file(const std::string& path, const std::vector<OrderedPass>& order,
                           int deposit);
    static void write_segments(std::ostream& output,
                               const std::vector<OrderedPass>& order);
    static void write_cluster_segments(std::ostream& output,
                                       const std::vector<OrderedPass>& order,
                                       const std::vector<int>& edge_clusters);
    static void write_segments_file(const std::string& path,
                                    const std::vector<OrderedPass>& order);
    static void write_cluster_segments_file(
        const std::string& path, const std::vector<OrderedPass>& order,
        const std::vector<int>& edge_clusters);
};

#endif
