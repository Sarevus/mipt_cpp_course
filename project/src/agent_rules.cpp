#include "agent_rules.h"

#include "fields.h"

namespace nano_edr {

namespace {

bool Contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

bool EndsWith(const std::string& text, const std::string& suffix) {
    if (text.size() < suffix.size()) {
        return false;
    }
    return text.compare(text.size() - suffix.size(), suffix.size(), suffix) ==
           0;
}

// Сравниваем имя файла, а не подстроку: cmd.exe со словом wscript
// в командной строке — не скриптовый хост.
bool ImageIs(const std::string& image, const std::string& name) {
    return image == name || EndsWith(image, "\\" + name);
}

// image обязателен: без него правило про образ не проверить, поэтому
// исключение из GetRequiredField, а не тихий false.
std::string ProcessImage(const Event& event) {
    return NormalizePath(GetRequiredField(event, "image"));
}

// Без cmdline процесс законен, просто проверять нечего — отсюда false.
bool NormalizedCommandLine(const Event& event, std::string& out) {
    const std::string* cmdline = FindField(event, "cmdline");
    if (cmdline == nullptr) {
        return false;
    }
    out = NormalizePath(*cmdline);
    return true;
}

bool IsFileChange(const Event& event) {
    return event.type == "file_create" || IsFileWrite(event) ||
           event.type == "file_move";
}

// У file_move путь назначения лежит в поле to, у остальных — в path.
const std::string* TargetPath(const Event& event) {
    if (event.type == "file_move") {
        return FindField(event, "to");
    }
    return FindField(event, "path");
}

// wscript законно запускают и из C:\corp\tools, детект — только из Temp.
bool ScriptHostFromTemp(const Event& event) {
    if (!IsProcessStart(event)) {
        return false;
    }
    const std::string image = ProcessImage(event);
    if (!ImageIs(image, "wscript.exe") && !ImageIs(image, "cscript.exe")) {
        return false;
    }
    std::string cmdline;
    if (!NormalizedCommandLine(event, cmdline)) {
        return false;
    }
    return Contains(cmdline, R"(\appdata\local\temp\)") ||
           Contains(cmdline, R"(\windows\temp\)");
}

// http: без слешей: после NormalizePath "//" превращается в "\".
bool LolbinDownload(const Event& event) {
    if (!IsProcessStart(event)) {
        return false;
    }
    const std::string image = ProcessImage(event);
    if (!ImageIs(image, "certutil.exe") && !ImageIs(image, "bitsadmin.exe")) {
        return false;
    }
    std::string cmdline;
    if (!NormalizedCommandLine(event, cmdline)) {
        return false;
    }
    return Contains(cmdline, "urlcache") || Contains(cmdline, "transfer") ||
           Contains(cmdline, "http:") || Contains(cmdline, "https:");
}

bool HiddenPowershell(const Event& event) {
    if (!IsProcessStart(event)) {
        return false;
    }
    const std::string image = ProcessImage(event);
    if (!ImageIs(image, "powershell.exe") && !ImageIs(image, "pwsh.exe")) {
        return false;
    }
    std::string cmdline;
    if (!NormalizedCommandLine(event, cmdline)) {
        return false;
    }
    return Contains(cmdline, "-w hidden") ||
           Contains(cmdline, "-windowstyle hidden") ||
           Contains(cmdline, "-enc") || Contains(cmdline, "-encodedcommand");
}

// Полный путь, а не просто \startup\: у Word есть свой STARTUP
// для временных файлов, и это не автозагрузка.
bool AutostartWrite(const Event& event) {
    if (!IsFileChange(event)) {
        return false;
    }
    const std::string* path = TargetPath(event);
    if (path == nullptr) {
        return false;
    }
    return Contains(NormalizePath(*path), R"(\start menu\programs\startup\)");
}

// Именно окончание: report.locked.docx — не шифровальщик.
bool RansomExtension(const Event& event) {
    if (!IsFileChange(event)) {
        return false;
    }
    const std::string* path = TargetPath(event);
    if (path == nullptr) {
        return false;
    }
    return EndsWith(NormalizePath(*path), ".locked");
}

constexpr Rule kRules[] = {
    {"script_host_from_temp", ScriptHostFromTemp, Severity::kHigh},
    {"lolbin_download", LolbinDownload, Severity::kHigh},
    {"hidden_powershell", HiddenPowershell, Severity::kMedium},
    {"autostart_write", AutostartWrite, Severity::kHigh},
    {"ransom_extension", RansomExtension, Severity::kCritical},
};

}  // namespace

const Rule* AgentRules() {
    return kRules;
}

size_t AgentRuleCount() {
    return sizeof(kRules) / sizeof(kRules[0]);
}

}  // namespace nano_edr
