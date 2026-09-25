#include "LaunchRole.hpp"

#include "LaunchCatalog.hpp"
#include "Command.hpp"

#include <algorithm>
#include <array>
#include <bit>
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
    std::array<RoleScore, 18>& scores,
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

    if (score.points >= 3 ||
        (score.points >= 3 &&
         fieldCount >= 2)) {
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
        return std::max(
            strength,
            3);
    }

    return strength;
}

void AddTextSignals(
    std::array<RoleScore, 18>& scores,
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
             L"troubleshoot",
             L"troubleshooter",
             L"health check",
             L"诊断",
             L"故障排除"})) {
        AddSignal(
            scores,
            ApplicationRole::DiagnosticTool,
            SemanticStrength(
                value,
                field,
                strength,
                {L"diagnostics",
                 L"diagnostic tool",
                 L"diagnostic utility",
                 L"troubleshooter",
                 L"health check",
                 L"诊断工具",
                 L"诊断程序",
                 L"故障排除"}),
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

    if (ContainsAny(
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
             L"troubleshoot",
             L"troubleshooter",
             L"health check",
             L"诊断",
             L"故障排除",
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
             L"修复",
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
}

struct CatalogGroupParts {
    std::wstring product;
    std::wstring locationKind;
    std::wstring location;

    [[nodiscard]] bool Valid()
        const noexcept {
        return !product.empty() &&
            !locationKind.empty() &&
            !location.empty();
    }
};

[[nodiscard]] CatalogGroupParts
ParseCatalogGroupKeyParts(
    std::wstring_view key) {

    constexpr std::wstring_view
        kProductPrefix = L"product:";
    constexpr std::wstring_view
        kRootPrefix = L"root:";
    constexpr std::wstring_view
        kMenuPrefix = L"menu:";

    CatalogGroupParts parts;

    if (!key.starts_with(
            kProductPrefix)) {
        return parts;
    }

    const std::size_t separator =
        key.find(
            L'|',
            kProductPrefix.size());

    if (separator ==
        std::wstring_view::npos) {
        return parts;
    }

    parts.product =
        std::wstring(
            key.substr(
                kProductPrefix.size(),
                separator -
                    kProductPrefix.size()));

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
        a.product == b.product &&
        a.locationKind ==
            b.locationKind &&
        RelatedCatalogLocation(
            a.location,
            b.location);
}

[[nodiscard]] std::wstring
CatalogProductKey(
    const Command& command) {

    return ParseCatalogGroupKeyParts(
               command.catalogGroupKey)
        .product;
}

[[nodiscard]] bool
IsContextPromotableRole(
    ApplicationRole role) noexcept {

    switch (role) {
    case ApplicationRole::ConfigurationTool:
    case ApplicationRole::DiagnosticTool:
    case ApplicationRole::BenchmarkTool:
    case ApplicationRole::Installer:
    case ApplicationRole::Uninstaller:
    case ApplicationRole::Updater:
    case ApplicationRole::Downloader:
    case ApplicationRole::RepairTool:
        return true;

    case ApplicationRole::Unknown:
    case ApplicationRole::PrimaryApplication:
    case ApplicationRole::CompanionApplication:
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
    case ApplicationRole::AlternateLaunch:
        return "alternate-launch";
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
        18>
        values{{
            {"unknown", ApplicationRole::Unknown},
            {"primary-application", ApplicationRole::PrimaryApplication},
            {"companion-application", ApplicationRole::CompanionApplication},
            {"alternate-launch", ApplicationRole::AlternateLaunch},
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
    case ApplicationRole::AlternateLaunch:
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
    const std::wstring product =
        Compact(
            evidence.executable
                .productName);

    if (product.empty()) {
        return {};
    }

    const std::wstring root =
        NormalizedPath(
            evidence.installRootHint);

    if (!root.empty()) {
        return L"product:" +
            product +
            L"|root:" +
            root;
    }

    const std::wstring menu =
        NormalizedPath(
            evidence.startMenuFolder);

    if (!menu.empty()) {
        return L"product:" +
            product +
            L"|menu:" +
            menu;
    }

    return {};
}

std::vector<std::wstring>
BuildDistinctiveTokens(
    const LaunchEvidence& evidence) {

    auto titleTokens =
        Tokens(
            evidence.displayTitle);

    std::vector<std::wstring>
        familyTokens =
            Tokens(
                evidence.executable
                    .productName);

    if (familyTokens.empty() &&
        !evidence.startMenuFolder
             .empty()) {
        familyTokens =
            Tokens(
                evidence.startMenuFolder
                    .filename()
                    .wstring());
    }

    titleTokens.erase(
        std::remove_if(
            titleTokens.begin(),
            titleTokens.end(),
            [&](const std::wstring& token) {
                return IsVersionToken(token) ||
                    std::find(
                        familyTokens.begin(),
                        familyTokens.end(),
                        token) !=
                        familyTokens.end();
            }),
        titleTokens.end());

    // CJK product/role text is often contiguous, so ordinary tokenization
    // cannot always separate family text from explicit role intent. Persist
    // generic semantic role phrases as additional distinctive tokens during
    // discovery; query admission can then stay I/O-free and product-neutral.
    AppendDistinctivePhraseTokens(
        evidence.displayTitle,
        titleTokens);

    return titleTokens;
}

ApplicationRoleDecision
ClassifyApplicationRole(
    const LaunchEvidence& evidence) {

    std::array<RoleScore, 18>
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

    if (!product.empty()) {
        if (SameCompact(
                evidence.displayTitle,
                product)) {
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

        if (SameCompact(
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

        const bool titleExtension =
            StartsWithCompact(
                evidence.displayTitle,
                product);

        const bool descriptionExtension =
            StartsWithCompact(
                description,
                product);

        const auto distinctive =
            BuildDistinctiveTokens(
                evidence);

        if ((titleExtension ||
             descriptionExtension) &&
            distinctive.empty()) {
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
                titleExtension
                    ? EvidenceField::Title
                    : EvidenceField::
                          Description);
        } else if (
            (titleExtension ||
             descriptionExtension) &&
            !distinctive.empty()) {
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
                titleExtension
                    ? EvidenceField::Title
                    : EvidenceField::
                          Description);
        }
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
        BuildDistinctiveTokens(
            evidence);

    return decision;
}

void CalibrateCatalogRoleContext(
    std::vector<Command*>& commands) {

    std::unordered_map<
        std::wstring,
        std::vector<Command*>>
        byProduct;

    for (Command* command :
         commands) {
        if (command == nullptr ||
            command->source ==
                CommandSource::User) {
            continue;
        }

        const std::wstring product =
            CatalogProductKey(
                *command);

        if (!product.empty()) {
            byProduct[product]
                .push_back(command);
        }
    }

    for (auto& [product, entries] :
         byProduct) {
        (void)product;

        for (Command* candidate :
             entries) {
            if (candidate == nullptr ||
                candidate->source ==
                    CommandSource::User ||
                candidate->applicationRole ==
                    ApplicationRole::
                        PrimaryApplication) {
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

            // A weak role word such as "settings" stays conservative in
            // isolation. Inside a product/location group that already has a
            // clear primary application, the same cue becomes corroborated
            // catalog evidence. Independent companion names (Encoder,
            // Composer, Render, Editor, etc.) produce no such role cue.
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

            if ((candidate
                     ->applicationRole ==
                         ApplicationRole::
                             Unknown ||
                 candidate
                     ->applicationRole ==
                         ApplicationRole::
                             CompanionApplication) &&
                IsContextPromotableRole(
                    titleDecision.role)) {
                candidate->applicationRole =
                    titleDecision.role;
                candidate->roleConfidence =
                    RoleConfidence::Medium;
                candidate->catalogVisibility =
                    CatalogVisibilityForRole(
                        candidate
                            ->applicationRole,
                        candidate
                            ->roleConfidence);
                continue;
            }

            if (IsContextPromotableRole(
                    candidate
                        ->applicationRole) &&
                candidate->roleConfidence ==
                    RoleConfidence::Low) {
                candidate->roleConfidence =
                    RoleConfidence::Medium;
                candidate->catalogVisibility =
                    CatalogVisibilityForRole(
                        candidate
                            ->applicationRole,
                        candidate
                            ->roleConfidence);
                continue;
            }

            if ((candidate
                     ->applicationRole ==
                         ApplicationRole::
                             Unknown ||
                 candidate
                     ->applicationRole ==
                         ApplicationRole::
                             CompanionApplication) &&
                LooksLikeAlternateLaunch(
                    candidate->title)) {

                const bool sameTarget =
                    !BaseCanonicalIdentity(
                         *candidate)
                         .empty() &&
                    BaseCanonicalIdentity(
                        *candidate) ==
                        BaseCanonicalIdentity(
                            *primary);

                candidate->applicationRole =
                    ApplicationRole::
                        AlternateLaunch;
                candidate->roleConfidence =
                    sameTarget ||
                            !candidate
                                 ->arguments
                                 .empty()
                        ? RoleConfidence::High
                        : RoleConfidence::Medium;
                candidate->catalogVisibility =
                    CatalogVisibilityForRole(
                        candidate
                            ->applicationRole,
                        candidate
                            ->roleConfidence);
            }
        }
    }
}

} // namespace altrun
