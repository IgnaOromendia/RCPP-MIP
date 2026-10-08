#include <io/PathOrderWriter.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        const std::vector<OrderedPass> order = {
            {1, 1, {20, -2, 9, 0, 1, 0, 1}},
            {2, 1, {3, 0, 0, 1, 1, 1, 0, 4, 7}},
            {3, 1, {4, -1, 1, 2, 1, 0, 1}},
            {4, 1, {5, 1, 2, 3, 2, 0, 1, 7, 4}},
            {5, 2, {5, 1, 2, 3, 2, 0, 1, 7, 4}},
            {6, 1, {21, -2, 3, 9, 2, 0, 1}},
        };
        std::ostringstream output;
        PathOrderWriter::write_segments(output, order);
        check(output.str() ==
              "vehiculo,orden,nodo_origen,nodo_destino\n"
              "1,2,5,8\n"
              "2,4,8,5\n"
              "2,5,8,5\n",
              "CSV must retain original directions, repeated passes and global order");

        const std::vector<OrderedPass> clustered_order = {
            {1, 1, {3, 0, 0, 1, 1, 1, 0, 4, 7}, 1},
            {2, 1, {4, 1, 1, 2, 1, 0, 1, 7, 4}, 0},
        };
        std::ostringstream clustered_output;
        PathOrderWriter::write_cluster_segments(
            clustered_output, clustered_order, {2, 0});
        check(clustered_output.str() ==
              "vehiculo,orden,nodo_origen,nodo_destino,cluster\n"
              "1,1,5,8,1\n"
              "1,2,8,5,3\n",
              "Cluster CSV must export the one-based cluster of every pass");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
