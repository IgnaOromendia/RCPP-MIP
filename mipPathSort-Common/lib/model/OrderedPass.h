#ifndef ORDERED_PASS_H
#define ORDERED_PASS_H

#include <model/PathSortInstance.h>

struct OrderedPass {
    int position = 0;
    int pass = 0;
    PathEdge edge;
};

#endif
