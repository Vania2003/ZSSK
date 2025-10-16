#ifndef ZSSK_SCHEDULER_H
#define ZSSK_SCHEDULER_H

#pragma once
#include <vector>
#include <string>

struct Task {
    int id;
    int p;
};

std::vector<Task> loadTasks(const std::string& filename);

#endif // ZSSK_SCHEDULER_H
