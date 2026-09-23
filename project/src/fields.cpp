// Как функции доступа к полям сообщают об ошибке.
//
//   FindField            nullptr, а не исключение: у file_write нет поля
//                        domain, и спрашивать про него законно. Отсутствие
//                        поля — один из двух ожидаемых исходов, и вызывающий
//                        проверяет его на месте.
//   GetRequiredField     std::invalid_argument: поле объявлено обязательным,
//                        и его отсутствие — нарушение контракта формата.
//                        Пустая строка вместо него соврала бы вызывающему,
//                        а обработчик у такой ошибки один — в main.
//   GetIntField (out)    false: значение приходит из журнала, и «812abc» или
//                        переполнение — битые внешние данные, а не ошибка
//                        программы. Их ждут и обрабатывают на месте.
//   GetIntField          fallback: для полей, отсутствие которых осмысленно,
//   (fallback)           отказ и есть значение по умолчанию.
//   Предикаты            false: вопрос о событии — всегда да или нет. Событие
//                        другого типа или без нужного поля — просто «нет».
//   NormalizePath        отказов нет: любая строка приводится к виду.

#include "fields.h"

#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <system_error>

#include "event.h"

namespace nano_edr {

namespace {

std::string ToLower(const std::string& text) {
    std::string result = text;
    for (char& c : result) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

bool EndsWith(const std::string& text, const std::string& suffix) {
    if (text.size() < suffix.size()) {
        return false;
    }
    return text.compare(text.size() - suffix.size(), suffix.size(), suffix) ==
           0;
}

// Дописывает символ пути, склеивая идущие подряд разделители.
void AppendPathChar(std::string& path, char c) {
    if (c == '\\' && !path.empty() && path.back() == '\\') {
        return;
    }
    path.push_back(c);
}

}  // namespace

const std::string* FindField(const Event& event, const std::string& key) {
    for (const Field& field : event.fields) {
        if (field.key == key) {
            return &field.value;
        }
    }
    return nullptr;
}

const std::string& GetRequiredField(const Event& event,
                                    const std::string& key) {
    const std::string* value = FindField(event, key);
    if (value == nullptr) {
        throw std::invalid_argument("у события " + event.type + " ts=" +
                                    event.ts + " нет обязательного поля " +
                                    key);
    }
    return *value;
}

bool GetIntField(const Event& event, const std::string& key, uint64_t* out) {
    const std::string* text = FindField(event, key);
    if (text == nullptr) {
        return false;
    }

    const char* begin = text->data();
    const char* end = begin + text->size();
    uint64_t value = 0;
    const std::from_chars_result result = std::from_chars(begin, end, value);
    if (result.ec != std::errc() || result.ptr != end) {
        return false;
    }

    *out = value;
    return true;
}

uint64_t GetIntField(const Event& event, const std::string& key,
                     uint64_t fallback) {
    uint64_t value = 0;
    if (GetIntField(event, key, &value)) {
        return value;
    }
    return fallback;
}

bool IsProcessStart(const Event& event) {
    return event.type == "process_start";
}

bool IsFileWrite(const Event& event) {
    return event.type == "file_write";
}

bool IsNetConnect(const Event& event) {
    return event.type == "net_connect";
}

bool PathEndsWith(const Event& event, const std::string& suffix) {
    const std::string* path = FindField(event, "path");
    if (path == nullptr) {
        return false;
    }
    return EndsWith(ToLower(*path), ToLower(suffix));
}

bool CommandLineContains(const Event& event, const std::string& needle) {
    const std::string* cmdline = FindField(event, "cmdline");
    if (cmdline == nullptr) {
        return false;
    }
    return ToLower(*cmdline).find(ToLower(needle)) != std::string::npos;
}

std::string NormalizePath(const std::string& path) {
    const std::string kTempDir = "\\appdata\\local\\temp";
    const std::string lower = ToLower(path);

    std::string result;
    std::size_t i = 0;
    while (i < lower.size()) {
        std::size_t variable_size = 0;
        if (lower.compare(i, 6, "%temp%") == 0) {
            variable_size = 6;
        } else if (lower.compare(i, 5, "%tmp%") == 0) {
            variable_size = 5;
        }

        if (variable_size > 0) {
            for (char c : kTempDir) {
                AppendPathChar(result, c);
            }
            i += variable_size;
            continue;
        }

        const char c = lower[i] == '/' ? '\\' : lower[i];
        AppendPathChar(result, c);
        ++i;
    }
    return result;
}

}  // namespace nano_edr
