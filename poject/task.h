#pragma once
#include <cstddef>
#include <string>
#include <vector>

enum class Priority { Low = 1, Medium = 2, High = 3 };

enum class SortMode { Id, Priority, DueDate };

struct Task {
    int id = 0;
    std::string title;
    bool done = false;
    Priority priority = Priority::Medium;
    std::string due; // 空字符串表示没有截止日期，否则为 YYYY-MM-DD
};

class TaskManager {
public:
    // 添加任务；标题为空或日期非法时返回 false。
    bool add(const std::string& title,
             Priority priority = Priority::Medium,
             const std::string& due = std::string());
    void list(SortMode mode = SortMode::Id) const;
    std::vector<Task> sorted(SortMode mode) const;
    // 按标题子串（不区分大小写）搜索并打印结果，返回命中数量。
    std::size_t search(const std::string& keyword) const;
    bool markDone(int id);
    bool remove(int id);
    bool load(const std::string& path);
    bool save(const std::string& path) const;

    std::size_t size() const { return tasks.size(); }
    bool empty() const { return tasks.empty(); }

private:
    std::vector<Task> tasks;
    int nextId = 1;
};

// 解析 "高/中/低"、"high/medium/low"、"h/m/l"、"1/2/3"。
bool parsePriority(const std::string& text, Priority& out);
std::string priorityText(Priority priority);
bool parseSortMode(const std::string& text, SortMode& out);
std::string sortModeText(SortMode mode);
// 校验 YYYY-MM-DD（只看格式与基本范围）。
bool isValidDate(const std::string& text);
