#include "algorithms.h"
#include <algorithm>
#include <random>
#include <thread>
#include <mutex>
#include <atomic>
#include <iostream>
#include <chrono>
#include <execution>

// ======================================================
// Helper: compute total completion time ΣCi
// ======================================================
long long calculateTotalCompletionTime(const std::vector<Task>& tasks,
                                       const std::vector<int>& order)
{
    long long sum = 0;
    long long current = 0;
    for (int idx : order) {
        current += tasks[idx].p;
        sum += current;
    }
    return sum;
}

// ======================================================
// Algorithm 1: SPT (Shortest Processing Time first)
// ======================================================
std::vector<int> sptOrder(const std::vector<Task>& tasks, int threads)
{
    std::vector<int> order(tasks.size());
    std::iota(order.begin(), order.end(), 0);

    if (threads > 1) {
        std::sort(std::execution::par, order.begin(), order.end(),
                  [&](int a, int b){ return tasks[a].p < tasks[b].p; });
    } else {
        std::sort(order.begin(), order.end(),
                  [&](int a, int b){ return tasks[a].p < tasks[b].p; });
    }
    return order;
}

// ======================================================
// Algorithm 2: Cheapest Insertion (parallel-aware)
// ======================================================
std::vector<int> cheapestInsertionOrder(const std::vector<Task>& tasks, int threads)
{
    int n = (int)tasks.size();
    if (n == 0) return {};

    std::vector<int> order;
    order.reserve(n);

    // Start with first two shortest tasks
    std::vector<int> indices(n);
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](int a, int b){ return tasks[a].p < tasks[b].p; });

    order.push_back(indices[0]);
    if (n > 1) order.push_back(indices[1]);

    for (int i = 2; i < n; ++i) {
        int t = indices[i];
        long long bestIncrease = LLONG_MAX;
        int bestPos = 0;

        // Parallel section
        if (threads > 1 && order.size() > 100) {
            std::mutex mtx;
            std::vector<std::thread> pool;
            int chunk = std::max(1, (int)order.size() / threads);

            for (int th = 0; th < threads; ++th) {
                int start = th * chunk;
                int end = std::min((int)order.size(), start + chunk);
                pool.emplace_back([&, start, end] {
                    long long localBest = LLONG_MAX;
                    int localPos = start;
                    for (int pos = start; pos <= end; ++pos) {
                        std::vector<int> tmp = order;
                        tmp.insert(tmp.begin() + pos, t);
                        long long sum = calculateTotalCompletionTime(tasks, tmp);
                        if (sum < localBest) {
                            localBest = sum;
                            localPos = pos;
                        }
                    }
                    std::scoped_lock lock(mtx);
                    if (localBest < bestIncrease) {
                        bestIncrease = localBest;
                        bestPos = localPos;
                    }
                });
            }
            for (auto& th : pool) th.join();
        } else {
            // Sequential fallback
            for (int pos = 0; pos <= (int)order.size(); ++pos) {
                std::vector<int> tmp = order;
                tmp.insert(tmp.begin() + pos, t);
                long long sum = calculateTotalCompletionTime(tasks, tmp);
                if (sum < bestIncrease) {
                    bestIncrease = sum;
                    bestPos = pos;
                }
            }
        }
        order.insert(order.begin() + bestPos, t);
    }

    return order;
}

// ======================================================
// Algorithm 3: Local Search 2-swap (hybrid sequential/parallel)
// ======================================================
LsResult localSearch2Swap(const std::vector<Task>& tasks,
                          const LsParams& params, int threads)
{
    int n = (int)tasks.size();
    LsResult res;
    if (n == 0) return res;

    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);

    std::mt19937 gen(params.seed);
    std::shuffle(order.begin(), order.end(), gen);

    long long bestSum = calculateTotalCompletionTime(tasks, order);
    bool improved = true;
    int noImprove = 0;

    auto start = std::chrono::steady_clock::now();

    while (improved) {
        improved = false;

        long long globalBestDelta = 0;
        int globalBestI = -1, globalBestJ = -1;

        const std::vector<int> currentOrder = order;

        if (threads > 1 && n > 200) {
            std::mutex mtx;
            std::atomic<int> iIndex = 0;

            auto worker = [&]() {
                long long localBestDelta = 0;
                int localBestI = -1, localBestJ = -1;

                while (true) {
                    int i = iIndex++;
                    if (i >= n - 1) break;

                    for (int j = i + 1; j < n; ++j) {
                        std::vector<int> tmp = currentOrder;
                        std::swap(tmp[i], tmp[j]);
                        long long newSum = calculateTotalCompletionTime(tasks, tmp);
                        long long delta = bestSum - newSum;
                        if (delta > localBestDelta) {
                            localBestDelta = delta;
                            localBestI = i;
                            localBestJ = j;
                        }
                    }
                }

                if (localBestDelta > 0) {
                    std::scoped_lock lock(mtx);
                    if (localBestDelta > globalBestDelta) {
                        globalBestDelta = localBestDelta;
                        globalBestI = localBestI;
                        globalBestJ = localBestJ;
                    }
                }
            };

            std::vector<std::thread> pool;
            for (int t = 0; t < threads; ++t) pool.emplace_back(worker);
            for (auto& th : pool) th.join();
        } else {
            for (int i = 0; i < n - 1; ++i) {
                for (int j = i + 1; j < n; ++j) {
                    std::vector<int> tmp = order;
                    std::swap(tmp[i], tmp[j]);
                    long long newSum = calculateTotalCompletionTime(tasks, tmp);
                    long long delta = bestSum - newSum;
                    if (delta > globalBestDelta) {
                        globalBestDelta = delta;
                        globalBestI = i;
                        globalBestJ = j;
                    }
                }
            }
        }

        if (globalBestDelta > 0 && globalBestI >= 0) {
            std::swap(order[globalBestI], order[globalBestJ]);
            bestSum -= globalBestDelta;
            improved = true;
            noImprove = 0;
        } else {
            noImprove++;
        }

        auto now = std::chrono::steady_clock::now();
        long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        if (elapsed > params.timeBudgetMs) break;
        if (noImprove > params.maxNoImproveTries) break;
    }

    res.order = order;
    res.sumC = bestSum;
    return res;
}
