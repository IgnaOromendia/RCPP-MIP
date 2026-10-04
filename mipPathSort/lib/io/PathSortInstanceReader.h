#ifndef PATH_SORT_INSTANCE_READER_H
#define PATH_SORT_INSTANCE_READER_H

#include "../model/PathSortInstance.h"
#include <model/Instance.h>
#include <iosfwd>
#include <string>

class PathSortInstanceReader {
public:
    static PathSortInstance read(const Instance& graph_instance,
                                 std::istream& solution,
                                 const std::string& solution_name = "solucion");
    static PathSortInstance read_file(const Instance& graph_instance,
                                      const std::string& solution_path = "out.dat");
    static PathSortInstance read_files(const std::string& graph_path,
                                       const std::string& turns_path,
                                       const std::string& solution_path = "out.dat");
};

#endif
