#include "task.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {

std::string trim(const std::string& text) {
    const std::string blanks = " \t\r\n";
    const std::size_t begin = text.find_first_not_of(blanks);
    if (begin == std::string::npos) return {};
    const std::size_t end = text.find_last_not_of(blanks);
    return text.substr(begin, end - begin + 1);
}

} // namespace

bool TaskManager::add(const std::string& title) {
    const std::string cleaned = trim(title);
    if (cleaned.empty()) return false;
    tasks.push_back(Task{nextId++, cleaned, false});
    return true;
}

void TaskManager::list() const {
    if (tasks.empty()) {
        std::cout << "No tasks.\n";
        return;
    }
    for (const auto& task : tasks) {
        std::cout << task.id << ". [" << (task.done ? "yes" : " ") << "] " << task.title << "\n";
    }
}

bool TaskManager::markDone(int id) {
    for (auto& task : tasks) {
        if (task.id == id) {
            task.done = true;
            return true;
        }
    }
    return false;
}

bool TaskManager::remove(int id) {
    for (auto it = tasks.begin(); it != tasks.end(); ++it) {
        if (it->id == id) {
            tasks.erase(it);
            return true;
        }
    }
    return false;
}

bool TaskManager::load(const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;

    std::vector<Task> loaded;
    int maxId = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (trim(line).empty()) continue;

        std::istringstream ss(line);
        Task task;
        if (!(ss >> task.id >> task.done)) continue; // skip malformed lines
        std::getline(ss, task.title);
        if (!task.title.empty() && task.title[0] == ' ') task.title.erase(0, 1);
        task.title = trim(task.title);
        if (task.id <= 0 || task.title.empty()) continue;

        maxId = std::max(maxId, task.id);
        loaded.push_back(task);
    }
    if (in.bad()) return false;

    tasks = std::move(loaded);
    nextId = maxId + 1;
    return true;
}

bool TaskManager::save(const std::string& path) const {
    std::ofstream out(path);
    if (!out) return false;
    for (const auto& task : tasks) {
        out << task.id << " " << (task.done ? 1 : 0) << " " << task.title << "\n";
    }
    out.flush();
    return static_cast<bool>(out);
}
