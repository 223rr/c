#include "task.h"

#include <cstdio>
#include <iostream>
#include <string>

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
    check(manager.add("Test task 1"), "add accepts a title");
    check(manager.add("Test task 2"), "add accepts a second title");
    check(!manager.add("   "), "add rejects a blank title");
    check(manager.size() == 2, "add appends tasks");

    check(manager.markDone(1), "markDone finds an existing id");
    check(!manager.markDone(3), "markDone rejects an unknown id");
    check(manager.remove(2), "remove deletes an existing id");
    check(!manager.remove(3), "remove rejects an unknown id");
    check(manager.save(file), "save writes the file");

    TaskManager loaded;
    check(loaded.load(file), "load reads the file");
    check(loaded.size() == 1, "load restores the saved tasks");
    check(loaded.markDone(1), "loaded task keeps its id");
    check(loaded.add("Test task 3"), "add works after load");
    check(loaded.size() == 2, "loaded manager keeps numbering");

    TaskManager missing;
    check(!missing.load("no_such_file.txt"), "load reports a missing file");
    check(missing.empty(), "failed load leaves the list empty");

    const std::string badFile = "test_bad.txt";
    if (std::FILE* f = std::fopen(badFile.c_str(), "w")) {
        std::fputs("garbage line\n\n-4 0 negative\n3 0 real task\n", f);
        std::fclose(f);
    }
    TaskManager tolerant;
    check(tolerant.load(badFile), "load tolerates malformed files");
    check(tolerant.size() == 1, "malformed lines are skipped");

    std::remove(file.c_str());
    std::remove(badFile.c_str());

    std::cout << (failures == 0 ? "ALL TESTS PASSED\n" : "TESTS FAILED\n");
    return failures == 0 ? 0 : 1;
}
