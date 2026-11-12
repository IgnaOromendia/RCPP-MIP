#ifndef SUBTOUR_BREAKER_H
#define SUBTOUR_BREAKER_H

#include <graph/NeutralExpandedGraph.h>
#include <graph/SuperGraph.h>
#include <model/Solution.h>

struct ProvisionalConnection {
    int from, to;
    long long count;
};

class SubtourBreaker {
public:
    SubtourBreaker(const SuperGraph& graph, const NeutralExpandedGraph& expanded,
        const Solution& solution, const vector<int>& neutral_arc_ids);

    const Solution& get_solution() const { return _solution; }
    const vector<ProvisionalConnection>& get_connections() const { return _connections; }
    bool was_repaired() const { return _repaired; }

private:
    Solution _solution;
    vector<ProvisionalConnection> _connections;
    bool _repaired = false;

    vector<int> components(const SuperGraph& graph) const;
};

#endif
