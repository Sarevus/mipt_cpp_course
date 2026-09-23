#include "parse.h"

#include <cstddef>
#include <string>

#include "event.h"

namespace nano_edr {

namespace {

bool IsSpace(char c) {
    return c == ' ' || c == '\t';
}

// Первое вхождение ts, type и pid занимает шапку, всё остальное — в fields.
struct Header {
    bool has_ts = false;
    bool has_type = false;
    bool has_pid = false;
};

void StorePair(const std::string* key, const std::string* value, Header* header,
               Event* out) {
    if (*key == "ts" && !header->has_ts) {
        out->ts = *value;
        header->has_ts = true;
    } else if (*key == "type" && !header->has_type) {
        out->type = *value;
        header->has_type = true;
    } else if (*key == "pid" && !header->has_pid) {
        out->pid = *value;
        header->has_pid = true;
    } else {
        Field field;
        field.key = *key;
        field.value = *value;
        out->fields.push_back(field);
    }
}

}  // namespace

bool IsBlankOrComment(const std::string* line) {
    for (char c : *line) {
        if (!IsSpace(c)) {
            return c == '#' || c == ';';
        }
    }
    return true;
}

bool ParseEventLine(const std::string* line, Event* out) {
    if (IsBlankOrComment(line)) {
        return false;
    }

    *out = Event();
    Header header;
    const std::size_t size = line->size();
    std::size_t i = 0;

    while (true) {
        while (i < size && IsSpace((*line)[i])) {
            ++i;
        }
        if (i == size) {
            break;
        }

        const std::size_t key_begin = i;
        while (i < size && (*line)[i] != '=' && !IsSpace((*line)[i])) {
            ++i;
        }
        if (i == size || (*line)[i] != '=' || i == key_begin) {
            return false;
        }
        const std::string key = line->substr(key_begin, i - key_begin);
        ++i;

        std::string value;
        if (i < size && (*line)[i] == '"') {
            const std::size_t close = line->find('"', i + 1);
            if (close == std::string::npos) {
                return false;
            }
            value = line->substr(i + 1, close - i - 1);
            i = close + 1;
            if (i < size && !IsSpace((*line)[i])) {
                return false;
            }
        } else {
            const std::size_t value_begin = i;
            while (i < size && !IsSpace((*line)[i])) {
                ++i;
            }
            value = line->substr(value_begin, i - value_begin);
        }

        StorePair(&key, &value, &header, out);
    }

    return header.has_ts && header.has_type;
}

}  // namespace nano_edr
