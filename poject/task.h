#pragma once
#include <cstddef>
#include <string>
#include <vector>

struct Task {
    int id = 0;
    std::string title;
    bool done = false;
};

class TaskManager {
public:
    // Adds a task; returns false when the title is empty.
    bool add(const std::string& title);
    void list() const;
    bool markDone(int id);
    bool remove(int id);
    // Returns false when the file cannot be opened or read.
    bool load(const std::string& path);
    // Returns false when the file cannot be written.
    bool save(const std::string& path) const;

    std::size_t size() const { return tasks.size(); }
    bool empty() const { return tasks.empty(); }

private:
    std::vector<Task> tasks;
    int nextId = 1;
};
