#include <charconv>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <fstream>
#include <print>
#include <string>
#include <vector>

#include "agent_rules.h"
#include "event_list.h"
#include "parse.h"

using nano_edr::AgentRuleCount;
using nano_edr::AgentRules;
using nano_edr::CheckRules;
using nano_edr::Event;
using nano_edr::EventList;
using nano_edr::EventNode;
using nano_edr::IsBlankOrComment;
using nano_edr::ListPushBack;
using nano_edr::ParseEventLine;

namespace {

constexpr std::size_t kDefaultWindowSize = 64;

struct Options {
    std::string log_path;
    bool quiet = false;
    std::size_t window_size = kDefaultWindowSize;
};

struct TypeCount {
    std::string type;
    uint64_t count = 0;
};

struct Summary {
    uint64_t lines = 0;
    uint64_t events = 0;
    uint64_t skipped = 0;
    uint64_t broken = 0;
    uint64_t detects = 0;
    std::vector<TypeCount> types;
};

// out меняется только при успехе.
bool ParseWindowSize(const std::string& text, std::size_t& out) {
    const char* end = text.data() + text.size();
    std::size_t value = 0;
    const std::from_chars_result result =
        std::from_chars(text.data(), end, value);
    if (result.ec != std::errc() || result.ptr != end) {
        return false;
    }
    out = value;
    return true;
}

// false — причина уже в stderr. Код возврата, а не исключение: обработать
// ошибку можно сразу в main, пробрасывать её некуда.
bool ParseArgs(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--quiet") {
            options.quiet = true;
        } else if (arg == "--window-size") {
            if (i + 1 >= argc || !ParseWindowSize(argv[i + 1],
                                                   options.window_size)) {
                std::print(stderr, "--window-size ждёт целое число >= 0\n");
                return false;
            }
            ++i;
        } else if (arg.starts_with("--")) {
            std::print(stderr, "неизвестный ключ: {}\n", arg);
            return false;
        } else if (options.log_path.empty()) {
            options.log_path = arg;
        } else {
            std::print(stderr, "лишний аргумент: {}\n", arg);
            return false;
        }
    }

    if (options.log_path.empty()) {
        std::print(stderr,
                   "использование: nano-edr <журнал.log> [--quiet] "
                   "[--window-size N]\n");
        return false;
    }
    return true;
}

void CountType(std::vector<TypeCount>& types, const std::string& type) {
    for (TypeCount& entry : types) {
        if (entry.type == type) {
            ++entry.count;
            return;
        }
    }
    TypeCount entry;
    entry.type = type;
    entry.count = 1;
    types.push_back(entry);
}

// Два последних события окна, старшее первым. Последнее — хвост окна,
// предпоследнее вызывающий хранит копией: за ним по односвязному списку
// пришлось бы идти от головы. Копия, а не указатель на узел, — узел может
// уже быть удалён, когда окно полно.
void PrintContext(const EventList& window, const Event& before_last) {
    if (window.size >= 2) {
        std::print("[CTX] -2: ts={} type={} pid={}\n", before_last.ts,
                   before_last.type, before_last.pid);
    }
    if (window.tail != nullptr) {
        const Event& last = window.tail->event;
        std::print("[CTX] -1: ts={} type={} pid={}\n", last.ts, last.type,
                   last.pid);
    }
}

// Битая строка идёт в счётчик, прогон продолжается. Исключение из правил
// не ловим: оно уходит в main, а узлы окна вернёт деструктор.
Summary Run(std::ifstream& log, const Options& options) {
    Summary summary;
    EventList window;
    window.capacity = options.window_size;
    // Предпоследнее событие окна; осмысленно, только пока window.size >= 2.
    Event before_last;

    std::string line;
    while (std::getline(log, line)) {
        ++summary.lines;

        if (IsBlankOrComment(&line)) {
            ++summary.skipped;
            continue;
        }

        Event event;
        if (!ParseEventLine(&line, &event)) {
            ++summary.broken;
            continue;
        }

        ++summary.events;
        CountType(summary.types, event.type);

        const std::size_t detects =
            CheckRules(event, AgentRules(), AgentRuleCount());
        summary.detects += detects;
        // Контекст — то, что было до детекта, поэтому событие идёт в окно
        // только после печати.
        if (detects > 0 && !options.quiet) {
            PrintContext(window, before_last);
        }
        if (window.tail != nullptr) {
            before_last = window.tail->event;
        }
        ListPushBack(&window, &event);
    }
    return summary;
}

void PrintSummary(const Summary& summary) {
    std::print("строк {}, событий {}, пропущено {}, битых {}, детектов {}\n",
               summary.lines, summary.events, summary.skipped, summary.broken,
               summary.detects);
    for (const TypeCount& entry : summary.types) {
        std::print("  {}: {}\n", entry.type, entry.count);
    }
}

}  // namespace

// 0 — прогон дошёл до конца, 1 — нарушен контракт формата,
// 2 — работать не с чем (аргументы, файл не открылся).
int main(int argc, char** argv) {
    try {
        Options options;
        if (!ParseArgs(argc, argv, options)) {
            return 2;
        }

        std::ifstream log(options.log_path);
        if (!log) {
            std::print(stderr, "не удалось открыть журнал: {}\n",
                       options.log_path);
            return 2;
        }

        const Summary summary = Run(log, options);
        if (!options.quiet) {
            PrintSummary(summary);
        }
        return 0;
    } catch (const std::exception& error) {
        std::print(stderr, "прогон прерван: {}\n", error.what());
        return 1;
    }
}
