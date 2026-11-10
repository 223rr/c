#include "task.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {

bool isSpace(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

std::string trim(const std::string& text) {
    std::size_t begin = 0;
    while (begin < text.size() && isSpace(text[begin])) ++begin;
    std::size_t end = text.size();
    while (end > begin && isSpace(text[end - 1])) --end;
    return text.substr(begin, end - begin);
}

std::string toLower(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

// 从 pos 处取出一个以空白分隔的词，并把 pos 移到词后。
std::string takeWord(const std::string& text, std::size_t& pos) {
    while (pos < text.size() && isSpace(text[pos])) ++pos;
    const std::size_t begin = pos;
    while (pos < text.size() && !isSpace(text[pos])) ++pos;
    return text.substr(begin, pos - begin);
}

int priorityRank(Priority priority) { return static_cast<int>(priority); }

// 没有截止日期的任务排在最后。
bool dueBefore(const std::string& a, const std::string& b) {
    if (a.empty() != b.empty()) return !a.empty();
    return a < b;
}

bool compareByPriority(const Task& a, const Task& b) {
    if (priorityRank(a.priority) != priorityRank(b.priority)) {
        return priorityRank(a.priority) > priorityRank(b.priority); // 高 → 低
    }
    if (a.due != b.due) return dueBefore(a.due, b.due);
    return a.id < b.id;
}

bool compareByDue(const Task& a, const Task& b) {
    if (a.due != b.due) return dueBefore(a.due, b.due); // 早 → 晚
    if (priorityRank(a.priority) != priorityRank(b.priority)) {
        return priorityRank(a.priority) > priorityRank(b.priority);
    }
    return a.id < b.id;
}

std::string formatTask(const Task& task) {
    std::ostringstream out;
    out << task.id << ". [" << (task.done ? "x" : " ") << "] "
        << priorityText(task.priority) << " "
        << (task.due.empty() ? std::string("-") : task.due) << " "
        << task.title;
    return out.str();
}

} // namespace

bool parsePriority(const std::string& text, Priority& out) {
    const std::string key = toLower(trim(text));
    if (key == "1" || key == "h" || key == "high" || key == "高") { out = Priority::High; return true; }
    if (key == "2" || key == "m" || key == "medium" || key == "中") { out = Priority::Medium; return true; }
    if (key == "3" || key == "l" || key == "low" || key == "低") { out = Priority::Low; return true; }
    return false;
}

std::string priorityText(Priority priority) {
    switch (priority) {
    case Priority::High: return "高";
    case Priority::Low: return "低";
    case Priority::Medium: default: return "中";
    }
}

bool parseSortMode(const std::string& text, SortMode& out) {
    const std::string key = toLower(trim(text));
    if (key == "id" || key == "编号" || key == "默认") { out = SortMode::Id; return true; }
    if (key == "priority" || key == "p" || key == "优先级") { out = SortMode::Priority; return true; }
    if (key == "date" || key == "due" || key == "d" || key == "日期") { out = SortMode::DueDate; return true; }
    return false;
}

std::string sortModeText(SortMode mode) {
    switch (mode) {
    case SortMode::Priority: return "priority";
    case SortMode::DueDate: return "date";
    case SortMode::Id: default: return "id";
    }
}

bool isValidDate(const std::string& text) {
    if (text.size() != 10) return false;
    if (text[4] != '-' || text[7] != '-') return false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (i == 4 || i == 7) continue;
        if (!std::isdigit(static_cast<unsigned char>(text[i]))) return false;
    }
    const int year = std::stoi(text.substr(0, 4));
    const int month = std::stoi(text.substr(5, 2));
    const int day = std::stoi(text.substr(8, 2));
    if (year < 1970 || year > 2999) return false;
    if (month < 1 || month > 12) return false;
    if (day < 1) return false;

    static const int monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    const int limit = (month == 2 && leap) ? 29 : monthDays[month - 1];
    return day <= limit;
}

bool TaskManager::add(const std::string& title, Priority priority, const std::string& due) {
    const std::string cleaned = trim(title);
    if (cleaned.empty()) return false;
    if (!due.empty() && !isValidDate(due)) return false;

    Task task;
    task.id = nextId++;
    task.title = cleaned;
    task.priority = priority;
    task.due = due;
    tasks.push_back(task);
    return true;
}

std::vector<Task> TaskManager::sorted(SortMode mode) const {
    std::vector<Task> copy = tasks;
    switch (mode) {
    case SortMode::Priority:
        std::stable_sort(copy.begin(), copy.end(), compareByPriority);
        break;
    case SortMode::DueDate:
        std::stable_sort(copy.begin(), copy.end(), compareByDue);
        break;
    case SortMode::Id:
    default:
        std::stable_sort(copy.begin(), copy.end(), [](const Task& a, const Task& b) { return a.id < b.id; });
        break;
    }
    return copy;
}

void TaskManager::list(SortMode mode) const {
    if (tasks.empty()) {
        std::cout << "No tasks.\n";
        return;
    }
    for (const Task& task : sorted(mode)) std::cout << formatTask(task) << "\n";
}

std::size_t TaskManager::search(const std::string& keyword) const {
    const std::string needle = toLower(trim(keyword));
    if (needle.empty()) return 0;

    std::size_t found = 0;
    for (const Task& task : sorted(SortMode::Id)) {
        if (toLower(task.title).find(needle) != std::string::npos) {
            std::cout << formatTask(task) << "\n";
            ++found;
        }
    }
    return found;
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
        int done = 0;
        if (!(ss >> task.id >> done)) continue; // 跳过畸形行
        task.done = done != 0;

        std::string remainder;
        std::getline(ss, remainder);
        remainder = trim(remainder);

        std::size_t pos = 0;
        const std::string first = takeWord(remainder, pos);
        if (first.size() == 1 && first[0] >= '1' && first[0] <= '3') {
            task.priority = static_cast<Priority>(first[0] - '0');
            const std::size_t afterPriority = pos;
            const std::string second = takeWord(remainder, pos);
            if (second == "-") {
                // 没有截止日期
            } else if (isValidDate(second)) {
                task.due = second;
            } else {
                pos = afterPriority; // 这一段其实是标题开头
            }
            task.title = trim(remainder.substr(pos));
        } else {
            task.title = remainder; // 旧格式：<id> <done> <标题>
        }

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
    for (const Task& task : tasks) {
        out << task.id << " " << (task.done ? 1 : 0) << " "
            << static_cast<int>(task.priority) << " "
            << (task.due.empty() ? std::string("-") : task.due) << " "
            << task.title << "\n";
    }
    out.flush();
    return static_cast<bool>(out);
}
