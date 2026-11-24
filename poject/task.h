#pragma once
#include <cstddef>
#include <string>
#include <vector>

enum class Priority { Low = 1, Medium = 2, High = 3 };

enum class SortMode { Id, Priority, DueDate };

struct SubTask {
    std::string title;
    bool done = false;
};

struct Task {
    int id = 0;
    std::string title;
    bool done = false;
    Priority priority = Priority::Medium;
    std::string due;    // 截止日期 YYYY-MM-DD，空表示没有
    std::string target; // 目标完成日 YYYY-MM-DD，空表示没有
    std::vector<SubTask> subs;       // 子任务清单
    std::string notes;               // 多行备注
    std::vector<std::string> files;  // 附件文件名
    std::string repeat;              // 重复：daily / weekly / monthly，空表示不重复
    std::string doneAt;              // 完成日期 YYYY-MM-DD，用于复盘/打卡
    int spent = 0;                   // 累计专注秒数
    std::string sticker;             // 贴纸（一个 emoji）
    std::vector<std::string> tags;   // 自定义标签，如 工作 / 生活
};

class TaskManager {
public:
    // 添加任务；标题为空或日期非法时返回 false。
    bool add(const std::string& title,
             Priority priority = Priority::Medium,
             const std::string& due = std::string(),
             const std::string& target = std::string(),
             const std::string& repeat = std::string());
    void list(SortMode mode = SortMode::Id) const;
    std::vector<Task> sorted(SortMode mode) const;
    // 按标题子串（不区分大小写）搜索并打印结果，返回命中数量。
    std::size_t search(const std::string& keyword) const;
    bool markDone(int id);
    bool markUndone(int id);
    // 把已删除的任务放回列表（用于撤销），id 已存在时返回 false。
    bool restore(const Task& task);
    // 删除所有已完成任务，返回删除数量。
    std::size_t removeDone();
    // 查找任务，不存在返回 nullptr。
    const Task* find(int id) const;
    // 子任务：增、勾选/取消、删。
    bool addSub(int id, const std::string& title);
    bool toggleSub(int id, std::size_t index);
    bool removeSub(int id, std::size_t index);
    // 备注与附件。
    bool setNotes(int id, const std::string& notes);
    bool addFile(int id, const std::string& name);
    bool removeFile(int id, const std::string& name);
    // 重复规则："" / daily / weekly / monthly。
    bool setRepeat(int id, const std::string& repeat);
    // 记录完成日期（空表示清空）。
    bool setDoneAt(int id, const std::string& date);
    // 专注计时累加秒数。
    bool addSpent(int id, int seconds);
    // 贴纸（空表示清除）。
    bool setSticker(int id, const std::string& sticker);
    // 标签增删。
    bool addTag(int id, const std::string& tag);
    bool removeTag(int id, const std::string& tag);
    bool remove(int id);
    bool setPriority(int id, Priority priority);
    // due 为空表示清除截止日期；日期非法或 id 不存在时返回 false。
    bool setDue(int id, const std::string& due);
    // target 为空表示清除目标完成日。
    bool setTarget(int id, const std::string& target);
    bool contains(int id) const;
    // 打印单个任务，id 不存在时返回 false。
    bool show(int id) const;
    bool load(const std::string& path);
    bool save(const std::string& path) const;

    std::size_t size() const { return tasks.size(); }
    bool empty() const { return tasks.empty(); }

private:
    // 让编号从 1 开始连续排列（删除后调用）
    void renumber();

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
// 截止日是「目标完成的最后日期」：目标完成日不得晚于截止日，任一为空表示不限制。
bool datesConsistent(const std::string& due, const std::string& target);
