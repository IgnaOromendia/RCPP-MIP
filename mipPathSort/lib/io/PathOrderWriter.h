#ifndef PATH_ORDER_WRITER_H
#define PATH_ORDER_WRITER_H

#include "../model/PathSorter.h"
#include <iosfwd>
#include <string>
#include <vector>

class PathOrderWriter {
public:
    static void write(std::ostream& output, const std::vector<OrderedPass>& order,
                      int deposit);
    static void write_file(const std::string& path, const std::vector<OrderedPass>& order,
                           int deposit);
};

#endif
