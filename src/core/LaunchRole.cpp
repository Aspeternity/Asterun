#include "LaunchRole.hpp"

#include "LaunchCatalog.hpp"
#include "Command.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cwctype>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <unordered_map>
#include <vector>

namespace altrun {
namespace {

enum class EvidenceField : unsigned {
    Title = 1u << 0,
    Description = 1u << 1,
    OriginalFilename = 1u << 2,
    InternalName = 1u << 3,
    Arguments = 1u << 4,
    Structural = 1u << 5,
    ProductRelation = 1u << 6,
    TargetName = 1u << 7,
};

struct RoleScore {
    int points{0};
    unsigned fields{0};
    bool structuralHigh{false};
};

[[nodiscard]] std::wstring Lower(
    std::wstring_view value) {
    std::wstring result(value);
    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](wchar_t ch) {
            return static_cast<wchar_t>(
                std::towlower(ch));
        });
    return result;
}

[[nodiscard]] std::wstring Compact(
    std::wstring_view value) {
    std::wstring result;
    result.reserve(value.size());

    for (const wchar_t ch : value) {
        if (std::iswalnum(ch) ||
            ch >= 0x4E00) {
            result.push_back(
                static_cast<wchar_t>(
                    std::towlower(ch)));
        }
    }

    return result;
}

[[nodiscard]]
std::vector<std::wstring> Tokens(
    std::wstring_view value) {
    std::vector<std::wstring> result;
    std::wstring current;

    auto flush = [&]() {
        if (current.empty()) {
            return;
        }

        std::transform(
            current.begin(),
            current.end(),
            current.begin(),
            [](wchar_t ch) {
                return static_cast<wchar_t>(
                    std::towlower(ch));
            });

        if (std::find(
                result.begin(),
                result.end(),
                current) == result.end()) {
            result.push_back(current);
        }

        current.clear();
    };

    for (const wchar_t ch : value) {
        if (std::iswalnum(ch) ||
            ch >= 0x4E00) {
            current.push_back(ch);
        } else {
            flush();
        }
    }

    flush();
    return result;
}

[[nodiscard]] bool ContainsAny(
    std::wstring_view value,
    std::initializer_list<
        std::wstring_view> needles) {
    const std::wstring lower =
        Lower(value);

    return std::any_of(
        needles.begin(),
        needles.end(),
        [&](std::wstring_view needle) {
            return lower.find(needle) !=
                std::wstring::npos;
        });
}

[[nodiscard]] bool
ContainsRoleToken(
    std::wstring_view value,
    std::initializer_list<
        std::wstring_view> needles) {

    const auto tokens =
        Tokens(value);

    for (const auto& token :
         tokens) {
        const std::wstring compactToken =
            Compact(token);

        for (const auto needle :
             needles) {
            const std::wstring compactNeedle =
                Compact(needle);

            if (compactNeedle.empty()) {
                continue;
            }

            if (compactToken ==
                compactNeedle) {
                return true;
            }

            // Windows shortcut/version-resource fields often append a year
            // directly to a role word (for example "Scheduler2026"). Treat
            // only a >=4 digit suffix as the same role word; do not use a
            // raw substring match that would turn "async" into "sync".
            if (compactToken.starts_with(
                    compactNeedle) &&
                compactToken.size() >=
                    compactNeedle.size() + 4) {
                const std::wstring_view suffix(
                    compactToken.data() +
                        compactNeedle.size(),
                    compactToken.size() -
                        compactNeedle.size());

                if (std::all_of(
                        suffix.begin(),
                        suffix.end(),
                        [](wchar_t ch) {
                            return
                                std::iswdigit(
                                    ch) != 0;
                        })) {
                    return true;
                }
            }
        }
    }

    return false;
}

[[nodiscard]] bool IsVersionToken(
    std::wstring_view token) {
    if (token.empty()) {
        return true;
    }

    if (token == L"x64" ||
        token == L"x86" ||
        token == L"arm64" ||
        token == L"64bit" ||
        token == L"32bit") {
        return true;
    }

    std::size_t index = 0;

    if (token.front() == L'v' &&
        token.size() > 1) {
        index = 1;
    }

    bool sawDigit = false;

    for (; index < token.size();
         ++index) {
        const wchar_t ch =
            token[index];

        if (std::iswdigit(ch)) {
            sawDigit = true;
            continue;
        }

        if (ch == L'.') {
            continue;
        }

        return false;
    }

    return sawDigit;
}

[[nodiscard]] bool
IsFamilyVersionToken(
    std::wstring_view token) {

    if (IsVersionToken(token)) {
        return true;
    }

    const std::wstring lower =
        Lower(token);

    const auto prefixedDigits =
        [&](std::wstring_view prefix) {
            if (!lower.starts_with(prefix) ||
                lower.size() <= prefix.size()) {
                return false;
            }

            return std::all_of(
                lower.begin() +
                    static_cast<
                        std::ptrdiff_t>(
                            prefix.size()),
                lower.end(),
                [](wchar_t ch) {
                    return
                        std::iswdigit(ch) != 0 ||
                        ch == L'.';
                });
        };

    return prefixedDigits(L"sp") ||
        prefixedDigits(L"r") ||
        prefixedDigits(L"version") ||
        prefixedDigits(L"release");
}

[[nodiscard]] std::wstring
NormalizeFamilyStem(
    std::wstring_view value) {

    const auto tokens =
        Tokens(value);

    std::wstring stem;

    for (const auto& token : tokens) {
        if (IsFamilyVersionToken(token)) {
            continue;
        }

        stem += Compact(token);
    }

    if (stem.empty()) {
        stem = Compact(value);
    }

    if (stem.empty()) {
        return stem;
    }

    std::size_t digitStart =
        stem.size();

    while (digitStart > 0 &&
           std::iswdigit(
               stem[digitStart - 1])) {
        --digitStart;
    }

    if (stem.size() - digitStart >= 4) {
        stem.resize(digitStart);
    }

    return stem;
}

[[nodiscard]] bool
IsGenericStartMenuFamily(
    std::wstring_view value) {

    const std::wstring compact =
        Compact(value);

    return compact.empty() ||
        compact == L"programs" ||
        compact == L"startmenu" ||
        compact == L"accessories" ||
        compact == L"windowstools" ||
        compact == L"administrativetools" ||
        compact == L"systemtools" ||
        compact == L"developertools" ||
        compact == L"程序" ||
        compact == L"附件" ||
        compact == L"管理工具" ||
        compact == L"系统工具";
}

[[nodiscard]] std::wstring
StartMenuSuiteName(
    const std::filesystem::path& folder) {

    if (folder.empty()) {
        return {};
    }

    bool afterPrograms = false;

    for (const auto& component : folder) {
        const std::wstring name =
            component.wstring();

        if (Compact(name) == L"programs") {
            afterPrograms = true;
            continue;
        }

        if (afterPrograms &&
            !IsGenericStartMenuFamily(name)) {
            return name;
        }
    }

    const std::wstring leaf =
        folder.filename().wstring();

    return IsGenericStartMenuFamily(leaf)
        ? std::wstring{}
        : leaf;
}

[[nodiscard]] std::vector<std::wstring>
FamilyIdentityNames(
    const LaunchEvidence& evidence) {

    std::vector<std::wstring> names;

    const std::wstring menuSuite =
        StartMenuSuiteName(
            evidence.startMenuFolder);

    if (!menuSuite.empty()) {
        // Windows' Start Menu suite folder is shared across the entries the
        // user perceives as one installed family. Once that evidence exists,
        // a helper EXE's private ProductName must not expand the shared family
        // and accidentally consume distinctive names such as Composer,
        // Renderer, Benchmark or Launcher.
        names.push_back(
            menuSuite);
        return names;
    }

    if (!evidence.executable
             .productName.empty()) {
        names.push_back(
            evidence.executable
                .productName);
    }

    return names;
}

[[nodiscard]] std::vector<std::wstring>
FamilyIdentityStems(
    const LaunchEvidence& evidence) {

    std::vector<std::wstring> stems;

    for (const auto& name :
         FamilyIdentityNames(evidence)) {
        const std::wstring stem =
            NormalizeFamilyStem(name);

        if (stem.empty() ||
            std::find(
                stems.begin(),
                stems.end(),
                stem) != stems.end()) {
            continue;
        }

        stems.push_back(stem);
    }

    std::sort(
        stems.begin(),
        stems.end(),
        [](const std::wstring& left,
           const std::wstring& right) {
            return left.size() >
                right.size();
        });

    return stems;
}

[[nodiscard]] std::wstring
NormalizedPath(
    const std::filesystem::path& path) {
    std::wstring value =
        path.lexically_normal().wstring();

    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](wchar_t ch) {
            return ch == L'/'
                ? L'\\'
                : static_cast<wchar_t>(
                      std::towlower(ch));
        });

    while (value.size() > 3 &&
           value.back() == L'\\') {
        value.pop_back();
    }

    return value;
}

[[nodiscard]] bool SameCompact(
    std::wstring_view left,
    std::wstring_view right) {
    const std::wstring a = Compact(left);
    const std::wstring b = Compact(right);

    return !a.empty() && a == b;
}

[[nodiscard]] bool StartsWithCompact(
    std::wstring_view value,
    std::wstring_view prefix) {
    const std::wstring v = Compact(value);
    const std::wstring p = Compact(prefix);

    return !p.empty() &&
        v.size() > p.size() &&
        v.starts_with(p);
}

void AddSignal(
    std::array<RoleScore, 20>& scores,
    ApplicationRole role,
    int strength,
    EvidenceField field,
    bool structuralHigh = false) {

    const auto index =
        static_cast<std::size_t>(role);

    if (index >= scores.size()) {
        return;
    }

    auto& score = scores[index];
    score.points += strength;
    score.fields |=
        static_cast<unsigned>(field);
    score.structuralHigh =
        score.structuralHigh ||
        structuralHigh;
}

[[nodiscard]] int RolePriority(
    ApplicationRole role) noexcept {
    switch (role) {
    case ApplicationRole::InternalComponent:
        return 170;
    case ApplicationRole::ServiceComponent:
        return 160;
    case ApplicationRole::BackgroundComponent:
        return 150;
    case ApplicationRole::Uninstaller:
        return 140;
    case ApplicationRole::Installer:
        return 135;
    case ApplicationRole::Updater:
        return 130;
    case ApplicationRole::Downloader:
        return 125;
    case ApplicationRole::RepairTool:
        return 120;
    case ApplicationRole::Documentation:
        return 115;
    case ApplicationRole::ProductInfo:
        return 110;
    case ApplicationRole::BenchmarkTool:
        return 100;
    case ApplicationRole::DiagnosticTool:
        return 95;
    case ApplicationRole::ConfigurationTool:
        return 90;
    case ApplicationRole::SuiteUtility:
        return 92;
    case ApplicationRole::SuiteSubordinate:
        return 85;
    case ApplicationRole::AlternateLaunch:
        return 80;
    case ApplicationRole::CompanionApplication:
        return 70;
    case ApplicationRole::PrimaryApplication:
        return 65;
    case ApplicationRole::UserTool:
        return 60;
    case ApplicationRole::Unknown:
        return 0;
    }

    return 0;
}

[[nodiscard]] RoleConfidence
ConfidenceFor(
    const RoleScore& score) noexcept {
    const int fieldCount =
        std::popcount(score.fields);

    if (score.structuralHigh ||
        (score.points >= 4 &&
         fieldCount >= 2)) {
        return RoleConfidence::High;
    }

    if (score.points >= 3) {
        return RoleConfidence::Medium;
    }

    return RoleConfidence::Low;
}

[[nodiscard]] int SemanticStrength(
    std::wstring_view value,
    EvidenceField field,
    int strength,
    std::initializer_list<
        std::wstring_view> strongPhrases) {

    if (field == EvidenceField::Title &&
        ContainsAny(
            value,
            strongPhrases)) {
        // A complete high-information role phrase must outrank the
        // generic "title == ProductName" primary-app relationship. It still
        // remains Medium confidence because it is only one evidence field.
        return std::max(
            strength,
            5);
    }

    return strength;
}

[[nodiscard]] bool
HasContextualSuiteUtilityPhrase(
    std::wstring_view value) {

    return ContainsAny(
        value,
        {L"network monitor",
         L"network monitoring",
         L"license manager",
         L"licensing manager",
         L"license administrator",
         L"licensing administrator",
         L"license utility",
         L"licensing utility",
         L"network license manager",
         L"network license administrator",
         L"network license utility",
         L"service manager",
         L"service administrator",
         L"service console",
         L"service utility",
         L"网络监视器",
         L"网络监控",
         L"许可证管理器",
         L"许可管理器",
         L"许可证管理员",
         L"许可管理员",
         L"许可证工具",
         L"许可工具",
         L"服务管理器",
         L"服务管理员",
         L"服务控制台",
         L"服务工具"});
}

[[nodiscard]] bool
HasUserFacingServiceUtilityPhrase(
    std::wstring_view value) {

    return ContainsAny(
        value,
        {L"service manager",
         L"service administrator",
         L"service console",
         L"service utility",
         L"服务管理器",
         L"服务管理员",
         L"服务控制台",
         L"服务工具"});
}

void AddTextSignals(
    std::array<RoleScore, 20>& scores,
    std::wstring_view value,
    EvidenceField field,
    int strength) {

    if (value.empty()) {
        return;
    }

    if (ContainsAny(
            value,
            {L"uninstaller",
             L"uninstall",
             L"卸载"})) {
        AddSignal(
            scores,
            ApplicationRole::Uninstaller,
            SemanticStrength(
                value,
                field,
                strength,
                {L"uninstaller",
                 L"uninstall wizard",
                 L"卸载程序",
                 L"卸载向导"}),
            field);
    }

    if (ContainsAny(
            value,
            {L"installer",
             L"installation",
             L"setup",
             L"安装程序",
             L"安装向导"})) {
        AddSignal(
            scores,
            ApplicationRole::Installer,
            SemanticStrength(
                value,
                field,
                strength,
                {L"installer",
                 L"installation wizard",
                 L"setup wizard",
                 L"安装程序",
                 L"安装向导"}),
            field);
    }

    if (ContainsAny(
            value,
            {L"updater",
             L"update manager",
             L"update helper",
             L"更新程序",
             L"升级程序"})) {
        AddSignal(
            scores,
            ApplicationRole::Updater,
            SemanticStrength(
                value,
                field,
                strength,
                {L"updater",
                 L"update manager",
                 L"update helper",
                 L"更新程序",
                 L"升级程序"}),
            field);
    }

    if (ContainsAny(
            value,
            {L"downloader",
             L"download manager",
             L"background download",
             L"下载程序",
             L"下载管理",
             L"后台下载"})) {
        AddSignal(
            scores,
            ApplicationRole::Downloader,
            SemanticStrength(
                value,
                field,
                strength,
                {L"downloader",
                 L"download manager",
                 L"background download",
                 L"下载程序",
                 L"下载管理",
                 L"后台下载"}),
            field);
    }

    if (ContainsAny(
            value,
            {L"repair",
             L"修复"})) {
        AddSignal(
            scores,
            ApplicationRole::RepairTool,
            SemanticStrength(
                value,
                field,
                strength,
                {L"repair tool",
                 L"repair utility",
                 L"repair wizard",
                 L"修复工具",
                 L"修复向导"}),
            field);
    }

    if (ContainsAny(
            value,
            {L"settings",
             L"configuration",
             L"configurator",
             L"preferences",
             L"设置",
             L"配置"})) {
        AddSignal(
            scores,
            ApplicationRole::ConfigurationTool,
            SemanticStrength(
                value,
                field,
                strength,
                {L"settings wizard",
                 L"configuration wizard",
                 L"settings utility",
                 L"configuration utility",
                 L"设置向导",
                 L"配置向导",
                 L"设置工具",
                 L"配置工具"}),
            field);
    }

    if (ContainsAny(
            value,
            {L"diagnostic",
             L"diagnostics",
             L"system diagnostics",
             L"diagnostic assistant",
             L"troubleshoot",
             L"troubleshooter",
             L"health check",
             L"problem report",
             L"problem reporter",
             L"problem reporting",
             L"support tool",
             L"support utility",
             L"support assistant",
             L"recovery tool",
             L"recovery utility",
             L"诊断",
             L"故障排除",
             L"问题报告",
             L"支持工具",
             L"恢复工具"})) {
        AddSignal(
            scores,
            ApplicationRole::DiagnosticTool,
            SemanticStrength(
                value,
                field,
                strength,
                {L"diagnostics",
                 L"system diagnostics",
                 L"diagnostic tool",
                 L"diagnostic utility",
                 L"diagnostic assistant",
                 L"troubleshooter",
                 L"health check",
                 L"problem report",
                 L"problem reporter",
                 L"problem reporting",
                 L"support tool",
                 L"support utility",
                 L"support assistant",
                 L"recovery tool",
                 L"recovery utility",
                 L"诊断工具",
                 L"诊断程序",
                 L"故障排除",
                 L"问题报告",
                 L"支持工具",
                 L"恢复工具"}),
            field);
    }

    if (ContainsAny(
            value,
            {L"benchmark",
             L"performance test",
             L"performance benchmark",
             L"stress test",
             L"性能测试",
             L"基准测试"})) {
        AddSignal(
            scores,
            ApplicationRole::BenchmarkTool,
            SemanticStrength(
                value,
                field,
                strength,
                {L"performance test",
                 L"performance benchmark",
                 L"benchmark test",
                 L"stress test",
                 L"性能测试",
                 L"基准测试"}),
            field);
    }

    const bool strongSuiteUtility =
        ContainsAny(
            value,
            {L"task scheduler",
             L"job scheduler",
             L"sync manager",
             L"synchronization manager",
             L"synchronization tool",
             L"sync utility",
             L"automation tool",
             L"automation utility",
             L"maintenance console",
             L"maintenance tool",
             L"maintenance utility",
             L"任务计划程序",
             L"任务计划工具",
             L"任务调度器",
             L"任务调度工具",
             L"同步管理器",
             L"同步工具",
             L"自动化工具",
             L"维护控制台",
             L"维护工具"});

    const bool weakSuiteUtility =
        ContainsRoleToken(
            value,
            {L"scheduler",
             L"sync",
             L"synchronizer",
             L"synchronization",
             L"automation",
             L"maintenance"}) ||
        HasContextualSuiteUtilityPhrase(
            value) ||
        ContainsAny(
            value,
            {L"调度",
             L"同步",
             L"自动化",
             L"维护"});

    if (strongSuiteUtility ||
        weakSuiteUtility) {
        AddSignal(
            scores,
            ApplicationRole::
                SuiteUtility,
            strongSuiteUtility
                ? SemanticStrength(
                      value,
                      field,
                      strength,
                      {L"task scheduler",
                       L"job scheduler",
                       L"sync manager",
                       L"synchronization manager",
                       L"synchronization tool",
                       L"sync utility",
                       L"automation tool",
                       L"automation utility",
                       L"maintenance console",
                       L"maintenance tool",
                       L"maintenance utility",
                       L"任务计划程序",
                       L"任务计划工具",
                       L"任务调度器",
                       L"任务调度工具",
                       L"同步管理器",
                       L"同步工具",
                       L"自动化工具",
                       L"维护控制台",
                       L"维护工具"})
                : 1,
            field);
    }

    if (ContainsAny(
            value,
            {L"documentation",
             L"manual",
             L"user guide",
             L"release notes",
             L"readme",
             L"online help",
             L"文档",
             L"手册",
             L"帮助"})) {
        AddSignal(
            scores,
            ApplicationRole::Documentation,
            strength,
            field);
    }

    if (ContainsAny(
            value,
            {L"quick launch",
             L"quick start",
             L"safe mode",
             L"no plugins",
             L"without plugins",
             L"disable plugins",
             L"64-bit launcher",
             L"32-bit launcher",
             L"快速启动",
             L"快速开始",
             L"安全模式",
             L"无插件",
             L"禁用插件"})) {
        AddSignal(
            scores,
            ApplicationRole::
                AlternateLaunch,
            std::min(
                strength,
                2),
            field);
    }

    if (ContainsAny(
            value,
            {L"background",
             L"broker",
             L"helper",
             L"host",
             L"后台"})) {
        AddSignal(
            scores,
            ApplicationRole::BackgroundComponent,
            std::max(1, strength - 1),
            field);
    }

    if (!HasUserFacingServiceUtilityPhrase(
            value) &&
        ContainsAny(
            value,
            {L" service",
             L"service ",
             L"service.exe",
             L"daemon",
             L"服务"})) {
        AddSignal(
            scores,
            ApplicationRole::ServiceComponent,
            strength,
            field);
    }
}

void AppendDistinctivePhraseTokens(
    std::wstring_view value,
    std::vector<std::wstring>& tokens) {

    const std::wstring lower =
        Lower(value);

    for (const std::wstring_view phrase : {
             L"settings",
             L"configuration",
             L"configurator",
             L"preferences",
             L"settings wizard",
             L"configuration wizard",
             L"设置",
             L"配置",
             L"设置向导",
             L"配置向导",
             L"diagnostic",
             L"diagnostics",
             L"system diagnostics",
             L"diagnostic assistant",
             L"problem report",
             L"problem reporter",
             L"problem reporting",
             L"support tool",
             L"support utility",
             L"support assistant",
             L"recovery tool",
             L"recovery utility",
             L"troubleshoot",
             L"troubleshooter",
             L"health check",
             L"诊断",
             L"故障排除",
             L"问题报告",
             L"支持工具",
             L"恢复工具",
             L"benchmark",
             L"performance test",
             L"performance benchmark",
             L"stress test",
             L"性能测试",
             L"基准测试",
             L"updater",
             L"update manager",
             L"update helper",
             L"更新程序",
             L"升级程序",
             L"downloader",
             L"download manager",
             L"background download",
             L"下载程序",
             L"下载管理",
             L"后台下载",
             L"repair",
             L"repair tool",
             L"repair wizard",
             L"修复",
             L"修复工具",
             L"修复向导",
             L"installer",
             L"installation wizard",
             L"setup wizard",
             L"安装程序",
             L"安装向导",
             L"uninstall",
             L"uninstaller",
             L"uninstall wizard",
             L"卸载",
             L"卸载程序",
             L"卸载向导",
             L"documentation",
             L"manual",
             L"user guide",
             L"release notes",
             L"readme",
             L"online help",
             L"文档",
             L"手册",
             L"帮助",
             L"about",
             L"关于",
             L"background",
             L"broker",
             L"后台",
             L"service",
             L"daemon",
             L"服务",
             L"task scheduler",
             L"job scheduler",
             L"sync manager",
             L"synchronization manager",
             L"synchronization tool",
             L"sync utility",
             L"automation tool",
             L"automation utility",
             L"maintenance console",
             L"maintenance tool",
             L"maintenance utility",
             L"network monitor",
             L"network monitoring",
             L"license manager",
             L"licensing manager",
             L"license administrator",
             L"licensing administrator",
             L"license utility",
             L"licensing utility",
             L"network license manager",
             L"network license administrator",
             L"network license utility",
             L"service manager",
             L"service administrator",
             L"service console",
             L"service utility",
             L"网络监视器",
             L"网络监控",
             L"许可证管理器",
             L"许可管理器",
             L"许可证管理员",
             L"许可管理员",
             L"许可证工具",
             L"许可工具",
             L"服务管理器",
             L"服务管理员",
             L"服务控制台",
             L"服务工具",
             L"任务计划程序",
             L"任务计划工具",
             L"任务调度器",
             L"任务调度工具",
             L"同步管理器",
             L"同步工具",
             L"自动化工具",
             L"维护控制台",
             L"维护工具",
             L"quick launch",
             L"quick start",
             L"safe mode",
             L"no plugins",
             L"without plugins",
             L"disable plugins",
             L"64-bit launcher",
             L"32-bit launcher",
             L"快速启动",
             L"快速开始",
             L"安全模式",
             L"无插件",
             L"禁用插件"}) {

        if (lower.find(phrase) ==
            std::wstring::npos) {
            continue;
        }

        const std::wstring token(
            phrase);

        if (std::find(
                tokens.begin(),
                tokens.end(),
                token) ==
            tokens.end()) {
            tokens.push_back(
                token);
        }
    }

    const auto appendIntent =
        [&](std::wstring token) {
            if (token.empty() ||
                std::find(
                    tokens.begin(),
                    tokens.end(),
                    token) !=
                    tokens.end()) {
                return;
            }

            tokens.push_back(
                std::move(token));
        };

    if (ContainsAny(
            value,
            {L"network monitor",
             L"network monitoring"})) {
        appendIntent(L"monitor");
    }

    if (ContainsAny(
            value,
            {L"网络监视器"})) {
        appendIntent(L"监视器");
    }

    if (ContainsAny(
            value,
            {L"网络监控"})) {
        appendIntent(L"监控");
    }

    for (const std::wstring_view word : {
             L"scheduler",
             L"sync",
             L"synchronizer",
             L"synchronization",
             L"automation",
             L"maintenance"}) {
        if (!ContainsRoleToken(
                value,
                {word})) {
            continue;
        }

        const std::wstring token(word);

        if (std::find(
                tokens.begin(),
                tokens.end(),
                token) ==
            tokens.end()) {
            tokens.push_back(token);
        }
    }

    for (const std::wstring_view phrase : {
             L"调度",
             L"同步",
             L"自动化",
             L"维护"}) {
        if (lower.find(phrase) ==
            std::wstring::npos) {
            continue;
        }

        const std::wstring token(phrase);

        if (std::find(
                tokens.begin(),
                tokens.end(),
                token) ==
            tokens.end()) {
            tokens.push_back(token);
        }
    }
}

struct CatalogGroupParts {
    std::wstring family;
    std::wstring locationKind;
    std::wstring location;

    [[nodiscard]] bool Valid()
        const noexcept {
        return !family.empty() &&
            !locationKind.empty() &&
            !location.empty();
    }
};

[[nodiscard]] CatalogGroupParts
ParseCatalogGroupKeyParts(
    std::wstring_view key) {

    constexpr std::wstring_view
        kFamilyPrefix = L"family:";
    constexpr std::wstring_view
        kLegacyProductPrefix = L"product:";
    constexpr std::wstring_view
        kRootPrefix = L"root:";
    constexpr std::wstring_view
        kMenuPrefix = L"menu:";

    CatalogGroupParts parts;

    std::size_t prefixSize = 0;

    if (key.starts_with(
            kFamilyPrefix)) {
        prefixSize =
            kFamilyPrefix.size();
    } else if (key.starts_with(
                   kLegacyProductPrefix)) {
        // Parsing the old spelling is harmless for transient/test Commands;
        // Provider Cache schema upgrades still rebuild generated state.
        prefixSize =
            kLegacyProductPrefix.size();
    } else {
        return parts;
    }

    const std::size_t separator =
        key.find(
            L'|',
            prefixSize);

    if (separator ==
        std::wstring_view::npos) {
        return parts;
    }

    parts.family =
        std::wstring(
            key.substr(
                prefixSize,
                separator -
                    prefixSize));

    const std::wstring_view tail =
        key.substr(separator + 1);

    if (tail.starts_with(
            kRootPrefix)) {
        parts.locationKind = L"root";
        parts.location =
            std::wstring(
                tail.substr(
                    kRootPrefix.size()));
    } else if (tail.starts_with(
                   kMenuPrefix)) {
        parts.locationKind = L"menu";
        parts.location =
            std::wstring(
                tail.substr(
                    kMenuPrefix.size()));
    }

    return parts;
}

[[nodiscard]] std::wstring
ParentCatalogLocation(
    std::wstring_view value) {

    const std::size_t slash =
        value.find_last_of(L'\\');

    if (slash ==
            std::wstring_view::npos ||
        slash <= 2) {
        return {};
    }

    return std::wstring(
        value.substr(0, slash));
}

[[nodiscard]] bool
IsCatalogLocationAncestor(
    std::wstring_view parent,
    std::wstring_view child) {

    return parent.size() <
            child.size() &&
        child.starts_with(parent) &&
        child[parent.size()] == L'\\';
}

[[nodiscard]] bool
RelatedCatalogLocation(
    std::wstring_view left,
    std::wstring_view right) {

    if (left == right) {
        return true;
    }

    if (IsCatalogLocationAncestor(
            left,
            right) ||
        IsCatalogLocationAncestor(
            right,
            left)) {
        return true;
    }

    const std::wstring leftParent =
        ParentCatalogLocation(left);
    const std::wstring rightParent =
        ParentCatalogLocation(right);

    return !leftParent.empty() &&
        leftParent == rightParent;
}

[[nodiscard]] bool
SameCatalogContext(
    const Command& left,
    const Command& right) {

    if (left.catalogGroupKey.empty() ||
        right.catalogGroupKey.empty()) {
        return false;
    }

    if (left.catalogGroupKey ==
        right.catalogGroupKey) {
        return true;
    }

    const CatalogGroupParts a =
        ParseCatalogGroupKeyParts(
            left.catalogGroupKey);
    const CatalogGroupParts b =
        ParseCatalogGroupKeyParts(
            right.catalogGroupKey);

    return a.Valid() &&
        b.Valid() &&
        a.family == b.family &&
        a.locationKind ==
            b.locationKind &&
        RelatedCatalogLocation(
            a.location,
            b.location);
}

[[nodiscard]] std::wstring
CatalogFamilyKey(
    const Command& command) {

    return ParseCatalogGroupKeyParts(
               command.catalogGroupKey)
        .family;
}

[[nodiscard]] bool
IsContextPromotableRole(
    ApplicationRole role) noexcept {

    switch (role) {
    case ApplicationRole::ConfigurationTool:
    case ApplicationRole::DiagnosticTool:
    case ApplicationRole::BenchmarkTool:
    case ApplicationRole::SuiteUtility:
    case ApplicationRole::Installer:
    case ApplicationRole::Uninstaller:
    case ApplicationRole::Updater:
    case ApplicationRole::Downloader:
    case ApplicationRole::RepairTool:
        return true;

    case ApplicationRole::Unknown:
    case ApplicationRole::PrimaryApplication:
    case ApplicationRole::CompanionApplication:
    case ApplicationRole::SuiteSubordinate:
    case ApplicationRole::AlternateLaunch:
    case ApplicationRole::UserTool:
    case ApplicationRole::Documentation:
    case ApplicationRole::ProductInfo:
    case ApplicationRole::BackgroundComponent:
    case ApplicationRole::ServiceComponent:
    case ApplicationRole::InternalComponent:
        return false;
    }

    return false;
}

[[nodiscard]] bool
LooksLikeAlternateLaunch(
    std::wstring_view title) {

    return ContainsAny(
        title,
        {L"quick launch",
         L"quick start",
         L"safe mode",
         L"no plugins",
         L"without plugins",
         L"disable plugins",
         L"64-bit launcher",
         L"32-bit launcher",
         L"快速启动",
         L"快速开始",
         L"安全模式",
         L"无插件",
         L"禁用插件"});
}

[[nodiscard]] std::wstring
BaseCanonicalIdentity(
    const Command& command) {

    if (!command.canonicalIdentity
             .empty()) {
        const std::size_t args =
            command.canonicalIdentity
                .find(L"|args:");

        return command.canonicalIdentity
            .substr(
                0,
                args);
    }

    return NormalizedPath(
        std::filesystem::path(
            command.target));
}

[[nodiscard]] std::wstring
AlternateBaseTitle(
    std::wstring_view title) {

    std::wstring value =
        Lower(title);

    for (const std::wstring_view phrase : {
             L"quick launch",
             L"quick start",
             L"safe mode",
             L"no plugins",
             L"without plugins",
             L"disable plugins",
             L"64-bit launcher",
             L"32-bit launcher",
             L"快速启动",
             L"快速开始",
             L"安全模式",
             L"无插件",
             L"禁用插件"}) {
        std::size_t position = 0;

        while ((position =
                    value.find(
                        phrase,
                        position)) !=
               std::wstring::npos) {
            value.erase(
                position,
                phrase.size());
        }
    }

    return NormalizeFamilyStem(
        value);
}

struct AlternateRelation {
    bool related{false};
    bool sameTarget{false};
    bool sameFamily{false};
    bool sameBaseTitle{false};
};

[[nodiscard]] bool
IsSuiteTopologyIdentityRole(
    const Command& command) noexcept {

    return
        command.catalogVisibility ==
            CatalogVisibility::Normal &&
        command.applicationRole ==
            ApplicationRole::
                CompanionApplication &&
        command.roleConfidence !=
            RoleConfidence::Low;
}

[[nodiscard]] std::vector<std::wstring>
NormalizedDistinctiveTokens(
    const Command& command) {

    std::vector<std::wstring> result;

    for (const auto& raw :
         command.distinctiveTokens) {
        std::wstring token =
            Compact(raw);

        if (token.empty()) {
            continue;
        }

        result.push_back(
            std::move(token));
    }

    return result;
}

[[nodiscard]] std::wstring
JoinTokens(
    const std::vector<std::wstring>&
        tokens) {

    std::wstring result;

    for (const auto& token : tokens) {
        result += token;
    }

    return result;
}

[[nodiscard]] std::wstring
SuiteTitleIdentity(
    const Command& command) {

    const std::wstring title =
        NormalizeFamilyStem(
            command.title);
    const std::wstring family =
        CatalogFamilyKey(
            command);

    if (!family.empty() &&
        title.size() >
            family.size() &&
        title.starts_with(
            family)) {
        return title.substr(
            family.size());
    }

    // Metadata-derived distinctive tokens can legitimately change once a
    // shortcut resolves to its real executable. Fall back to them only when
    // the catalog-family prefix cannot be removed from the display title.
    return JoinTokens(
        NormalizedDistinctiveTokens(
            command));
}

[[nodiscard]] std::wstring
CanonicalTargetPath(
    const Command& command) {

    std::wstring identity =
        BaseCanonicalIdentity(
            command);

    constexpr std::wstring_view
        kFilePrefix = L"file:";

    if (!identity.starts_with(
            kFilePrefix)) {
        return {};
    }

    identity.erase(
        0,
        kFilePrefix.size());

    if (identity.empty()) {
        return {};
    }

    std::transform(
        identity.begin(),
        identity.end(),
        identity.begin(),
        [](wchar_t ch) {
            if (ch == L'/') {
                return L'\\';
            }

            return static_cast<wchar_t>(
                std::towlower(ch));
        });

    while (identity.size() > 3 &&
           identity.back() == L'\\') {
        identity.pop_back();
    }

    return identity;
}

[[nodiscard]] std::wstring_view
PathLeaf(
    std::wstring_view path) {

    const std::size_t separator =
        path.find_last_of(
            L"\\/");

    if (separator ==
        std::wstring_view::npos) {
        return path;
    }

    return path.substr(
        separator + 1);
}

[[nodiscard]] std::wstring
PathParent(
    std::wstring_view path) {

    while (path.size() > 3 &&
           (path.back() == L'\\' ||
            path.back() == L'/')) {
        path.remove_suffix(1);
    }

    const std::size_t separator =
        path.find_last_of(
            L"\\/");

    if (separator ==
        std::wstring_view::npos) {
        return {};
    }

    if (separator == 2 &&
        path.size() >= 3 &&
        path[1] == L':') {
        return std::wstring(
            path.substr(
                0,
                3));
    }

    return std::wstring(
        path.substr(
            0,
            separator));
}

[[nodiscard]] std::wstring
PathStem(
    std::wstring_view path) {

    std::wstring_view leaf =
        PathLeaf(path);

    const std::size_t extension =
        leaf.find_last_of(L'.');

    if (extension !=
            std::wstring_view::npos &&
        extension > 0) {
        leaf =
            leaf.substr(
                0,
                extension);
    }

    return Compact(leaf);
}

[[nodiscard]] bool
StrictIdentityExtension(
    std::wstring_view anchor,
    std::wstring_view candidate) {

    return
        anchor.size() >= 3 &&
        candidate.size() >
            anchor.size() &&
        candidate.starts_with(
            anchor) &&
        candidate.size() -
                anchor.size() >=
            2;
}

[[nodiscard]] bool
HasExecutableStemTopology(
    std::wstring_view candidatePath,
    std::wstring_view anchorPath) {

    const std::wstring candidateStem =
        PathStem(
            candidatePath);
    const std::wstring anchorStem =
        PathStem(
            anchorPath);

    return StrictIdentityExtension(
        anchorStem,
        candidateStem);
}

[[nodiscard]] bool
HasDirectorySegmentTopology(
    std::wstring_view candidatePath,
    std::wstring_view anchorPath) {

    std::wstring candidateDirectory =
        PathParent(
            candidatePath);
    std::wstring anchorDirectory =
        PathParent(
            anchorPath);

    // Canonical launch identities are Windows paths even when core tests run
    // on a non-Windows host. Parse their separators lexically instead of
    // delegating to std::filesystem, whose native separator rules would treat
    // backslashes as ordinary characters on POSIX.
    //
    // Walk only a few equal trailing directories (for example "bin") before
    // comparing the first differing sibling segment. This catches installed
    // layouts such as "Visualize" -> "Visualize Boost" without turning a
    // distant common ancestor into suite-parent evidence.
    for (int depth = 0;
         depth < 4;
         ++depth) {
        if (candidateDirectory.empty() ||
            anchorDirectory.empty()) {
            return false;
        }

        const std::wstring candidateLeaf =
            Compact(
                PathLeaf(
                    candidateDirectory));
        const std::wstring anchorLeaf =
            Compact(
                PathLeaf(
                    anchorDirectory));

        if (StrictIdentityExtension(
                anchorLeaf,
                candidateLeaf)) {
            const std::wstring
                candidateParent =
                    PathParent(
                        candidateDirectory);
            const std::wstring
                anchorParent =
                    PathParent(
                        anchorDirectory);

            return
                !anchorParent.empty() &&
                candidateParent ==
                    anchorParent;
        }

        if (candidateLeaf !=
            anchorLeaf) {
            return false;
        }

        candidateDirectory =
            PathParent(
                candidateDirectory);
        anchorDirectory =
            PathParent(
                anchorDirectory);
    }

    return false;
}

[[nodiscard]] bool
HasSuiteTargetTopology(
    const Command& candidate,
    const Command& anchor) {

    const auto candidatePath =
        CanonicalTargetPath(
            candidate);
    const auto anchorPath =
        CanonicalTargetPath(
            anchor);

    if (candidatePath.empty() ||
        anchorPath.empty()) {
        return false;
    }

    return
        HasExecutableStemTopology(
            candidatePath,
            anchorPath) ||
        HasDirectorySegmentTopology(
            candidatePath,
            anchorPath);
}

[[nodiscard]] std::vector<std::wstring>
SuiteTopologyDelta(
    const Command& candidate,
    const Command& anchor) {

    if (!SameCatalogContext(
            candidate,
            anchor) ||
        BaseCanonicalIdentity(
            candidate) ==
            BaseCanonicalIdentity(
                anchor)) {
        return {};
    }

    const std::wstring candidateIdentity =
        SuiteTitleIdentity(
            candidate);
    const std::wstring anchorIdentity =
        SuiteTitleIdentity(
            anchor);

    if (!StrictIdentityExtension(
            anchorIdentity,
            candidateIdentity)) {
        return {};
    }

    const std::wstring titleDelta =
        candidateIdentity.substr(
            anchorIdentity.size());

    if (!HasSuiteTargetTopology(
            candidate,
            anchor)) {
        return {};
    }

    const auto candidateTokens =
        NormalizedDistinctiveTokens(
            candidate);
    const auto anchorTokens =
        NormalizedDistinctiveTokens(
            anchor);

    bool tokenPrefix =
        !anchorTokens.empty() &&
        anchorTokens.size() <
            candidateTokens.size();

    if (tokenPrefix) {
        for (std::size_t index = 0;
             index < anchorTokens.size();
             ++index) {
            if (anchorTokens[index] !=
                candidateTokens[index]) {
                tokenPrefix = false;
                break;
            }
        }
    }

    if (tokenPrefix) {
        return std::vector<std::wstring>(
            candidateTokens.begin() +
                static_cast<std::ptrdiff_t>(
                    anchorTokens.size()),
            candidateTokens.end());
    }

    // The display-title delta is deliberately independent of executable
    // metadata. After an advertised shortcut is resolved, ProductName and
    // FileDescription can change the family-stripped metadata tokens while
    // the user-visible child intent (for example "player" or "boost") stays
    // stable.
    return {titleDelta};
}

struct SuiteTopologyDecision {
    Command* candidate{nullptr};
    std::vector<std::wstring>
        intentTokens;
    std::size_t anchorIdentityLength{0};
};

[[nodiscard]] AlternateRelation
AlternateRelationToPrimary(
    const Command& candidate,
    const Command& primary) {

    AlternateRelation relation;

    const std::wstring candidateTarget =
        BaseCanonicalIdentity(
            candidate);
    const std::wstring primaryTarget =
        BaseCanonicalIdentity(
            primary);

    relation.sameTarget =
        !candidateTarget.empty() &&
        candidateTarget ==
            primaryTarget;

    const std::wstring candidateFamily =
        CatalogFamilyKey(
            candidate);
    const std::wstring primaryFamily =
        CatalogFamilyKey(
            primary);

    relation.sameFamily =
        !candidateFamily.empty() &&
        candidateFamily ==
            primaryFamily;

    const std::wstring alternateBase =
        AlternateBaseTitle(
            candidate.title);
    const std::wstring primaryBase =
        NormalizeFamilyStem(
            primary.title);

    relation.sameBaseTitle =
        !alternateBase.empty() &&
        alternateBase ==
            primaryBase;

    relation.related =
        relation.sameTarget ||
        relation.sameFamily ||
        relation.sameBaseTitle;

    return relation;
}



} // namespace

const char* ApplicationRoleName(
    ApplicationRole role) noexcept {
    switch (role) {
    case ApplicationRole::Unknown:
        return "unknown";
    case ApplicationRole::PrimaryApplication:
        return "primary-application";
    case ApplicationRole::CompanionApplication:
        return "companion-application";
    case ApplicationRole::SuiteSubordinate:
        return "suite-subordinate";
    case ApplicationRole::AlternateLaunch:
        return "alternate-launch";
    case ApplicationRole::SuiteUtility:
        return "suite-utility";
    case ApplicationRole::UserTool:
        return "user-tool";
    case ApplicationRole::ConfigurationTool:
        return "configuration-tool";
    case ApplicationRole::DiagnosticTool:
        return "diagnostic-tool";
    case ApplicationRole::BenchmarkTool:
        return "benchmark-tool";
    case ApplicationRole::Installer:
        return "installer";
    case ApplicationRole::Uninstaller:
        return "uninstaller";
    case ApplicationRole::Updater:
        return "updater";
    case ApplicationRole::Downloader:
        return "downloader";
    case ApplicationRole::RepairTool:
        return "repair-tool";
    case ApplicationRole::Documentation:
        return "documentation";
    case ApplicationRole::ProductInfo:
        return "product-info";
    case ApplicationRole::BackgroundComponent:
        return "background-component";
    case ApplicationRole::ServiceComponent:
        return "service-component";
    case ApplicationRole::InternalComponent:
        return "internal-component";
    }

    return "unknown";
}

ApplicationRole ParseApplicationRole(
    std::string_view value,
    ApplicationRole fallback) noexcept {
    static constexpr std::array<
        std::pair<std::string_view,
                  ApplicationRole>,
        20>
        values{{
            {"unknown", ApplicationRole::Unknown},
            {"primary-application", ApplicationRole::PrimaryApplication},
            {"companion-application", ApplicationRole::CompanionApplication},
            {"suite-subordinate", ApplicationRole::SuiteSubordinate},
            {"alternate-launch", ApplicationRole::AlternateLaunch},
            {"suite-utility", ApplicationRole::SuiteUtility},
            {"user-tool", ApplicationRole::UserTool},
            {"configuration-tool", ApplicationRole::ConfigurationTool},
            {"diagnostic-tool", ApplicationRole::DiagnosticTool},
            {"benchmark-tool", ApplicationRole::BenchmarkTool},
            {"installer", ApplicationRole::Installer},
            {"uninstaller", ApplicationRole::Uninstaller},
            {"updater", ApplicationRole::Updater},
            {"downloader", ApplicationRole::Downloader},
            {"repair-tool", ApplicationRole::RepairTool},
            {"documentation", ApplicationRole::Documentation},
            {"product-info", ApplicationRole::ProductInfo},
            {"background-component", ApplicationRole::BackgroundComponent},
            {"service-component", ApplicationRole::ServiceComponent},
            {"internal-component", ApplicationRole::InternalComponent},
        }};

    for (const auto& [name, role] :
         values) {
        if (value == name) {
            return role;
        }
    }

    return fallback;
}

const char* RoleConfidenceName(
    RoleConfidence confidence) noexcept {
    switch (confidence) {
    case RoleConfidence::Low:
        return "low";
    case RoleConfidence::Medium:
        return "medium";
    case RoleConfidence::High:
        return "high";
    }

    return "low";
}

RoleConfidence ParseRoleConfidence(
    std::string_view value,
    RoleConfidence fallback) noexcept {
    if (value == "low") {
        return RoleConfidence::Low;
    }
    if (value == "medium") {
        return RoleConfidence::Medium;
    }
    if (value == "high") {
        return RoleConfidence::High;
    }
    return fallback;
}

const char* CatalogVisibilityName(
    CatalogVisibility visibility) noexcept {
    switch (visibility) {
    case CatalogVisibility::Normal:
        return "normal";
    case CatalogVisibility::StrongMatchOnly:
        return "strong-match-only";
    case CatalogVisibility::Hidden:
        return "hidden";
    }

    return "normal";
}

CatalogVisibility ParseCatalogVisibility(
    std::string_view value,
    CatalogVisibility fallback) noexcept {
    if (value == "normal") {
        return CatalogVisibility::Normal;
    }
    if (value == "strong-match-only") {
        return CatalogVisibility::StrongMatchOnly;
    }
    if (value == "hidden") {
        return CatalogVisibility::Hidden;
    }
    return fallback;
}

CatalogVisibility CatalogVisibilityForRole(
    ApplicationRole role,
    RoleConfidence confidence) noexcept {

    if (role == ApplicationRole::Unknown ||
        role == ApplicationRole::PrimaryApplication ||
        role == ApplicationRole::CompanionApplication ||
        role == ApplicationRole::UserTool) {
        return CatalogVisibility::Normal;
    }

    if (role == ApplicationRole::ProductInfo) {
        return confidence ==
                RoleConfidence::Low
            ? CatalogVisibility::
                  StrongMatchOnly
            : CatalogVisibility::Hidden;
    }

    if (confidence == RoleConfidence::Low) {
        return CatalogVisibility::Normal;
    }

    if (confidence == RoleConfidence::Medium) {
        return CatalogVisibility::
            StrongMatchOnly;
    }

    switch (role) {
    case ApplicationRole::SuiteSubordinate:
    case ApplicationRole::AlternateLaunch:
    case ApplicationRole::SuiteUtility:
    case ApplicationRole::ConfigurationTool:
    case ApplicationRole::DiagnosticTool:
    case ApplicationRole::BenchmarkTool:
    case ApplicationRole::Downloader:
    case ApplicationRole::RepairTool:
        return CatalogVisibility::
            StrongMatchOnly;

    case ApplicationRole::Installer:
    case ApplicationRole::Uninstaller:
    case ApplicationRole::Updater:
    case ApplicationRole::Documentation:
    case ApplicationRole::ProductInfo:
    case ApplicationRole::BackgroundComponent:
    case ApplicationRole::ServiceComponent:
    case ApplicationRole::InternalComponent:
        return CatalogVisibility::Hidden;

    case ApplicationRole::Unknown:
    case ApplicationRole::PrimaryApplication:
    case ApplicationRole::CompanionApplication:
    case ApplicationRole::UserTool:
        return CatalogVisibility::Normal;
    }

    return CatalogVisibility::Normal;
}

std::wstring BuildCatalogGroupKey(
    const LaunchEvidence& evidence) {

    const std::wstring menuSuite =
        StartMenuSuiteName(
            evidence.startMenuFolder);
    const std::wstring menuFamily =
        NormalizeFamilyStem(
            menuSuite);
    const std::wstring menu =
        NormalizedPath(
            evidence.startMenuFolder);

    // For Start Menu entries, Windows' own suite folder is the most stable
    // cross-executable context. Do not let a helper EXE's private ProductName
    // split one installed suite into unrelated groups.
    if (!menuFamily.empty() &&
        !menu.empty()) {
        return L"family:" +
            menuFamily +
            L"|menu:" +
            menu;
    }

    const std::wstring productFamily =
        NormalizeFamilyStem(
            evidence.executable
                .productName);

    if (productFamily.empty()) {
        return {};
    }

    const std::wstring root =
        NormalizedPath(
            evidence.installRootHint);

    if (!root.empty()) {
        return L"family:" +
            productFamily +
            L"|root:" +
            root;
    }

    return {};
}

std::vector<std::wstring>
BuildDistinctiveTokens(
    const LaunchEvidence& evidence) {

    const auto titleTokens =
        Tokens(
            evidence.displayTitle);

    const auto identityNames =
        FamilyIdentityNames(
            evidence);
    const auto familyStems =
        FamilyIdentityStems(
            evidence);

    std::vector<std::wstring>
        familyWords;
    std::vector<std::wstring>
        versionTokens;

    for (const auto& name :
         identityNames) {
        for (const auto& token :
             Tokens(name)) {
            const std::wstring compact =
                Compact(token);

            if (compact.empty()) {
                continue;
            }

            if (IsFamilyVersionToken(
                    token)) {
                if (std::find(
                        versionTokens.begin(),
                        versionTokens.end(),
                        compact) ==
                    versionTokens.end()) {
                    versionTokens.push_back(
                        compact);
                }
                continue;
            }

            if (std::find(
                    familyWords.begin(),
                    familyWords.end(),
                    compact) ==
                familyWords.end()) {
                familyWords.push_back(
                    compact);
            }
        }
    }

    std::vector<std::wstring>
        residualTokens;

    const auto appendUnique =
        [&](std::wstring token) {
            if (token.empty() ||
                IsFamilyVersionToken(
                    token)) {
                return;
            }

            if (std::find(
                    residualTokens.begin(),
                    residualTokens.end(),
                    token) ==
                residualTokens.end()) {
                residualTokens.push_back(
                    std::move(token));
            }
        };

    const auto trimVersionEdges =
        [&](std::wstring& token) {
            bool changed = true;

            while (changed &&
                   !token.empty()) {
                changed = false;

                for (const auto& version :
                     versionTokens) {
                    if (version.empty()) {
                        continue;
                    }

                    if (token.starts_with(
                            version)) {
                        token.erase(
                            0,
                            version.size());
                        changed = true;
                    }

                    if (token.ends_with(
                            version)) {
                        token.resize(
                            token.size() -
                            version.size());
                        changed = true;
                    }
                }
            }

            if (token.empty()) {
                return;
            }

            std::size_t prefixDigits = 0;

            while (prefixDigits <
                       token.size() &&
                   std::iswdigit(
                       token[
                           prefixDigits])) {
                ++prefixDigits;
            }

            if (prefixDigits >= 4) {
                token.erase(
                    0,
                    prefixDigits);
            }

            if (token.empty()) {
                return;
            }

            std::size_t suffixStart =
                token.size();

            while (suffixStart > 0 &&
                   std::iswdigit(
                       token[
                           suffixStart -
                           1])) {
                --suffixStart;
            }

            if (token.size() -
                    suffixStart >=
                4) {
                token.resize(
                    suffixStart);
            }
        };

    for (const auto& rawToken :
         titleTokens) {

        if (IsFamilyVersionToken(
                rawToken)) {
            continue;
        }

        std::wstring token =
            Compact(rawToken);

        if (token.empty()) {
            continue;
        }

        if (std::find(
                familyWords.begin(),
                familyWords.end(),
                token) !=
            familyWords.end()) {
            continue;
        }

        const auto stemIt =
            std::find_if(
                familyStems.begin(),
                familyStems.end(),
                [&](const std::wstring&
                        stem) {
                    return
                        !stem.empty() &&
                        token.size() >
                            stem.size() &&
                        token.starts_with(
                            stem);
                });

        if (stemIt !=
            familyStems.end()) {
            token.erase(
                0,
                stemIt->size());
            trimVersionEdges(token);
            appendUnique(
                std::move(token));
            continue;
        }

        appendUnique(
            Lower(rawToken));
    }

    return residualTokens;
}

namespace {

[[nodiscard]] bool
RoleUsesExplicitSemanticIntent(
    ApplicationRole role) noexcept {

    switch (role) {
    case ApplicationRole::SuiteSubordinate:
    case ApplicationRole::AlternateLaunch:
    case ApplicationRole::SuiteUtility:
    case ApplicationRole::ConfigurationTool:
    case ApplicationRole::DiagnosticTool:
    case ApplicationRole::BenchmarkTool:
    case ApplicationRole::Installer:
    case ApplicationRole::Uninstaller:
    case ApplicationRole::Updater:
    case ApplicationRole::Downloader:
    case ApplicationRole::RepairTool:
    case ApplicationRole::Documentation:
    case ApplicationRole::ProductInfo:
    case ApplicationRole::BackgroundComponent:
    case ApplicationRole::ServiceComponent:
    case ApplicationRole::InternalComponent:
        return true;

    case ApplicationRole::Unknown:
    case ApplicationRole::PrimaryApplication:
    case ApplicationRole::CompanionApplication:
    case ApplicationRole::UserTool:
        return false;
    }

    return false;
}

[[nodiscard]] std::vector<std::wstring>
QueryIntentTokensForRole(
    std::wstring_view title,
    ApplicationRole role,
    std::vector<std::wstring>
        residualTokens) {

    if (!RoleUsesExplicitSemanticIntent(
            role)) {
        return residualTokens;
    }

    std::vector<std::wstring>
        semanticTokens;

    AppendDistinctivePhraseTokens(
        title,
        semanticTokens);

    // Restrictive catalog roles normally re-admit only on explicit role
    // semantics. Metadata can nevertheless identify an opaque diagnostic or
    // suite utility whose display title carries only a short product-specific
    // name. In that case the already family-stripped residual is the safest
    // explicit entry-name intent (for example a generic "Acme RX" fixture
    // leaves only "rx"). Family identity itself is never restored here.
    if (semanticTokens.empty() &&
        (role ==
             ApplicationRole::
                 DiagnosticTool ||
         role ==
             ApplicationRole::
                 SuiteUtility ||
         role ==
             ApplicationRole::
                 SuiteSubordinate)) {
        return residualTokens;
    }

    return semanticTokens;
}

} // namespace

ApplicationRoleDecision
ClassifyApplicationRole(
    const LaunchEvidence& evidence) {

    std::array<RoleScore, 20>
        scores{};

    if (IsStrongInternalPackagedEntry(
            evidence.packagedVisibility)) {
        AddSignal(
            scores,
            ApplicationRole::
                InternalComponent,
            6,
            EvidenceField::Structural,
            true);
    }

    const std::wstring lowerTitle =
        Lower(evidence.displayTitle);

    if (lowerTitle == L"about" ||
        lowerTitle.starts_with(
            L"about ") ||
        (lowerTitle.size() > 2 &&
         lowerTitle.starts_with(
             L"关于"))) {
        AddSignal(
            scores,
            ApplicationRole::ProductInfo,
            3,
            EvidenceField::Title);
    }

    AddTextSignals(
        scores,
        evidence.displayTitle,
        EvidenceField::Title,
        2);

    AddTextSignals(
        scores,
        evidence.executable
            .fileDescription,
        EvidenceField::Description,
        3);

    AddTextSignals(
        scores,
        evidence.executable
            .originalFilename,
        EvidenceField::
            OriginalFilename,
        2);

    AddTextSignals(
        scores,
        evidence.executable
            .internalName,
        EvidenceField::InternalName,
        2);

    AddTextSignals(
        scores,
        evidence.arguments,
        EvidenceField::Arguments,
        3);

    const std::wstring targetName =
        std::filesystem::path(
            evidence.resolvedTarget)
            .filename()
            .wstring();

    AddTextSignals(
        scores,
        targetName,
        EvidenceField::TargetName,
        1);

    const auto& product =
        evidence.executable
            .productName;

    const auto& description =
        evidence.executable
            .fileDescription;

    const auto familyNames =
        FamilyIdentityNames(
            evidence);

    const auto residualTokens =
        BuildDistinctiveTokens(
            evidence);

    const std::wstring compactTitle =
        Compact(
            evidence.displayTitle);
    const std::wstring normalizedTitleFamily =
        NormalizeFamilyStem(
            evidence.displayTitle);

    bool titleMatchesFamily = false;
    bool titleExtendsFamily = false;

    for (const auto& family :
         familyNames) {
        const std::wstring familyStem =
            NormalizeFamilyStem(
                family);

        if (familyStem.empty()) {
            continue;
        }

        if (normalizedTitleFamily ==
            familyStem) {
            titleMatchesFamily = true;
            break;
        }

        if (compactTitle.size() >
                familyStem.size() &&
            compactTitle.starts_with(
                familyStem)) {
            titleExtendsFamily = true;
        }
    }

    if (titleMatchesFamily) {
        AddSignal(
            scores,
            ApplicationRole::
                PrimaryApplication,
            2,
            EvidenceField::Title);
        AddSignal(
            scores,
            ApplicationRole::
                PrimaryApplication,
            2,
            EvidenceField::
                ProductRelation);
    }

    if (!product.empty() &&
        SameCompact(
            description,
            product)) {
        AddSignal(
            scores,
            ApplicationRole::
                PrimaryApplication,
            3,
            EvidenceField::
                Description);
    }

    const bool descriptionExtension =
        !product.empty() &&
        StartsWithCompact(
            description,
            product);

    if ((titleExtendsFamily ||
         descriptionExtension) &&
        residualTokens.empty()) {
        AddSignal(
            scores,
            ApplicationRole::
                PrimaryApplication,
            2,
            EvidenceField::
                ProductRelation);
        AddSignal(
            scores,
            ApplicationRole::
                PrimaryApplication,
            2,
            titleExtendsFamily
                ? EvidenceField::Title
                : EvidenceField::
                      Description);
    } else if (
        (titleExtendsFamily ||
         descriptionExtension) &&
        !residualTokens.empty()) {
        AddSignal(
            scores,
            ApplicationRole::
                CompanionApplication,
            2,
            EvidenceField::
                ProductRelation);
        AddSignal(
            scores,
            ApplicationRole::
                CompanionApplication,
            1,
            titleExtendsFamily
                ? EvidenceField::Title
                : EvidenceField::
                      Description);
    }

    if (evidence.source ==
            LaunchCandidateSource::Path ||
        evidence.targetKind ==
            LaunchTargetKind::
                ConsoleExecutable ||
        evidence.targetKind ==
            LaunchTargetKind::
                CommandScript) {
        AddSignal(
            scores,
            ApplicationRole::UserTool,
            2,
            EvidenceField::Structural);
    }

    ApplicationRole bestRole =
        ApplicationRole::Unknown;
    RoleScore bestScore{};

    for (std::size_t index = 1;
         index < scores.size();
         ++index) {
        const auto role =
            static_cast<ApplicationRole>(
                index);

        const auto& score =
            scores[index];

        if (score.points >
                bestScore.points ||
            (score.points ==
                 bestScore.points &&
             score.points > 0 &&
             RolePriority(role) >
                 RolePriority(
                     bestRole))) {
            bestRole = role;
            bestScore = score;
        }
    }

    ApplicationRoleDecision decision;
    decision.role = bestRole;

    if (bestRole !=
        ApplicationRole::Unknown) {
        decision.confidence =
            ConfidenceFor(
                bestScore);
    }

    decision.visibility =
        CatalogVisibilityForRole(
            decision.role,
            decision.confidence);

    decision.catalogGroupKey =
        BuildCatalogGroupKey(
            evidence);

    decision.distinctiveTokens =
        QueryIntentTokensForRole(
            evidence.displayTitle,
            decision.role,
            residualTokens);

    return decision;
}

void CalibrateCatalogRoleContext(
    std::vector<Command*>& commands) {

    const auto applyRole =
        [](Command& command,
           ApplicationRole role,
           RoleConfidence confidence) {
            command.applicationRole =
                role;
            command.roleConfidence =
                confidence;
            command.catalogVisibility =
                CatalogVisibilityForRole(
                    role,
                    confidence);
            command.distinctiveTokens =
                QueryIntentTokensForRole(
                    command.title,
                    role,
                    std::move(
                        command
                            .distinctiveTokens));
        };

    // Publication-time invariant repair: provider-specific metadata must not
    // be able to turn an explicit high-information role title back into a
    // Normal primary/companion entry. This pass is I/O-free and only
    // re-evaluates the already-cached display title.
    for (Command* command :
         commands) {
        if (command == nullptr ||
            command->source ==
                CommandSource::User) {
            continue;
        }

        LaunchEvidence titleEvidence;
        titleEvidence.source =
            LaunchCandidateSource::
                StartMenu;
        titleEvidence.displayTitle =
            command->title;
        titleEvidence.targetKind =
            LaunchTargetKind::
                GuiExecutable;

        const ApplicationRoleDecision
            titleDecision =
                ClassifyApplicationRole(
                    titleEvidence);

        if (!IsContextPromotableRole(
                titleDecision.role) ||
            titleDecision.confidence ==
                RoleConfidence::Low) {
            continue;
        }

        const bool normalIdentityRole =
            command->applicationRole ==
                ApplicationRole::Unknown ||
            command->applicationRole ==
                ApplicationRole::
                    PrimaryApplication ||
            command->applicationRole ==
                ApplicationRole::
                    CompanionApplication;

        if (normalIdentityRole ||
            (command->applicationRole ==
                 titleDecision.role &&
             command->roleConfidence ==
                 RoleConfidence::Low)) {
            applyRole(
                *command,
                titleDecision.role,
                titleDecision.confidence);
        }
    }

    std::vector<Command*> primaries;

    for (Command* command :
         commands) {
        if (command == nullptr ||
            command->source ==
                CommandSource::User ||
            command->applicationRole !=
                ApplicationRole::
                    PrimaryApplication ||
            command->roleConfidence ==
                RoleConfidence::Low) {
            continue;
        }

        primaries.push_back(command);
    }

    // Alternate launch variants are activation relationships first, catalog
    // location relationships second. A shortcut can live in a different
    // Start Menu subfolder (or even expose a different helper ProductName)
    // while still launching the same primary target with variant arguments.
    // Same-family or title-base evidence is sufficient for Medium confidence;
    // the same canonical target is High confidence. The phrase alone remains
    // Low/Normal and never suppresses an isolated application.
    for (Command* candidate :
         commands) {
        if (candidate == nullptr ||
            candidate->source ==
                CommandSource::User ||
            !LooksLikeAlternateLaunch(
                candidate->title)) {
            continue;
        }

        const bool eligibleRole =
            candidate->applicationRole ==
                ApplicationRole::Unknown ||
            candidate->applicationRole ==
                ApplicationRole::
                    PrimaryApplication ||
            candidate->applicationRole ==
                ApplicationRole::
                    CompanionApplication ||
            candidate->applicationRole ==
                ApplicationRole::
                    AlternateLaunch;

        if (!eligibleRole) {
            continue;
        }

        AlternateRelation bestRelation;

        for (const Command* primary :
             primaries) {
            if (primary == nullptr ||
                primary == candidate) {
                continue;
            }

            const AlternateRelation
                relation =
                    AlternateRelationToPrimary(
                        *candidate,
                        *primary);

            if (!relation.related) {
                continue;
            }

            if (!bestRelation.related ||
                (relation.sameTarget &&
                 !bestRelation.sameTarget)) {
                bestRelation = relation;
            }

            if (bestRelation.sameTarget) {
                break;
            }
        }

        if (!bestRelation.related) {
            continue;
        }

        applyRole(
            *candidate,
            ApplicationRole::
                AlternateLaunch,
            bestRelation.sameTarget
                ? RoleConfidence::High
                : RoleConfidence::Medium);
    }

    std::unordered_map<
        std::wstring,
        std::vector<Command*>>
        byFamily;

    for (Command* command :
         commands) {
        if (command == nullptr ||
            command->source ==
                CommandSource::User) {
            continue;
        }

        const std::wstring family =
            CatalogFamilyKey(
                *command);

        if (!family.empty()) {
            byFamily[family]
                .push_back(command);
        }
    }

    for (auto& [family, entries] :
         byFamily) {
        (void)family;

        for (Command* candidate :
             entries) {
            if (candidate == nullptr ||
                candidate->source ==
                    CommandSource::User) {
                continue;
            }

            Command* primary = nullptr;

            for (Command* peer :
                 entries) {
                if (peer == nullptr ||
                    peer == candidate ||
                    peer->applicationRole !=
                        ApplicationRole::
                            PrimaryApplication ||
                    peer->roleConfidence ==
                        RoleConfidence::Low ||
                    !SameCatalogContext(
                        *candidate,
                        *peer)) {
                    continue;
                }

                primary = peer;
                break;
            }

            if (primary == nullptr) {
                continue;
            }

            // Weak semantic words become actionable evidence only when the
            // same family/location also exposes a clear primary application.
            // Independent Editor/Renderer/Composer-style companions have no
            // such role cue and remain untouched. SuiteUtility deliberately
            // includes weak Sync/Scheduler/Automation/Maintenance cues here:
            // they stay Normal in isolation and become StrongMatchOnly only
            // after this corroboration.
            LaunchEvidence titleEvidence;
            titleEvidence.source =
                LaunchCandidateSource::
                    StartMenu;
            titleEvidence.displayTitle =
                candidate->title;
            titleEvidence.targetKind =
                LaunchTargetKind::
                    GuiExecutable;

            const ApplicationRoleDecision
                titleDecision =
                    ClassifyApplicationRole(
                        titleEvidence);

            const bool contextualUtilitySurface =
                titleDecision.role ==
                    ApplicationRole::
                        SuiteUtility &&
                HasContextualSuiteUtilityPhrase(
                    candidate->title);

            const bool identityRole =
                candidate->applicationRole ==
                    ApplicationRole::Unknown ||
                candidate->applicationRole ==
                    ApplicationRole::
                        PrimaryApplication ||
                candidate->applicationRole ==
                    ApplicationRole::
                        CompanionApplication ||
                (contextualUtilitySurface &&
                 (candidate
                      ->applicationRole ==
                      ApplicationRole::
                          BackgroundComponent ||
                  candidate
                      ->applicationRole ==
                      ApplicationRole::
                          ServiceComponent));

            if (identityRole &&
                IsContextPromotableRole(
                    titleDecision.role)) {
                applyRole(
                    *candidate,
                    titleDecision.role,
                    RoleConfidence::Medium);
                continue;
            }

            if (IsContextPromotableRole(
                    candidate
                        ->applicationRole) &&
                candidate->roleConfidence ==
                    RoleConfidence::Low) {
                applyRole(
                    *candidate,
                    candidate
                        ->applicationRole,
                    RoleConfidence::Medium);
            }
        }
    }

    // Suite topology is evaluated only after semantic/alternate calibration.
    // A longer companion becomes a subordinate only when two independent
    // structural relationships agree:
    //   1) its family-stripped display identity strictly extends another
    //      normal companion in the same catalog context; and
    //   2) the real resolved target independently extends that companion by
    //      executable stem or by a nearby installed directory segment.
    //
    // Target topology is intentionally independent from product vocabulary.
    // Opaque one-off companions remain Normal unless separate evidence exists.
    std::vector<SuiteTopologyDecision>
        topologyDecisions;

    for (auto& [family, entries] :
         byFamily) {
        (void)family;

        for (Command* candidate :
             entries) {
            if (candidate == nullptr ||
                candidate->source ==
                    CommandSource::User ||
                !IsSuiteTopologyIdentityRole(
                    *candidate)) {
                continue;
            }

            SuiteTopologyDecision best;
            best.candidate = candidate;

            for (Command* anchorCommand :
                 entries) {
                if (anchorCommand == nullptr ||
                    anchorCommand == candidate ||
                    anchorCommand->source ==
                        CommandSource::User ||
                    !IsSuiteTopologyIdentityRole(
                        *anchorCommand)) {
                    continue;
                }

                auto delta =
                    SuiteTopologyDelta(
                        *candidate,
                        *anchorCommand);

                if (delta.empty()) {
                    continue;
                }

                const std::size_t
                    anchorLength =
                        SuiteTitleIdentity(
                            *anchorCommand)
                            .size();

                if (anchorLength <=
                    best.anchorIdentityLength) {
                    continue;
                }

                best.anchorIdentityLength =
                    anchorLength;
                best.intentTokens =
                    std::move(delta);
            }

            if (!best.intentTokens.empty()) {
                topologyDecisions.push_back(
                    std::move(best));
            }
        }
    }

    for (auto& decision :
         topologyDecisions) {
        if (decision.candidate == nullptr) {
            continue;
        }

        decision.candidate
            ->distinctiveTokens =
            std::move(
                decision.intentTokens);

        applyRole(
            *decision.candidate,
            ApplicationRole::
                SuiteSubordinate,
            RoleConfidence::Medium);
    }
}

} // namespace altrun
