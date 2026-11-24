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

    // 修改优先级 / 截止日期 / 目标完成日
    TaskManager edit;
    check(edit.add("甲", Priority::Low, "2026-12-31", "2026-03-20"), "add a task to edit");
    check(edit.setPriority(1, Priority::High), "setPriority changes an existing task");
    check(!edit.setPriority(99, Priority::High), "setPriority rejects an unknown id");
    check(edit.sorted(SortMode::Priority)[0].priority == Priority::High, "changed priority affects sorting");
    check(edit.setDue(1, "2026-09-09"), "setDue sets a date");
    check(!edit.setDue(1, "2026-01-01"), "setDue rejects a due date earlier than the target");
    check(!edit.setDue(1, "2026-13-01"), "setDue rejects an invalid date");
    check(edit.setDue(1, ""), "setDue can clear the date");
    check(edit.sorted(SortMode::Id)[0].due.empty(), "cleared date is really empty");
    check(!edit.setDue(99, "2026-09-09"), "setDue rejects an unknown id");
    check(edit.contains(1) && !edit.contains(99), "contains reports membership");
    check(edit.show(1) && !edit.show(99), "show prints only existing tasks");

    // 目标完成日
    check(edit.sorted(SortMode::Id)[0].target == "2026-03-20", "target date is stored on add");
    check(edit.setDue(1, "2026-04-15"), "setDue accepts a due date after the target");
    check(edit.setTarget(1, "2026-04-01"), "setTarget sets a date");
    check(!edit.setTarget(1, "2026-05-01"), "setTarget rejects a target later than the due date");
    check(edit.sorted(SortMode::Id)[0].target == "2026-04-01", "rejected target keeps the old value");
    check(!edit.setTarget(1, "2026-02-30"), "setTarget rejects an invalid date");
    check(!edit.setTarget(99, "2026-04-01"), "setTarget rejects an unknown id");
    check(edit.setTarget(1, ""), "setTarget can clear the date");
    check(edit.sorted(SortMode::Id)[0].target.empty(), "cleared target is really empty");

    TaskManager order;
    check(!order.add("顺序", Priority::Medium, "2026-03-10", "2026-03-20"),
          "add rejects a target later than the due date");
    check(order.add("顺序", Priority::Medium, "2026-03-20", "2026-03-10"),
          "add accepts a target before the due date");
    check(datesConsistent("2026-03-20", "2026-03-20"), "target equal to due date is allowed");
    check(!datesConsistent("2026-03-10", "2026-03-20"), "target after due date is inconsistent");
    check(datesConsistent("", "2026-03-20") && datesConsistent("2026-03-20", ""),
          "a missing date means no constraint");

    const std::string targetFile = "test_target.txt";
    check(edit.setTarget(1, "2026-04-10"), "set target before saving");
    check(edit.save(targetFile), "save writes a target date");
    TaskManager targetLoaded;
    check(targetLoaded.load(targetFile), "load reads a target date");
    check(targetLoaded.sorted(SortMode::Id)[0].target == "2026-04-10", "target survives the round trip");
    std::remove(targetFile.c_str());

    // 撤销删除 / 清空已完成 / 编号连续
    TaskManager undoTest;
    check(undoTest.add("待删除"), "add a task for undo");
    const Task* found = undoTest.find(1);
    check(found != nullptr && found->title == "待删除", "find returns the task");
    const Task copy = *found;
    check(undoTest.remove(1), "remove the task");
    check(undoTest.find(1) == nullptr, "removed task is gone");
    check(undoTest.restore(copy), "restore brings the task back");
    check(undoTest.find(1) != nullptr, "restored task is found again");

    TaskManager numbering;
    numbering.add("甲");
    numbering.add("乙");
    numbering.add("丙");
    check(numbering.remove(2), "remove the middle task");
    const std::vector<Task> renumbered = numbering.sorted(SortMode::Id);
    check(renumbered.size() == 2 && renumbered[0].id == 1 && renumbered[1].id == 2 &&
              renumbered[1].title == "丙",
          "ids are renumbered after removal");
    check(numbering.add("丁"), "add after removal");
    check(numbering.sorted(SortMode::Id).back().id == 3, "new task continues from 3");

    check(undoTest.add("已完成"), "add a task to complete");
    undoTest.markDone(2);
    check(undoTest.removeDone() == 1, "removeDone deletes only completed tasks");
    check(undoTest.size() == 1, "one task left after clearing done");

    // 子任务
    TaskManager subs;
    check(subs.add("准备汇报"), "add a task with subtasks");
    check(subs.addSub(1, "整理数据"), "addSub adds the first subtask");
    check(subs.addSub(1, "制作PPT"), "addSub adds the second subtask");
    check(!subs.addSub(1, "   "), "addSub rejects an empty title");
    check(!subs.addSub(9, "x"), "addSub rejects an unknown id");
    check(subs.find(1) != nullptr && subs.find(1)->subs.size() == 2, "two subtasks stored");
    check(subs.toggleSub(1, 1), "toggleSub flips a subtask");
    check(subs.find(1)->subs[1].done, "subtask is now done");
    check(subs.toggleSub(1, 1) && !subs.find(1)->subs[1].done, "toggleSub flips back");
    check(!subs.toggleSub(1, 5), "toggleSub rejects an out-of-range index");
    check(subs.removeSub(1, 0), "removeSub deletes a subtask");
    check(subs.find(1)->subs.size() == 1 && subs.find(1)->subs[0].title == "制作PPT",
          "the remaining subtask is kept");
    check(!subs.removeSub(1, 5), "removeSub rejects an out-of-range index");

    const std::string subFile = "test_subs.txt";
    check(subs.save(subFile), "save writes subtasks");
    TaskManager subLoaded;
    check(subLoaded.load(subFile), "load reads subtasks");
    check(subLoaded.find(1) != nullptr && subLoaded.find(1)->subs.size() == 1,
          "subtask survives the round trip");
    std::remove(subFile.c_str());

    // 备注与附件
    TaskManager doc;
    doc.add("写文档");
    check(doc.setNotes(1, "第一行\n第二行"), "setNotes stores multi-line text");
    check(doc.find(1) != nullptr && doc.find(1)->notes == "第一行\n第二行", "notes keep the newline");
    check(doc.addFile(1, "1_a.txt"), "addFile attaches a file");
    check(!doc.addFile(1, "1_a.txt"), "addFile rejects duplicates");
    check(doc.removeFile(1, "1_a.txt"), "removeFile detaches the file");
    doc.addFile(1, "1_a.txt");

    const std::string docFile = "test_doc.txt";
    check(doc.save(docFile), "save writes notes and attachments");
    TaskManager docLoaded;
    check(docLoaded.load(docFile), "load reads notes and attachments");
    check(docLoaded.find(1) != nullptr && docLoaded.find(1)->notes == "第一行\n第二行",
          "notes survive the round trip");
    check(docLoaded.find(1) != nullptr && docLoaded.find(1)->files.size() == 1,
          "attachments survive the round trip");
    std::remove(docFile.c_str());

    // 重复任务
    TaskManager rep;
    check(rep.add("交周报", Priority::High, "2026-11-27", "", "weekly"), "add accepts a repeat rule");
    check(rep.find(1) != nullptr && rep.find(1)->repeat == "weekly", "repeat rule is stored");
    check(!rep.add("坏规则", Priority::Medium, "2026-11-27", "", "yearly"), "add rejects an unknown repeat rule");
    check(rep.setRepeat(1, "daily"), "setRepeat changes the rule");
    check(rep.find(1)->repeat == "daily", "repeat rule is updated");
    check(!rep.setRepeat(1, "hourly"), "setRepeat rejects an unknown rule");
    check(rep.setRepeat(1, ""), "setRepeat can clear the rule");
    check(rep.find(1)->repeat.empty(), "repeat rule is cleared");
    rep.setRepeat(1, "monthly");

    const std::string repeatFile = "test_repeat.txt";
    check(rep.save(repeatFile), "save writes the repeat rule");
    TaskManager repeatLoaded;
    check(repeatLoaded.load(repeatFile), "load reads the repeat rule");
    check(repeatLoaded.find(1) != nullptr && repeatLoaded.find(1)->repeat == "monthly",
          "repeat rule survives the round trip");
    std::remove(repeatFile.c_str());

    // 完成日期（复盘/打卡用）
    TaskManager hist;
    check(hist.add("打卡任务", Priority::Medium, "2026-11-24"), "add a task to complete");
    check(hist.setDoneAt(1, "2026-11-24"), "setDoneAt records a date");
    check(hist.find(1) != nullptr && hist.find(1)->doneAt == "2026-11-24", "doneAt is stored");
    check(!hist.setDoneAt(1, "2026-13-01"), "setDoneAt rejects an invalid date");
    check(!hist.setDoneAt(99, "2026-11-24"), "setDoneAt rejects an unknown id");

    const std::string histFile = "test_doneat.txt";
    check(hist.save(histFile), "save writes doneAt");
    TaskManager histLoaded;
    check(histLoaded.load(histFile), "load reads doneAt");
    check(histLoaded.find(1) != nullptr && histLoaded.find(1)->doneAt == "2026-11-24",
          "doneAt survives the round trip");
    check(histLoaded.setDoneAt(1, ""), "setDoneAt can be cleared");
    check(histLoaded.find(1)->doneAt.empty(), "cleared doneAt is empty");
    std::remove(histFile.c_str());

    // 专注时长与贴纸
    TaskManager extras;
    extras.add("专注任务");
    check(extras.addSpent(1, 1500), "addSpent accumulates");
    check(extras.addSpent(1, 300), "addSpent accumulates again");
    check(extras.find(1) != nullptr && extras.find(1)->spent == 1800, "spent total is stored");
    check(!extras.addSpent(1, 0), "addSpent rejects zero");
    check(!extras.addSpent(99, 60), "addSpent rejects an unknown id");
    check(extras.setSticker(1, "⭐"), "setSticker sets an emoji");
    check(extras.find(1) != nullptr && extras.find(1)->sticker == "⭐", "sticker is stored");
    check(extras.setSticker(1, ""), "setSticker can be cleared");
    extras.setSticker(1, "🍀");

    const std::string extrasFile = "test_extras.txt";
    check(extras.save(extrasFile), "save writes spent and sticker");
    TaskManager extrasLoaded;
    check(extrasLoaded.load(extrasFile), "load reads spent and sticker");
    check(extrasLoaded.find(1) != nullptr && extrasLoaded.find(1)->spent == 1800,
          "spent survives the round trip");
    check(extrasLoaded.find(1) != nullptr && extrasLoaded.find(1)->sticker == "🍀",
          "sticker survives the round trip");
    std::remove(extrasFile.c_str());

    // 标签
    TaskManager tagged;
    tagged.add("贴标签");
    check(tagged.addTag(1, "工作"), "addTag adds a tag");
    check(tagged.addTag(1, "生活"), "addTag adds a second tag");
    check(!tagged.addTag(1, "工作"), "addTag rejects duplicates");
    check(!tagged.addTag(1, "   "), "addTag rejects an empty tag");
    check(!tagged.addTag(99, "x"), "addTag rejects an unknown id");
    check(tagged.find(1) != nullptr && tagged.find(1)->tags.size() == 2, "two tags stored");
    check(tagged.removeTag(1, "工作"), "removeTag removes a tag");
    check(!tagged.removeTag(1, "工作"), "removeTag rejects a missing tag");
    check(tagged.find(1) != nullptr && tagged.find(1)->tags.size() == 1, "one tag left");

    const std::string tagFile = "test_tags.txt";
    check(tagged.save(tagFile), "save writes tags");
    TaskManager tagLoaded;
    check(tagLoaded.load(tagFile), "load reads tags");
    check(tagLoaded.find(1) != nullptr && tagLoaded.find(1)->tags.size() == 1 &&
              tagLoaded.find(1)->tags[0] == "生活",
          "tags survive the round trip");
    std::remove(tagFile.c_str());

    std::remove(file.c_str());
    std::remove(legacy.c_str());

    std::cout << (failures == 0 ? "ALL TESTS PASSED\n" : "TESTS FAILED\n");
    return failures == 0 ? 0 : 1;
}
