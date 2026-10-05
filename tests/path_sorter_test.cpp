#include <model/PathSorter.h>
#include <constraints/ModuleConstraintSetter.h>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

using Terms = std::map<IloInt, double>;
using Rows = std::map<std::string, Terms>;

void check_module_constraints() {
    IloEnv env;
    try {
        IloModel model(env);
        const std::vector<PathEdge> edges = {
            {3, 0, 4, 5, 1, 1, 0},
            {7, 1, 5, 9, 1, 0, 1},
            {11, 2, 5, 9, 2, 1, 0},
            {15, -2, 9, 4, 1, 0, 1}
        };
        ArcVariables X(env, edges.size());
        ArcVariables D(env, edges.size());
        for (std::size_t e = 0; e < edges.size(); ++e) {
            X[e] = IloNumVarArray(env, 1, 1, 4, ILOINT);
            D[e] = IloNumVarArray(env, edges.size(), 0, 4, ILOINT);
        }

        ModuleConstraintSetter setter(edges, env, model);
        setter.set_module_constraints(D, X);

        Rows expected;
        const auto add_pair = [&](std::size_t e, std::size_t f) {
            expected["Mod_" + std::to_string(e + 1) + "_" +
                     std::to_string(f + 1) + "_Pos"] = {
                {D[e][f].getId(), 1}, {X[e][0].getId(), -1}, {X[f][0].getId(), 1}
            };
            expected["Mod_" + std::to_string(e + 1) + "_" +
                     std::to_string(f + 1) + "_Neg"] = {
                {D[e][f].getId(), 1}, {X[e][0].getId(), 1}, {X[f][0].getId(), -1}
            };
        };
        add_pair(0, 1);
        add_pair(1, 3);
        add_pair(3, 0);

        for (IloModel::Iterator it(model); it.ok(); ++it) {
            auto* implementation = dynamic_cast<IloRangeI*>((*it).getImpl());
            check(implementation != nullptr, "Module setter added a non-range object");
            IloRange range(implementation);
            const std::string name = range.getName() ? range.getName() : "<unnamed>";
            const auto found = expected.find(name);
            check(found != expected.end(), "Unexpected or duplicate module row: " + name);
            check(range.getLB() == 0 && range.getUB() == IloInfinity,
                  "Incorrect module bounds: " + name);
            Terms actual;
            for (IloExpr::LinearIterator term = range.getLinearIterator(); term.ok(); ++term)
                if (term.getCoef() != 0)
                    actual[term.getVar().getId()] += term.getCoef();
            check(actual == found->second, "Incorrect module terms: " + name);
            expected.erase(found);
        }
        check(expected.empty(), "Missing module constraint");
        env.end();
    } catch (...) {
        env.end();
        throw;
    }
}

int main() {
    try {
        check_module_constraints();

        PathSortInstance instance;
        instance.vehicles = 2;
        instance.edges = {
            {3, 0, 4, 5, 1, 1, 2},
            {7, -1, 5, 8, 2, 0, 1}
        };

        PathSorter sorter(std::move(instance));
        check(sorter.edges().size() == 2, "PathSorter must retain every input edge");
        check(sorter.edges()[0].super_arc_id == 3 &&
              sorter.edges()[0].original_edge_id == 0 &&
              sorter.edges()[0].vehicle == 1 &&
              sorter.edges()[0].times() == 3,
              "PathSorter must consume the already parsed multiplicities");
        check(sorter.edges()[1].original_edge_id == -1 && sorter.edges()[1].times() == 1,
              "PathSorter must retain connector edges");
        check(sorter.pass_counts() == std::vector<int>({3, 1}),
              "PathSorter must define m_e from the incumbent traversal counts");
        check(sorter.total_passes() == 4,
              "PathSorter must define K as the sum of every m_e");

        // Sparse super-arc ids must not be used as indexes into the two local edges.
        sorter.generate_MIP();

        PathSorter empty;
        check(empty.edges().empty(), "Default PathSorter must have no input edges");
        check(empty.pass_counts().empty() && empty.total_passes() == 0,
              "An empty sorter must define an empty set of passes");
        empty.generate_MIP();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
