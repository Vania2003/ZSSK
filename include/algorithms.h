#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include "scheduler.h"

long long calculateTotalCompletionTime(const std::vector<Task>& tasks,
                                       const std::vector<int>& order);

std::vector<int> sptOrder(const std::vector<Task>& tasks, int threads);

std::vector<int> cheapestInsertionOrder(const std::vector<Task>& tasks, int threads);

struct LsParams {
    int maxNoImproveTries = 1000;
    int timeBudgetMs = 2000;
    unsigned int seed = 42;
};

struct LsResult {
    std::vector<int> order;
    long long sumC = 0;
};

LsResult localSearch2Swap(const std::vector<Task>& tasks,
                          const LsParams& params, int threads);
