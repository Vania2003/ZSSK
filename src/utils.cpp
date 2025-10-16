#include "utils.h"
#include <fstream>
#include <random>
#include <iostream>
#include <filesystem>

void generateInputFile(const std::string& filename, int n, DistributionType type) {
    namespace fs = std::filesystem;
    fs::path filePath(filename);

    try {
        if (!filePath.parent_path().empty()) {
            fs::create_directories(filePath.parent_path());
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: could not create folder for file "
                  << filename << " (" << e.what() << ")\n";
        return;
    }

    std::ofstream out(filename);
    if (!out.is_open()) {
        std::cerr << "Error: cannot create file " << filename << "\n";
        return;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distShort(1, 100);
    std::uniform_int_distribution<> distLong(300, 800);
    std::uniform_real_distribution<> probDist(0.0, 1.0);

    out << n << "\n";
    for (int i = 0; i < n; ++i) {
        int value;
        if (type == DistributionType::Uniform) {
            value = distShort(gen);
        } else {
            double prob = probDist(gen); // Bimodal: 80% short, 20% long
            value = (prob < 0.8) ? distShort(gen) : distLong(gen);
        }
        out << value;
        if (i < n - 1) out << " ";
    }
    out << "\n";

    std::cout << "File generated: " << filename
              << " (" << n << " tasks)\n"
              << "[Note] Files are saved relative to the build directory (e.g. cmake-build-debug/data/)\n";
}
