#include "task.h"

#include <cctype>
#include <iostream>
#include <sstream>
#include <string>

namespace {

const char* kDataFileName = "tasks.txt";

bool isSpace(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

void skipSpaces(const std::string& text, std::size_t& pos) {
    while (pos < text.size() && isSpace(text[pos])) ++pos;
}

std::string takeWord(const std::string& text, std::size_t& pos) {
    skipSpaces(text, pos);
    const std::size_t begin = pos;
    while (pos < text.size() && !isSpace(text[pos])) ++pos;
    return text.substr(begin, pos - begin);
}

bool parseId(const std::string& text, int& id) {
    std::istringstream ss(text);
    if (!(ss >> id)) return false;
    std::string trailing;
    return !(ss >> trailing); // 拒绝 "12abc" 和 "1 2"
}

// 数据文件默认放在可执行文件旁边，这样从任何目录启动都用同一份任务。
std::string defaultDataPath(const char* argv0) {
    const std::string exe = argv0 ? argv0 : "";
    const std::size_t slash = exe.find_last_of("\\/");
    if (slash == std::string::npos) return kDataFileName;
    return exe.substr(0, slash + 1) + kDataFileName;
}

// 解析 "add -p 高 -d 2026-05-01 写报告" 这类参数，标题保留原始空格。
bool parseAddArgs(const std::string& text, Priority& priority, std::string& due,
                  std::string& title, std::string& error) {
    std::size_t pos = 0;
    while (true) {
        skipSpaces(text, pos);
        if (pos >= text.size()) break;

        if (text.compare(pos, 2, "-p") == 0) {
            pos += 2;
            const std::string value = takeWord(text, pos);
            if (!parsePriority(value, priority)) {
                error = "unknown priority: " + value;
                return false;
            }
            continue;
        }
        if (text.compare(pos, 2, "-d") == 0) {
            pos += 2;
            const std::string value = takeWord(text, pos);
            if (!isValidDate(value)) {
                error = "invalid date (expected YYYY-MM-DD): " + value;
                return false;
            }
            due = value;
            continue;
        }
        break;
    }
    title = text.substr(pos);
    return true;
}

void saveTasks(const TaskManager& manager, const std::string& path) {
    if (!manager.save(path)) std::cout << "Warning: cannot save to " << path << "\n";
}

void printUsage() {
    std::cout << "Commands:\n"
                 "  add [-p 高|中|低] [-d YYYY-MM-DD] <标题>   add a task\n"
                 "  list                         show tasks (sorted by current mode)\n"
                 "  sort <id|priority|date>      change sort mode\n"
                 "  search <关键字>               search titles\n"
                 "  done <id>                    mark a task as done\n"
                 "  remove <id>                  delete a task\n"
                 "  load <file>                  switch to another data file\n"
                 "  save <file>                  export tasks to a file\n"
                 "  help                         show this text\n"
                 "  exit                         save and quit\n";
}

} // namespace

int main(int argc, char* argv[]) {
    std::string dataPath = defaultDataPath(argc > 0 ? argv[0] : nullptr);
    TaskManager manager;
    if (manager.load(dataPath)) {
        std::cout << "Loaded " << manager.size() << " task(s) from " << dataPath << "\n";
    }

    SortMode sortMode = SortMode::Id;
    std::string command;
    while (true) {
        std::cout << "Enter command (add/list/sort/search/done/remove/load/save/help/exit): ";
        if (!std::getline(std::cin, command)) break; // EOF: 停止而不是死循环
        if (command == "exit" || command == "quit") break;

        if (command == "list") {
            manager.list(sortMode);
        } else if (command == "help") {
            printUsage();
        } else if (command == "add") {
            std::cout << "Usage: add [-p 高|中|低] [-d YYYY-MM-DD] <标题>\n";
        } else if (command.rfind("add ", 0) == 0) {
            Priority priority = Priority::Medium;
            std::string due;
            std::string title;
            std::string error;
            if (!parseAddArgs(command.substr(4), priority, due, title, error)) {
                std::cout << "Add failed: " << error << "\n";
            } else if (!manager.add(title, priority, due)) {
                std::cout << "Task title is empty.\n";
            } else {
                saveTasks(manager, dataPath);
            }
        } else if (command == "sort") {
            std::cout << "Usage: sort <id|priority|date>\n";
        } else if (command.rfind("sort ", 0) == 0) {
            SortMode mode = SortMode::Id;
            if (!parseSortMode(command.substr(5), mode)) {
                std::cout << "Unknown sort mode.\n";
            } else {
                sortMode = mode;
                std::cout << "Sort mode: " << sortModeText(sortMode) << "\n";
                manager.list(sortMode);
            }
        } else if (command == "search") {
            std::cout << "Usage: search <关键字>\n";
        } else if (command.rfind("search ", 0) == 0) {
            const std::size_t found = manager.search(command.substr(7));
            std::cout << found << " match(es).\n";
        } else if (command == "done" || command == "remove") {
            std::cout << "Usage: " << command << " <id>\n";
        } else if (command.rfind("done ", 0) == 0) {
            int id = 0;
            if (!parseId(command.substr(5), id)) {
                std::cout << "Invalid task id.\n";
            } else if (!manager.markDone(id)) {
                std::cout << "Task not found.\n";
            } else {
                saveTasks(manager, dataPath);
            }
        } else if (command.rfind("remove ", 0) == 0) {
            int id = 0;
            if (!parseId(command.substr(7), id)) {
                std::cout << "Invalid task id.\n";
            } else if (!manager.remove(id)) {
                std::cout << "Task not found.\n";
            } else {
                saveTasks(manager, dataPath);
            }
        } else if (command == "load" || command == "save") {
            std::cout << "Usage: " << command << " <file>\n";
        } else if (command.rfind("load ", 0) == 0) {
            const std::string path = command.substr(5);
            if (!manager.load(path)) {
                std::cout << "Cannot read that file.\n";
            } else {
                dataPath = path; // 之后自动保存到这个文件
                std::cout << "Loaded " << manager.size() << " task(s) from " << dataPath << "\n";
            }
        } else if (command.rfind("save ", 0) == 0) {
            const std::string path = command.substr(5);
            if (!manager.save(path)) std::cout << "Cannot write that file.\n";
            else std::cout << "Saved " << manager.size() << " task(s) to " << path << "\n";
        } else {
            std::cout << "Unknown command.\n";
        }
    }

    saveTasks(manager, dataPath); // 退出时再存一次，防止中途异常中断丢失
    return 0;
}
