#ifndef WARM_START_H
#define WARM_START_H

#include <vector>

class PathSorterCluster;

class WarmStart {
public:
    explicit WarmStart(PathSorterCluster& sorter);

    void generate();
    bool complete_hierholzer();

private:
    std::vector<int> segment_positions() const;

    PathSorterCluster& _sorter;
};

#endif
