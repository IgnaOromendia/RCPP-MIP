#include "../lib/io/PathOrderWriter.h"
#include "../lib/io/PathSortInstanceReader.h"
#include "../lib/model/PathSorter.h"
#include <exception>
#include <iostream>
#include <string>
#include <utility>

int main(int argc, char** argv) {
    if (argc < 4 || argc > 5) {
        std::cerr << "Uso: pathSortExec <input.dat> <curvas.dat> <solucion.dat> [orden.dat]\n";
        return 1;
    }

    try {
        const std::string output_path = argc == 5 ? argv[4] : "orden.dat";
        PathSortInstance instance = PathSortInstanceReader::read_files(argv[1], argv[2], argv[3]);
        const int deposit = instance.deposit;

        PathSorter sorter(std::move(instance));
        sorter.generate_MIP();
        const CPLEXSolveResult result = sorter.solve();
        if (!result.has_solution) {
            std::cerr << "No se encontro un orden factible.\n";
            return result.status == IloAlgorithm::Error ? 1 : 2;
        }

        PathOrderWriter::write_file(output_path, sorter.extract_order(), deposit);
        std::cout << "Orden guardado en " << output_path << '\n';
        return 0;
    } catch (const IloException& error) {
        std::cerr << "Error de CPLEX: " << error << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
    }
    return 1;
}
