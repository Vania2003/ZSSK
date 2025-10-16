#include "scheduler.h"
#include <fstream>
#include <iostream>

std::vector<Task> loadTasks(const std::string& filename) {
    std::ifstream in(filename);
    if (!in.is_open()) {
        std::cerr << "Error: cannot open file " << filename << "\n";
        return {};
    }

    int n;
    in >> n;
    if (!in || n <= 0) {
        std::cerr << "Error: invalid number of tasks in file " << filename << "\n";
        return {};
    }

    std::vector<Task> tasks;
    tasks.reserve(n);

    for (int i = 0; i < n; ++i) {
        int p;
        in >> p;
        if (!in) {
            std::cerr << "Error: invalid data format in file " << filename << "\n";
            tasks.clear();
            return tasks;
        }
        tasks.push_back({i + 1, p});
    }

    return tasks;
}
