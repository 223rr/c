#include "task.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    std::cout << (condition ? "PASS " : "FAIL ") << what << "\n";
    if (!condition) ++failures;
}

} // namespace

int main() {
    const std::string file = "test_tasks.txt";

    TaskManager manager;
    check(manager.add("写报告", Priority::High, "2026-05-01"), "add with priority and due date");
    check(manager.add("修 bug", Priority::Low), "add without due date");
    check(manager.add("买牛奶", Priority::Medium, "2026-01-15"), "add a third task");
    check(!manager.add("坏日期", Priority::Medium, "2026-13-01"), "add rejects an invalid date");
    check(!manager.add("   "), "add rejects a blank title");
    check(manager.size() == 3, "add appends tasks");

    Priority parsed = Priority::Low;
    check(parsePriority("高", parsed) && parsed == Priority::High, "parsePriority accepts 高");
    check(parsePriority("2", parsed) && parsed == Priority::Medium, "parsePriority accepts 2");
    check(!parsePriority("urgent", parsed), "parsePriority rejects unknown text");
    check(isValidDate("2026-02-29") == false, "2026 is not a leap year");
    check(isValidDate("2024-02-29"), "2024-02-29 is valid");

    const std::vector<Task> byPriority = manager.sorted(SortMode::Priority);
    check(byPriority.size() == 3 && byPriority[0].title == "写报告" && byPriority[2].title == "修 bug",
          "sort by priority puts 高 first and 低 last");

    const std::vector<Task> byDue = manager.sorted(SortMode::DueDate);
    check(byDue.size() == 3 && byDue[0].title == "买牛奶" && byDue[1].title == "写报告" &&
              byDue[2].title == "修 bug",
          "sort by due date puts the earliest first and undated last");

    check(manager.search("bug") == 1, "search is case-insensitive");
    check(manager.search("报告") == 1, "search matches a chinese title");
    check(manager.search("zzz") == 0, "search returns 0 when nothing matches");

    check(manager.markDone(1), "markDone finds an existing id");
    check(!manager.markDone(99), "markDone rejects an unknown id");
    check(manager.remove(3), "remove deletes an existing id");
    check(!manager.remove(99), "remove rejects an unknown id");

    check(manager.save(file), "save writes the file");

    TaskManager loaded;
    check(loaded.load(file), "load reads the file");
    check(loaded.size() == 2, "load restores the saved tasks");
    const std::vector<Task> loadedByDue = loaded.sorted(SortMode::DueDate);
    check(!loadedByDue.empty() && loadedByDue[0].due == "2026-05-01", "due date survives the round trip");
    check(!loadedByDue.empty() && loadedByDue[0].priority == Priority::High, "priority survives the round trip");
    check(!loadedByDue.empty() && loadedByDue[1].due.empty(), "an empty due date survives the round trip");
    check(loaded.markDone(1), "loaded task keeps its id");

    check(loaded.add("新任务"), "add works after load");
    const std::vector<Task> afterAdd = loaded.sorted(SortMode::Id);
    check(!afterAdd.empty() && afterAdd.back().id == 3, "numbering continues after load");

    // 旧格式（id done 标题）仍可读取
    const std::string legacy = "test_legacy.txt";
    if (std::FILE* f = std::fopen(legacy.c_str(), "w")) {
        std::fputs("1 1 旧格式标题 带空格\n", f);
        std::fclose(f);
    }
    TaskManager old;
    check(old.load(legacy), "load reads a legacy file");
    const std::vector<Task> oldTasks = old.sorted(SortMode::Id);
    check(oldTasks.size() == 1 && oldTasks[0].title == "旧格式标题 带空格",
          "legacy title is kept whole");
    check(!oldTasks.empty() && oldTasks[0].priority == Priority::Medium && oldTasks[0].due.empty(),
          "legacy defaults are applied");

    TaskManager missing;
    check(!missing.load("no_such_file.txt"), "load reports a missing file");
    check(missing.empty(), "failed load leaves the list empty");

    std::remove(file.c_str());
    std::remove(legacy.c_str());

    std::cout << (failures == 0 ? "ALL TESTS PASSED\n" : "TESTS FAILED\n");
    return failures == 0 ? 0 : 1;
}
