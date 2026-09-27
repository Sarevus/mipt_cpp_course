// Доступ к полям события. Как каждая функция сообщает об ошибке и почему —
// у её определения.

#include "fields.h"

#include <cctype>
#include <charconv>
#include <stdexcept>

namespace nano_edr {

namespace {

// unsigned char обязателен: tolower от отрицательного char — UB, а русские
// буквы в журнале есть.
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

// Два разделителя подряд склеиваются в один.
void AppendPathChar(std::string& path, char c) {
    if (c == '\\' && !path.empty() && path.back() == '\\') {
        return;
    }
    path.push_back(c);
}

}  // namespace

// nullptr, а не исключение: у file_write нет поля domain, и спрашивать
// про него законно. Ссылку тут вернуть было бы не на что.
const std::string* FindField(const Event& event, const std::string& key) {
    for (const Field& field : event.fields) {
        if (field.key == key) {
            return &field.value;
        }
    }
    return nullptr;
}

// Исключение: без обязательного поля нарушен контракт формата, и пустая
// строка вместо него соврала бы вызывающему. Ловится в main.
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

// false: «812abc», «-5» или переполнение — битые данные из журнала, а не
// ошибка программы. Строка должна разобраться целиком, иначе число врёт.
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

// Для полей, которых может и не быть (ppid): отказ — это fallback.
uint64_t GetIntField(const Event& event, const std::string& key,
                     uint64_t fallback) {
    uint64_t value = 0;
    if (GetIntField(event, key, &value)) {
        return value;
    }
    return fallback;
}

// Предикаты ничего не бросают: нет нужного поля — значит, ответ «нет».

bool IsProcessStart(const Event& event) {
    return event.type == "process_start";
}

bool IsFileWrite(const Event& event) {
    return event.type == "file_write";
}

bool IsNetConnect(const Event& event) {
    return event.type == "net_connect";
}

// Без учёта регистра: в Windows A.JS и a.js — один файл.
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

// Отказов нет. %TEMP% раскрываем не из окружения: журнал снят на чужой
// машине, правилу нужен только общий кусок \appdata\local\temp\.
std::string NormalizePath(const std::string& path) {
    const std::string kTempDir = R"(\appdata\local\temp)";
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
