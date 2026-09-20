// Занятие 1.1: читаем журнал, считаем события по типам, печатаем детекты.
//   nano-edr <журнал.log> [--quiet]
#include <cstdio>
#include <fstream>
#include <map>
#include <print>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    std::string path;
    bool quiet = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--quiet") {
            quiet = true;
        } else {
            path = arg;
        }
    }
    if (path.empty()) {
        std::print(stderr, "использование: nano-edr <журнал.log> [--quiet]\n");
        return 2;
    }

    std::ifstream log(path);
    if (!log) {
        std::print(stderr, "не удалось открыть журнал: {}\n", path);
        return 2;
    }

    // Порядок признаков — часть формата вывода, а не оформление списка.
    const std::vector<std::string> marks = {
        "wscript.exe", ".locked", "certutil.exe", "\\Startup\\"};

    std::map<std::string, int> by_type;
    int lines = 0;
    int events = 0;
    std::string line;

    while (std::getline(log, line)) {
        ++lines;  // считаем все строки файла, включая пустые и комментарии

        // Комментарий может начинаться с отступа, и это всё ещё комментарий.
        std::size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        if (line[start] == '#' || line[start] == ';') continue;
        ++events;

        std::size_t t = line.find("type=");
        if (t != std::string::npos) {
            t += 5;
            ++by_type[line.substr(t, line.find(' ', t) - t)];
        }

        for (const std::string& mark : marks) {
            if (line.find(mark) != std::string::npos) {
                std::print("[DETECT] строка {}, признак {}: {}\n",
                           lines, mark, line);
            }
        }
    }

    if (!quiet) {
        std::print("\nстрок {}, событий {}\n", lines, events);
        for (const auto& [type, count] : by_type) {
            std::print("  {} — {}\n", type, count);
        }
    }
}
