#include "rules.h"

#include <memory>

#include "detect_line.h"

namespace nano_edr {

// "?" вместо исключения: такое значение бывает только из битого каста,
// и ронять из-за него прогон незачем.
const char* SeverityName(Severity severity) {
    switch (severity) {
        case Severity::kLow:
            return "low";
        case Severity::kMedium:
            return "medium";
        case Severity::kHigh:
            return "high";
        case Severity::kCritical:
            return "critical";
    }
    return "?";
}

// Исключения из правил не ловим — их ловит main.
size_t CheckRules(const Event& event, const Rule* rules, size_t rule_count) {
    size_t detects = 0;
    for (size_t i = 0; i < rule_count; ++i) {
        if (rules[i].check(event)) {
            // Задание 3. Голый `DetectLine* line = new ...` сам не удалится:
            // деструктор не вызовется, строки детекта не будет, память утечёт.
            // 3а: хватило бы `delete line;` после `++detects;`, но исключение
            // между new и delete этот delete перепрыгивает (3б). unique_ptr
            // удаляет объект на любом выходе из области видимости, в том числе
            // при раскрутке стека, а сам объект остаётся в динамической памяти.
            std::unique_ptr<DetectLine> line = std::make_unique<DetectLine>(DetectLine{rules[i], event});
            // std::print("[DETECT] {}  {}  ts={} pid={}\n",
            //            SeverityName(rules[i].severity), rules[i].id, event.ts,
            //            event.pid);
            ++detects;
        }
    }
    return detects;
}

}  // namespace nano_edr
