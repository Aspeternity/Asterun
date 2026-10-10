// Locale and platform independence of the opaque auxiliary identity rule.
//
// An "opaque" entry identity is a short ASCII code such as "X9". It lets a
// generic ProductName corroborate a role (single-entry classification) and,
// through suite context calibration, lets an entry next to a clear primary
// become a StrongMatchOnly suite utility. Meaningful non-ASCII names must not
// qualify on any platform or in any C runtime locale.
//
// Every scenario is checked under the "C" locale and under a UTF-8 locale.
// The MSVC CRT and glibc's UTF-8 locales report CJK ideographs as
// alphanumeric; glibc's "C" locale does not. The test verifies that each
// locale really took effect in this process before relying on it.

#include "core/Command.hpp"
#include "core/LaunchRole.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <clocale>
#include <cstdio>
#include <cwctype>
#include <iostream>
#include <string>
#include <vector>

using namespace altrun;

namespace {

struct Outcome {
    ApplicationRole role{ApplicationRole::Unknown};
    RoleConfidence confidence{RoleConfidence::Low};
    CatalogVisibility visibility{CatalogVisibility::Normal};
    std::vector<std::wstring> tokens;

    bool operator==(const Outcome&) const = default;
};

bool HasToken(
    const std::vector<std::wstring>& tokens,
    std::wstring_view token) {
    return std::find(tokens.begin(), tokens.end(), token) != tokens.end();
}

LaunchEvidence Evidence(
    std::wstring title,
    std::wstring target) {
    LaunchEvidence evidence;
    evidence.source = LaunchCandidateSource::StartMenu;
    evidence.displayTitle = std::move(title);
    evidence.resolvedTarget = std::move(target);
    evidence.installRootHint =
        std::filesystem::path(evidence.resolvedTarget).parent_path();
    evidence.targetKind = LaunchTargetKind::GuiExecutable;
    return evidence;
}

// Single entry: a generic diagnostic ProductName plus one independent
// metadata field. ProductName may only count for an opaque entry name.
LaunchEvidence CorroboratedEvidence(std::wstring_view name) {
    auto evidence = Evidence(
        L"Northwind " + std::wstring(name) + L" 2026",
        L"C:/Program Files/Northwind/Tool.exe");
    evidence.executable.productName = L"Northwind Diagnostic Utility";
    evidence.executable.internalName = L"Diagnostic Assistant";
    return evidence;
}

Outcome ClassifyCorroborated(std::wstring_view name) {
    const auto decision = ClassifyApplicationRole(CorroboratedEvidence(name));
    return {decision.role, decision.confidence, decision.visibility, decision.distinctiveTokens};
}

Outcome ClassifyBenchmark() {
    auto evidence = Evidence(
        L"Contoso Studio 性能测试 2026",
        L"C:/Program Files/Contoso/Tools/Bench.exe");
    evidence.startMenuFolder =
        L"C:/ProgramData/Microsoft/Windows/Start Menu/Programs/Contoso Studio 2026";
    evidence.executable.productName = L"Contoso Benchmark Utility";
    const auto decision = ClassifyApplicationRole(evidence);
    return {decision.role, decision.confidence, decision.visibility, decision.distinctiveTokens};
}

// Suite context: a clear High-confidence primary and an entry in the same
// catalog context whose only identity is `token`.
Outcome Calibrate(std::wstring_view token, bool sameInstallDirectory) {
    auto mainEvidence = Evidence(
        L"Acme Studio 2026",
        L"C:/Program Files/Acme/Studio/Studio.exe");
    mainEvidence.startMenuFolder =
        L"C:/ProgramData/Microsoft/Windows/Start Menu/Programs/Acme Studio 2026";
    const std::wstring group = BuildCatalogGroupKey(mainEvidence);

    Command primary;
    primary.source = CommandSource::StartMenu;
    primary.title = L"Acme Studio 2026";
    primary.target = L"C:\\Program Files\\Acme\\Studio\\Studio.exe";
    primary.canonicalIdentity = L"file:c:\\program files\\acme\\studio\\studio.exe";
    primary.applicationRole = ApplicationRole::PrimaryApplication;
    primary.roleConfidence = RoleConfidence::High;
    primary.catalogVisibility = CatalogVisibility::Normal;
    primary.catalogGroupKey = group;

    Command entry;
    entry.source = CommandSource::StartMenu;
    entry.title = L"Acme Studio " + std::wstring(token) + L" 2026";
    if (sameInstallDirectory) {
        entry.target = L"C:\\Program Files\\Acme\\Studio\\Entry.exe";
        entry.canonicalIdentity = L"file:c:\\program files\\acme\\studio\\entry.exe";
    } else {
        entry.target = L"C:\\Program Files\\Acme\\Independent\\Entry.exe";
        entry.canonicalIdentity = L"file:c:\\program files\\acme\\independent\\entry.exe";
    }
    entry.applicationRole = ApplicationRole::PrimaryApplication;
    entry.roleConfidence = RoleConfidence::Medium;
    entry.catalogVisibility = CatalogVisibility::Normal;
    entry.catalogGroupKey = group;
    entry.distinctiveTokens = {std::wstring(token)};

    std::vector<Command*> commands{&primary, &entry};
    CalibrateCatalogRoleContext(commands);
    return {entry.applicationRole, entry.roleConfidence, entry.catalogVisibility, entry.distinctiveTokens};
}

void ExpectOpaque(std::wstring_view name, std::wstring_view token) {
    const auto single = ClassifyCorroborated(name);
    assert(single.role == ApplicationRole::DiagnosticTool);
    assert(single.confidence == RoleConfidence::High);
    assert(single.visibility == CatalogVisibility::StrongMatchOnly);
    assert(HasToken(single.tokens, token));

    const auto suite = Calibrate(token, true);
    assert(suite.role == ApplicationRole::SuiteUtility);
    assert(suite.confidence == RoleConfidence::Medium);
    assert(suite.visibility == CatalogVisibility::StrongMatchOnly);
    assert(HasToken(suite.tokens, token));

    // Opaque naming is never sufficient without install-tree corroboration.
    const auto distant = Calibrate(token, false);
    assert(distant.role == ApplicationRole::PrimaryApplication);
    assert(distant.visibility == CatalogVisibility::Normal);
}

void ExpectNotOpaque(std::wstring_view name, std::wstring_view token) {
    // Without ProductName the single remaining metadata field is weak.
    const auto single = ClassifyCorroborated(name);
    assert(single.role == ApplicationRole::DiagnosticTool);
    assert(single.confidence == RoleConfidence::Low);
    assert(single.visibility == CatalogVisibility::Normal);

    // The entry keeps its own identity and stays in normal search results.
    const auto suite = Calibrate(token, true);
    assert(suite.role == ApplicationRole::PrimaryApplication);
    assert(suite.confidence == RoleConfidence::Medium);
    assert(suite.visibility == CatalogVisibility::Normal);
    assert(HasToken(suite.tokens, token));
}

// Accented Latin letters additionally depend on the tokenizer, which is out
// of scope here: in glibc's "C" locale the tokenizer drops "é" before this
// rule runs. Assert the rule whenever the product's own tokenization keeps
// the letter, and count those checks so they cannot silently vanish.
int ExpectAccentedNotOpaque(std::wstring_view name, std::wstring_view token) {
    const auto residual = BuildDistinctiveTokens(CorroboratedEvidence(name));
    if (!HasToken(residual, token)) {
        return 0;
    }
    ExpectNotOpaque(name, token);
    return 1;
}

struct LocaleRun {
    std::string name;
    std::vector<Outcome> invariant;
    int accentedChecks{0};
};

LocaleRun RunScenarios(const std::string& localeName) {
    LocaleRun run;
    run.name = localeName;

    // The failure first seen on Windows: a meaningful Chinese benchmark name
    // must not be treated as an opaque code that lets ProductName raise the
    // role confidence.
    const auto benchmark = ClassifyBenchmark();
    assert(benchmark.role == ApplicationRole::BenchmarkTool);
    assert(benchmark.confidence == RoleConfidence::Medium);
    assert(benchmark.visibility == CatalogVisibility::StrongMatchOnly);
    assert(benchmark.tokens.size() == 1);
    assert(benchmark.tokens[0] == L"性能测试");
    run.invariant.push_back(benchmark);

    // Chinese names, with and without digits, are never opaque codes.
    ExpectNotOpaque(L"测9", L"测9");
    ExpectNotOpaque(L"工具9", L"工具9");
    ExpectNotOpaque(L"编辑器", L"编辑器");

    // Short ASCII codes keep their established behaviour, length 2 to 5.
    ExpectOpaque(L"X9", L"x9");
    ExpectOpaque(L"Q7", L"q7");
    ExpectOpaque(L"QX", L"qx");
    ExpectOpaque(L"ABCDE", L"abcde");

    // Length limits are unchanged.
    ExpectNotOpaque(L"Q", L"q");
    ExpectNotOpaque(L"ABCDEF", L"abcdef");

    // Version identifiers are excluded as before.
    for (const wchar_t* version : {L"r2", L"v12", L"sp1"}) {
        const auto suite = Calibrate(version, true);
        assert(suite.role == ApplicationRole::PrimaryApplication);
        assert(suite.visibility == CatalogVisibility::Normal);
    }

    for (const std::wstring_view name :
         {L"X9", L"QX", L"ABCDE", L"Q", L"ABCDEF", L"测9", L"工具9", L"编辑器"}) {
        run.invariant.push_back(ClassifyCorroborated(name));
    }
    for (const std::wstring_view token :
         {L"x9", L"qx", L"abcde", L"q", L"abcdef", L"测9", L"工具9", L"编辑器", L"r2", L"v12", L"sp1"}) {
        run.invariant.push_back(Calibrate(token, true));
        run.invariant.push_back(Calibrate(token, false));
    }

    run.accentedChecks += ExpectAccentedNotOpaque(L"Ré", L"ré");
    run.accentedChecks += ExpectAccentedNotOpaque(L"Café", L"café");
    return run;
}

std::string Lowercase(std::string value) {
    for (auto& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

// Switches the process locale and reads it back, so the test never relies on
// an environment variable alone.
bool UseLocale(const char* requested, std::string& active) {
    const char* result = std::setlocale(LC_ALL, requested);
    if (result == nullptr) {
        return false;
    }
    const char* queried = std::setlocale(LC_ALL, nullptr);
    assert(queried != nullptr);
    active = queried;
    return true;
}

} // namespace

int main() {
    std::string cName;
    const bool cActive = UseLocale("C", cName);
    assert(cActive);
    assert(cName == "C");
    const bool cjkAlnumInC = std::iswalnum(0x6027) != 0;
    const auto cRun = RunScenarios(cName);

    std::string utf8Name;
    bool utf8Active = false;
    for (const char* candidate : {"C.UTF-8", "C.utf8", "en_US.UTF-8", ".UTF-8"}) {
        if (UseLocale(candidate, utf8Name)) {
            utf8Active = true;
            break;
        }
    }
    assert(utf8Active);
    const auto lowered = Lowercase(utf8Name);
    assert(lowered.find("utf-8") != std::string::npos ||
           lowered.find("utf8") != std::string::npos);
    const bool cjkAlnumInUtf8 = std::iswalnum(0x6027) != 0;
#if defined(__GLIBC__)
    // On glibc the two locales classify CJK differently, so this proves that
    // the scenarios really ran under two different wide-character tables.
    assert(!cjkAlnumInC);
    assert(cjkAlnumInUtf8);
#endif
    const auto utf8Run = RunScenarios(utf8Name);

    assert(cRun.invariant.size() == utf8Run.invariant.size());
    assert(cRun.invariant == utf8Run.invariant);
    assert(cRun.accentedChecks + utf8Run.accentedChecks >= 2);

    std::setlocale(LC_ALL, "C");

    std::cout << "Launch role locale tests passed: locales \"" << cName
              << "\" (iswalnum U+6027=" << cjkAlnumInC << ") and \"" << utf8Name
              << "\" (iswalnum U+6027=" << cjkAlnumInUtf8 << "); "
              << cRun.invariant.size() << " invariant outcomes compared; accented checks C="
              << cRun.accentedChecks << " UTF-8=" << utf8Run.accentedChecks << "\n";
    return 0;
}
