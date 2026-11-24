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

// 备注要压成一行存盘：反斜杠与换行做转义。
std::string escapeNotes(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 8);
    for (char c : text) {
        if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c != '\r') out.push_back(c);
    }
    return out;
}

std::string unescapeNotes(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            const char next = text[i + 1];
            if (next == 'n') { out.push_back('\n'); ++i; continue; }
            if (next == '\\') { out.push_back('\\'); ++i; continue; }
        }
        out.push_back(text[i]);
    }
    return out;
}

std::string formatTask(const Task& task) {
    std::ostringstream out;
    out << task.id << ". [" << (task.done ? "x" : " ") << "] "
        << priorityText(task.priority) << " "
        << "目标 " << (task.target.empty() ? std::string("-") : task.target) << " · "
        << "截止 " << (task.due.empty() ? std::string("-") : task.due) << " "
        << task.title;
    if (!task.subs.empty()) {
        std::size_t doneCount = 0;
        for (const SubTask& sub : task.subs) {
            if (sub.done) ++doneCount;
        }
        out << " (" << doneCount << "/" << task.subs.size() << ")";
    }
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

bool datesConsistent(const std::string& due, const std::string& target) {
    if (due.empty() || target.empty()) return true;
    return target <= due; // ISO 日期按字典序比较即按时间比较
}

bool isRepeatValid(const std::string& repeat) {
    return repeat.empty() || repeat == "daily" || repeat == "weekly" || repeat == "monthly";
}

bool TaskManager::add(const std::string& title, Priority priority,
                      const std::string& due, const std::string& target,
                      const std::string& repeat) {
    const std::string cleaned = trim(title);
    if (cleaned.empty()) return false;
    if (!due.empty() && !isValidDate(due)) return false;
    if (!target.empty() && !isValidDate(target)) return false;
    if (!datesConsistent(due, target)) return false;
    if (!isRepeatValid(repeat)) return false;

    Task task;
    task.id = nextId++;
    task.title = cleaned;
    task.priority = priority;
    task.due = due;
    task.target = target;
    task.repeat = repeat;
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

bool TaskManager::markUndone(int id) {
    for (auto& task : tasks) {
        if (task.id == id) {
            task.done = false;
            return true;
        }
    }
    return false;
}

bool TaskManager::remove(int id) {
    for (auto it = tasks.begin(); it != tasks.end(); ++it) {
        if (it->id == id) {
            tasks.erase(it);
            renumber(); // 删掉后编号重新排成 1..N
            return true;
        }
    }
    return false;
}

void TaskManager::renumber() {
    std::stable_sort(tasks.begin(), tasks.end(),
                     [](const Task& a, const Task& b) { return a.id < b.id; });
    int next = 1;
    for (Task& task : tasks) task.id = next++;
    nextId = next;
}

bool TaskManager::restore(const Task& task) {
    Task copy = task;
    if (find(copy.id) != nullptr) copy.id = nextId; // 编号被占用了就排到末尾
    tasks.push_back(copy);
    renumber();
    return true;
}

std::size_t TaskManager::removeDone() {
    const std::size_t before = tasks.size();
    tasks.erase(std::remove_if(tasks.begin(), tasks.end(),
                               [](const Task& task) { return task.done; }),
                tasks.end());
    const std::size_t removed = before - tasks.size();
    if (removed) renumber();
    return removed;
}

const Task* TaskManager::find(int id) const {
    for (const Task& task : tasks) {
        if (task.id == id) return &task;
    }
    return nullptr;
}

bool TaskManager::addSub(int id, const std::string& title) {
    const std::string cleaned = trim(title);
    if (cleaned.empty()) return false;
    for (Task& task : tasks) {
        if (task.id == id) {
            task.subs.push_back(SubTask{cleaned, false});
            return true;
        }
    }
    return false;
}

bool TaskManager::toggleSub(int id, std::size_t index) {
    for (Task& task : tasks) {
        if (task.id == id) {
            if (index >= task.subs.size()) return false;
            task.subs[index].done = !task.subs[index].done;
            return true;
        }
    }
    return false;
}

bool TaskManager::removeSub(int id, std::size_t index) {
    for (Task& task : tasks) {
        if (task.id == id) {
            if (index >= task.subs.size()) return false;
            task.subs.erase(task.subs.begin() + static_cast<std::ptrdiff_t>(index));
            return true;
        }
    }
    return false;
}

bool TaskManager::setNotes(int id, const std::string& notes) {
    for (Task& task : tasks) {
        if (task.id == id) {
            task.notes = notes;
            return true;
        }
    }
    return false;
}

bool TaskManager::addFile(int id, const std::string& name) {
    if (name.empty()) return false;
    for (Task& task : tasks) {
        if (task.id == id) {
            for (const std::string& existing : task.files) {
                if (existing == name) return false; // 重名就不重复挂
            }
            task.files.push_back(name);
            return true;
        }
    }
    return false;
}

bool TaskManager::removeFile(int id, const std::string& name) {
    for (Task& task : tasks) {
        if (task.id == id) {
            for (auto it = task.files.begin(); it != task.files.end(); ++it) {
                if (*it == name) {
                    task.files.erase(it);
                    return true;
                }
            }
            return false;
        }
    }
    return false;
}

bool TaskManager::setRepeat(int id, const std::string& repeat) {
    if (!isRepeatValid(repeat)) return false;
    for (Task& task : tasks) {
        if (task.id == id) {
            task.repeat = repeat;
            return true;
        }
    }
    return false;
}

bool TaskManager::setDoneAt(int id, const std::string& date) {
    if (!date.empty() && !isValidDate(date)) return false;
    for (Task& task : tasks) {
        if (task.id == id) {
            task.doneAt = date;
            return true;
        }
    }
    return false;
}

bool TaskManager::addSpent(int id, int seconds) {
    if (seconds <= 0) return false;
    for (Task& task : tasks) {
        if (task.id == id) {
            task.spent += seconds;
            return true;
        }
    }
    return false;
}

bool TaskManager::setSticker(int id, const std::string& sticker) {
    for (Task& task : tasks) {
        if (task.id == id) {
            task.sticker = sticker;
            return true;
        }
    }
    return false;
}

bool TaskManager::addTag(int id, const std::string& tag) {
    const std::string cleaned = trim(tag);
    if (cleaned.empty()) return false;
    for (Task& task : tasks) {
        if (task.id == id) {
            for (const std::string& existing : task.tags) {
                if (existing == cleaned) return false; // 已经有这个标签
            }
            task.tags.push_back(cleaned);
            return true;
        }
    }
    return false;
}

bool TaskManager::removeTag(int id, const std::string& tag) {
    const std::string cleaned = trim(tag);
    for (Task& task : tasks) {
        if (task.id == id) {
            for (auto it = task.tags.begin(); it != task.tags.end(); ++it) {
                if (*it == cleaned) {
                    task.tags.erase(it);
                    return true;
                }
            }
            return false;
        }
    }
    return false;
}

bool TaskManager::setPriority(int id, Priority priority) {
    for (auto& task : tasks) {
        if (task.id == id) {
            task.priority = priority;
            return true;
        }
    }
    return false;
}

bool TaskManager::setDue(int id, const std::string& due) {
    if (!due.empty() && !isValidDate(due)) return false;
    for (auto& task : tasks) {
        if (task.id == id) {
            if (!datesConsistent(due, task.target)) return false;
            task.due = due;
            return true;
        }
    }
    return false;
}

bool TaskManager::setTarget(int id, const std::string& target) {
    if (!target.empty() && !isValidDate(target)) return false;
    for (auto& task : tasks) {
        if (task.id == id) {
            if (!datesConsistent(task.due, target)) return false;
            task.target = target;
            return true;
        }
    }
    return false;
}

bool TaskManager::contains(int id) const {
    for (const Task& task : tasks) {
        if (task.id == id) return true;
    }
    return false;
}

bool TaskManager::show(int id) const {
    for (const Task& task : tasks) {
        if (task.id == id) {
            std::cout << formatTask(task) << "\n";
            return true;
        }
    }
    return false;
}

bool TaskManager::load(const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;

    std::vector<Task> loaded;
    std::string line;
    while (std::getline(in, line)) {
        // 兼容记事本另存为带 BOM 的 UTF-8 文件
        if (line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF) {
            line.erase(0, 3);
        }
        if (trim(line).empty()) continue;

        // 子任务行： "+ <0|1> <标题>"，挂在上一行任务下
        if (line.size() > 2 && line[0] == '+' && line[1] == ' ') {
            if (loaded.empty()) continue;
            std::string rest = trim(line.substr(2));
            if (rest.empty()) continue;
            int subDone = 0;
            std::istringstream subStream(rest);
            SubTask sub;
            if (subStream >> subDone) {
                std::string subTitle;
                std::getline(subStream, subTitle);
                sub.title = trim(subTitle);
            } else {
                sub.title = rest; // 没有标记位的老格式
            }
            if (sub.title.empty()) continue;
            sub.done = subDone != 0;
            loaded.back().subs.push_back(sub);
            continue;
        }

        // 备注行： "~ <转义后的多行文本>"
        if (line.size() > 2 && line[0] == '~' && line[1] == ' ') {
            if (loaded.empty()) continue;
            loaded.back().notes = unescapeNotes(line.substr(2));
            continue;
        }

        // 附件行： "@ <文件名>"
        if (line.size() > 2 && line[0] == '@' && line[1] == ' ') {
            if (loaded.empty()) continue;
            const std::string name = trim(line.substr(2));
            if (!name.empty()) loaded.back().files.push_back(name);
            continue;
        }

        // 重复规则行： "& daily"
        if (line.size() > 2 && line[0] == '&' && line[1] == ' ') {
            if (loaded.empty()) continue;
            const std::string repeat = trim(line.substr(2));
            if (isRepeatValid(repeat)) loaded.back().repeat = repeat;
            continue;
        }

        // 完成日期行： "^ 2026-11-24"
        if (line.size() > 2 && line[0] == '^' && line[1] == ' ') {
            if (loaded.empty()) continue;
            const std::string date = trim(line.substr(2));
            if (isValidDate(date)) loaded.back().doneAt = date;
            continue;
        }

        // 专注时长行： "$ 1500"
        if (line.size() > 2 && line[0] == '$' && line[1] == ' ') {
            if (loaded.empty()) continue;
            const std::string seconds = trim(line.substr(2));
            if (!seconds.empty() && seconds.find_first_not_of("0123456789") == std::string::npos) {
                loaded.back().spent = std::stoi(seconds);
            }
            continue;
        }

        // 贴纸行： "% 🧸"
        if (line.size() > 2 && line[0] == '%' && line[1] == ' ') {
            if (loaded.empty()) continue;
            loaded.back().sticker = trim(line.substr(2));
            continue;
        }

        // 标签行： "# 工作"
        if (line.size() > 2 && line[0] == '#' && line[1] == ' ') {
            if (loaded.empty()) continue;
            const std::string tag = trim(line.substr(2));
            if (!tag.empty()) loaded.back().tags.push_back(tag);
            continue;
        }

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

            bool hasDue = false;
            if (second == "-") {
                hasDue = true; // 没有截止日期
            } else if (isValidDate(second)) {
                hasDue = true;
                task.due = second;
            } else {
                pos = afterPriority; // 这一段其实是标题开头
            }

            if (hasDue) {
                const std::size_t afterDue = pos;
                const std::string third = takeWord(remainder, pos);
                if (third == "-") {
                    // 没有目标完成日
                } else if (isValidDate(third)) {
                    task.target = third;
                } else {
                    pos = afterDue; // 旧格式没有目标日，这一段是标题
                }
            }
            task.title = trim(remainder.substr(pos));
        } else {
            task.title = remainder; // 旧格式：<id> <done> <标题>
        }

        if (task.id <= 0 || task.title.empty()) continue;
        loaded.push_back(task);
    }
    if (in.bad()) return false;

    tasks = std::move(loaded);
    renumber(); // 旧文件里编号可能有空洞，读入后重新排成 1..N
    return true;
}

bool TaskManager::save(const std::string& path) const {
    std::ofstream out(path);
    if (!out) return false;
    for (const Task& task : tasks) {
        out << task.id << " " << (task.done ? 1 : 0) << " "
            << static_cast<int>(task.priority) << " "
            << (task.due.empty() ? std::string("-") : task.due) << " "
            << (task.target.empty() ? std::string("-") : task.target) << " "
            << task.title << "\n";
        for (const SubTask& sub : task.subs) {
            out << "+ " << (sub.done ? 1 : 0) << " " << sub.title << "\n";
        }
        if (!task.notes.empty()) out << "~ " << escapeNotes(task.notes) << "\n";
        for (const std::string& file : task.files) out << "@ " << file << "\n";
        if (!task.repeat.empty()) out << "& " << task.repeat << "\n";
        if (!task.doneAt.empty()) out << "^ " << task.doneAt << "\n";
        if (task.spent > 0) out << "$ " << task.spent << "\n";
        if (!task.sticker.empty()) out << "% " << task.sticker << "\n";
        for (const std::string& tag : task.tags) out << "# " << tag << "\n";
    }
    out.flush();
    return static_cast<bool>(out);
}
