#include <iostream>
#include <limits>
#include <string>
#include <vector>
#include <fstream>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <locale>
#include <map>

#include "utils.h"
#include "scheduler.h"
#include "algorithms.h"

static void clearInput() {
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

static int askInt(const std::string& prompt, int defVal) {
    std::cout << prompt << " [" << defVal << "]: ";
    int x;
    if (!(std::cin >> x)) { clearInput(); return defVal; }
    return x;
}

static std::string askStr(const std::string& prompt, const std::string& defVal) {
    std::cout << prompt << " [" << defVal << "]: ";
    std::string s;
    if (!(std::cin >> s)) { clearInput(); return defVal; }
    return s;
}

static bool askYesNo(const std::string& prompt, bool defNo = true) {
    std::cout << prompt << (defNo ? " [y/N]: " : " [Y/n]: ");
    char c='n';
    if (!(std::cin >> c)) { clearInput(); return !defNo; }
    if (defNo) return (c=='y' || c=='Y');
    return !(c=='n' || c=='N');
}

static DistributionType askDist() {
    std::cout << "Distribution (1=Uniform, 2=Bimodal) [1]: ";
    int d = 1;
    if (!(std::cin >> d)) { clearInput(); d = 1; }
    return (d == 2) ? DistributionType::Bimodal : DistributionType::Uniform;
}

static std::string csvEscape(const std::string& s, char sep) {
    bool needQuotes = s.find(sep) != std::string::npos ||
                      s.find('"')   != std::string::npos ||
                      s.find('\n')  != std::string::npos ||
                      s.find('\r')  != std::string::npos;
    if (!needQuotes) return s;
    std::string out; out.reserve(s.size() + 2);
    out.push_back('"');
    for (char c : s) {
        if (c == '"') out.push_back('"');
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

static void appendCsvRow(const std::string& csvPath,
                         const std::string& instanceId,
                         const std::string& algo,
                         int n, int threads,
                         long long timeMs,
                         long long sumC)
{
    namespace fs = std::filesystem;
    const char SEP = ';';
    static std::map<std::string, long long> baselineTimes; // zapamiętuje czas dla threads=1

    // Automatyczne obliczanie speedup i efficiency
    double speedup = 1.0, efficiency = 1.0;
    if (threads == 1) {
        baselineTimes[algo] = timeMs;
    } else if (baselineTimes.contains(algo) && baselineTimes[algo] > 0) {
        speedup = (double)baselineTimes[algo] / std::max(1.0, (double)timeMs);
        efficiency = speedup / threads;
    }

    fs::path p(csvPath);
    try {
        if (!p.parent_path().empty())
            fs::create_directories(p.parent_path());
    } catch (const std::exception& e) {
        std::cerr << "Error: cannot create directory for CSV: " << e.what() << "\n";
        return;
    }

    bool newFile = !fs::exists(p) || fs::file_size(p) == 0;
    std::ofstream out(csvPath, std::ios::app | std::ios::binary);
    if (!out) {
        std::cerr << "Error: cannot write to CSV " << csvPath << "\n";
        return;
    }

    // Zapis w UTF-8 z nagłówkiem
    out.imbue(std::locale::classic());
    if (newFile) {
        const unsigned char bom[3] = {0xEF, 0xBB, 0xBF};
        out.write(reinterpret_cast<const char*>(bom), 3);
        out << "run_at" << SEP << "instance" << SEP << "algo" << SEP
            << "n" << SEP << "threads" << SEP
            << "time_ms" << SEP << "sumC" << SEP
            << "speedup" << SEP << "efficiency" << "\n";
    }

    // Timestamp
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm lt{};
#if defined(_WIN32)
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    std::ostringstream ts;
    ts << std::put_time(&lt, "%Y-%m-%d %H:%M:%S");

    // Formatowanie liczb
    std::ostringstream sp, ef;
    sp << std::fixed << std::setprecision(3) << speedup;
    ef << std::fixed << std::setprecision(3) << efficiency;

    out << ts.str() << SEP
        << instanceId << SEP
        << algo << SEP
        << n << SEP
        << threads << SEP
        << timeMs << SEP
        << sumC << SEP
        << sp.str() << SEP
        << ef.str() << "\n";

    std::cout << "Appended to " << csvPath
              << "  (speedup=" << sp.str()
              << ", efficiency=" << ef.str() << ")\n";
}


static void printSettingsHelp() {
    std::cout << "\n-- Settings help --\n";
    std::cout << "threads: number of threads used in any algorithm (1/2/4/8).\n";
    std::cout << "time budget [ms]: time limit per local-search iteration (prevents infinite runs).\n";
    std::cout << "no-improve tries factor: limits local search effort, usually 1000*n.\n";
    std::cout << "seed: RNG seed; same seed -> reproducible results.\n";
    std::cout << "CSV path: output file (directories auto-created).\n";

    std::cout << "\nData generation / loading:\n";
    std::cout << " - Generate or load datasets from text files.\n";
    std::cout << " - If file missing, program can generate it automatically.\n";

    std::cout << "\nFile format:\n";
    std::cout << "   Line 1: n\n   Line 2: p1 p2 ... pn\n";

    std::cout << "\nAll relative paths resolve from build dir (e.g. cmake-build-debug/)\n";
}

int main() {
    std::vector<Task> tasks;
    std::string currentInstance = "NA";
    bool running = true;

    while (running) {
        std::cout << "\n==============================\n";
        std::cout << " SINGLE MACHINE SCHEDULER\n";
        std::cout << "==============================\n";
        std::cout << "1) Generate input file\n";
        std::cout << "2) Load tasks from file (auto-create if missing)\n";
        std::cout << "3) Run SPT\n";
        std::cout << "4) Run Cheapest Insertion\n";
        std::cout << "5) Run Local Search 2-swap\n";
        std::cout << "6) Benchmark all (SPT, CI, LS)\n";
        std::cout << "7) Help (settings)\n";
        std::cout << "0) Exit\n";
        std::cout << "Choose option: ";

        int choice;
        if (!(std::cin >> choice)) { clearInput(); continue; }
        if (choice == 0) { std::cout << "Bye.\n"; break; }

        switch (choice) {
            case 1: {
                std::string fname = askStr("Output filename", "data/input_200.txt");
                int n = askInt("Number of tasks", 200);
                DistributionType dist = askDist();
                generateInputFile(fname, n, dist);
                break;
            }

            case 2: {
                std::string fname = askStr("Input filename", "data/input_200.txt");
                namespace fs = std::filesystem;
                if (!fs::exists(fname)) {
                    std::cout << "File does not exist.\n";
                    if (askYesNo("Generate it now?", true)) {
                        int n = askInt("Number of tasks", 200);
                        DistributionType dist = askDist();
                        generateInputFile(fname, n, dist);
                    }
                }
                auto loaded = loadTasks(fname);
                if (!loaded.empty()) {
                    tasks = std::move(loaded);
                    currentInstance = fname;
                    std::cout << "Loaded " << tasks.size() << " tasks.\n";
                }
                break;
            }

            case 3: { // SPT
                if (tasks.empty()) { std::cout << "No tasks loaded.\n"; break; }
                int threads = askInt("Threads (1/2/4/8)", 1);
                auto t0 = std::chrono::steady_clock::now();
                auto order = sptOrder(tasks, threads);
                long long sumC = calculateTotalCompletionTime(tasks, order);
                auto t1 = std::chrono::steady_clock::now();
                long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

                std::cout << "SPT: sumC=" << sumC << " time=" << ms << " ms\n";
                if (askYesNo("Append to CSV?")) {
                    std::string csv = askStr("CSV path", "results.csv");
                    appendCsvRow(csv, currentInstance, "SPT", (int)tasks.size(), threads, ms, sumC);
                }
                break;
            }

            case 4: { // Cheapest Insertion
                if (tasks.empty()) { std::cout << "No tasks loaded.\n"; break; }
                int threads = askInt("Threads (1/2/4/8)", 1);
                auto t0 = std::chrono::steady_clock::now();
                auto order = cheapestInsertionOrder(tasks, threads);
                long long sumC = calculateTotalCompletionTime(tasks, order);
                auto t1 = std::chrono::steady_clock::now();
                long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

                std::cout << "CheapestInsertion: sumC=" << sumC << " time=" << ms << " ms\n";
                if (askYesNo("Append to CSV?")) {
                    std::string csv = askStr("CSV path", "results.csv");
                    appendCsvRow(csv, currentInstance, "CheapestInsertion", (int)tasks.size(), threads, ms, sumC);
                }
                break;
            }

            case 5: { // Local Search
                if (tasks.empty()) { std::cout << "No tasks loaded.\n"; break; }

                int threads = askInt("Threads (1/2/4/8)", 1);
                int timeBudgetMs = askInt("Time budget [ms]", 2000);
                unsigned int seed = (unsigned int)askInt("Random seed", 42);
                int noImproveFactor = askInt("No-improve tries factor (×n)", 1000);

                LsParams lp;
                lp.timeBudgetMs = timeBudgetMs;
                lp.seed = seed;
                lp.maxNoImproveTries = noImproveFactor * (int)tasks.size();

                auto t0 = std::chrono::steady_clock::now();
                auto res = localSearch2Swap(tasks, lp, threads);
                long long sumC = res.sumC;
                auto t1 = std::chrono::steady_clock::now();
                long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

                std::cout << "LocalSearch: sumC=" << sumC << " time=" << ms << " ms, threads=" << threads << "\n";
                if (askYesNo("Append to CSV?")) {
                    std::string csv = askStr("CSV path", "results.csv");
                    appendCsvRow(csv, currentInstance, "LocalSearch", (int)tasks.size(), threads, ms, sumC);
                }
                break;
            }

            case 6: { // Benchmark all
                if (tasks.empty()) { std::cout << "No tasks loaded.\n"; break; }
                std::string csv = askStr("CSV path", "results.csv");
                int threads = askInt("Threads (1/2/4/8)", 1);
                int timeBudgetMs = askInt("LS: Time budget [ms]", 2000);
                unsigned int seed = (unsigned int)askInt("Random seed", 42);
                int noImproveFactor = askInt("No-improve tries factor (×n)", 1000);

                LsParams lp;
                lp.timeBudgetMs = timeBudgetMs;
                lp.seed = seed;
                lp.maxNoImproveTries = noImproveFactor * (int)tasks.size();

                // SPT
                {
                    auto t0 = std::chrono::steady_clock::now();
                    auto ord = sptOrder(tasks, threads);
                    long long sumC = calculateTotalCompletionTime(tasks, ord);
                    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0).count();
                    std::cout << "[BENCH] SPT: sumC=" << sumC << " time=" << ms << " ms\n";
                    appendCsvRow(csv, currentInstance, "SPT", (int)tasks.size(), threads, ms, sumC);
                }
                // CI
                {
                    auto t0 = std::chrono::steady_clock::now();
                    auto ord = cheapestInsertionOrder(tasks, threads);
                    long long sumC = calculateTotalCompletionTime(tasks, ord);
                    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0).count();
                    std::cout << "[BENCH] CI: sumC=" << sumC << " time=" << ms << " ms\n";
                    appendCsvRow(csv, currentInstance, "CheapestInsertion", (int)tasks.size(), threads, ms, sumC);
                }
                // LS
                {
                    auto t0 = std::chrono::steady_clock::now();
                    auto res = localSearch2Swap(tasks, lp, threads);
                    long long sumC = res.sumC;
                    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0).count();
                    std::cout << "[BENCH] LS: sumC=" << sumC << " time=" << ms << " ms\n";
                    appendCsvRow(csv, currentInstance, "LocalSearch", (int)tasks.size(), threads, ms, sumC);
                }
                break;
            }

            case 7:
                printSettingsHelp();
                break;

            default:
                std::cout << "Invalid option.\n";
        }
    }
    return 0;
}
