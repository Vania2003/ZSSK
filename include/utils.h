#ifndef ZSSK_UTILS_H
#define ZSSK_UTILS_H

#pragma once
#include <string>

enum class DistributionType {
    Uniform,
    Bimodal
};

void generateInputFile(const std::string& filename, int n, DistributionType type);

#endif // ZSSK_UTILS_H
