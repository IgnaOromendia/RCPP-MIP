#include "../lib/io/PathOrderWriter.h"
#include "../lib/io/PathSortInstanceReader.h"
#include "../lib/model/PathSorter.h"
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>

int main(int argc, char** argv) {
    if (argc < 4 || argc > 6) {
        std::cerr << "Uso: pathSortExec <input.dat> <curvas.dat> <solucion.dat> "
                     "[orden.dat] [route_segments.csv]\n";
        return 1;
    }

    try {
        PathSortInstance instance = PathSortInstanceReader::read_files(argv[1], argv[2], argv[3]);
        const std::filesystem::path default_output_directory =
            std::filesystem::path("output") / "order";
        const std::string output_path = argc >= 5
            ? argv[4]
            : (default_output_directory /
               ("out_" + std::to_string(instance.original_nodes) + ".dat")).string();
        const std::string segments_path = argc == 6 ? argv[5] : "route_segments.csv";
        const int deposit = instance.deposit;

        PathSorter sorter(std::move(instance));
        sorter.generate_MIP();
        const CPLEXSolveResult result = sorter.solve();
        if (!result.has_solution) {
            std::cerr << "No se encontro un orden factible.\n";
            return result.status == IloAlgorithm::Error ? 1 : 2;
        }

        const std::vector<OrderedPass> order = sorter.extract_order();
        if (argc < 5) std::filesystem::create_directories(default_output_directory);
        PathOrderWriter::write_file(output_path, order, deposit);
        PathOrderWriter::write_segments_file(segments_path, order);
        std::cout << "Distancia minima: " << sorter.minimum_distance() << '\n';
        std::cout << "Orden guardado en " << output_path << '\n';
        std::cout << "Segmentos guardados en " << segments_path << '\n';
        return 0;
    } catch (const IloException& error) {
        std::cerr << "Error de CPLEX: " << error << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
    }
    return 1;
}
