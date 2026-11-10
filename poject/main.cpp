#include "task.h"

#include <iostream>
#include <sstream>
#include <string>

namespace {

bool parseId(const std::string& text, int& id) {
    std::istringstream ss(text);
    if (!(ss >> id)) return false;
    std::string trailing;
    return !(ss >> trailing); // reject "12abc" and "1 2"
}

void printUsage() {
    std::cout << "Commands:\n"
                 "  add <title>   add a task\n"
                 "  list          show all tasks\n"
                 "  done <id>     mark a task as done\n"
                 "  remove <id>   delete a task\n"
                 "  load <file>   load tasks from a file\n"
                 "  save <file>   save tasks to a file\n"
                 "  help          show this text\n"
                 "  exit          quit\n";
}

} // namespace

int main() {
    TaskManager manager;
    std::string command;
    while (true) {
        std::cout << "Enter command (add/list/done/remove/load/save/help/exit): ";
        if (!std::getline(std::cin, command)) break; // EOF: stop instead of looping forever
        if (command == "exit" || command == "quit") break;

        if (command == "list") {
            manager.list();
        } else if (command == "help") {
            printUsage();
        } else if (command == "add") {
            std::cout << "Usage: add <title>\n";
        } else if (command.rfind("add ", 0) == 0) {
            if (!manager.add(command.substr(4))) std::cout << "Task title is empty.\n";
        } else if (command == "done" || command == "remove") {
            std::cout << "Usage: " << command << " <id>\n";
        } else if (command.rfind("done ", 0) == 0) {
            int id = 0;
            if (!parseId(command.substr(5), id)) {
                std::cout << "Invalid task id.\n";
            } else if (!manager.markDone(id)) {
                std::cout << "Task not found.\n";
            }
        } else if (command.rfind("remove ", 0) == 0) {
            int id = 0;
            if (!parseId(command.substr(7), id)) {
                std::cout << "Invalid task id.\n";
            } else if (!manager.remove(id)) {
                std::cout << "Task not found.\n";
            }
        } else if (command == "load" || command == "save") {
            std::cout << "Usage: " << command << " <file>\n";
        } else if (command.rfind("load ", 0) == 0) {
            if (!manager.load(command.substr(5))) std::cout << "Cannot read that file.\n";
        } else if (command.rfind("save ", 0) == 0) {
            if (!manager.save(command.substr(5))) std::cout << "Cannot write that file.\n";
        } else {
            std::cout << "Unknown command.\n";
        }
    }
    return 0;
}
