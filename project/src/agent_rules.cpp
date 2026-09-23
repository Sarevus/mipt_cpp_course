#include "agent_rules.h"

#include <cstddef>
#include <iterator>
#include <string>

#include "event.h"
#include "fields.h"
#include "rules.h"

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

// Сравнивается имя файла образа, а не подстрока: cmd.exe со словом wscript
// в командной строке скриптовым хостом не становится.
bool ImageIs(const std::string& image, const std::string& name) {
    return image == name || EndsWith(image, "\\" + name);
}

// Образ обязателен: process_start без image бросает из GetRequiredField.
std::string ProcessImage(const Event& event) {
    return NormalizePath(GetRequiredField(event, "image"));
}

// false, если командной строки нет: проверять правилу нечего.
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
    return Contains(cmdline, "\\appdata\\local\\temp\\") ||
           Contains(cmdline, "\\windows\\temp\\");
}

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

bool AutostartWrite(const Event& event) {
    if (!IsFileChange(event)) {
        return false;
    }
    const std::string* path = TargetPath(event);
    if (path == nullptr) {
        return false;
    }
    return Contains(NormalizePath(*path), "\\start menu\\programs\\startup\\");
}

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
    return std::size(kRules);
}

}  // namespace nano_edr
