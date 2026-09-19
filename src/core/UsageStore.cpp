#include "UsageStore.hpp"

#include "../platform/WinUtil.hpp"

#include <fstream>

namespace altrun {

UsageStore::UsageStore(std::filesystem::path path) : path_(std::move(path)) {}

void UsageStore::Load() {
    usage_.clear();
    std::ifstream input(path_, std::ios::binary);
    if (!input) return;

    std::string lineUtf8;
    while (std::getline(input, lineUtf8)) {
        if (!lineUtf8.empty() && lineUtf8.back() == '\r') lineUtf8.pop_back();
        const auto fields = win::SplitTabs(win::Utf8ToWide(lineUtf8));
        if (fields.size() < 3 || fields[0].empty()) continue;

        try {
            UsageStat stat;
            stat.launches = std::stoull(fields[1]);
            stat.lastUsedUnix = std::stoll(fields[2]);
            usage_[fields[0]] = stat;
        } catch (...) {
            // Ignore malformed history rows rather than blocking startup.
        }
    }
}

void UsageStore::Record(std::wstring_view commandId) {
    auto& stat = usage_[std::wstring(commandId)];
    ++stat.launches;
    stat.lastUsedUnix = win::UnixTimeNow();
    Save();
}

void UsageStore::Save() const {
    const auto temp = path_.wstring() + L".tmp";
    {
        std::ofstream out(std::filesystem::path(temp), std::ios::binary | std::ios::trunc);
        if (!out) return;
        for (const auto& [id, stat] : usage_) {
            out << win::WideToUtf8(id) << '\t' << stat.launches << '\t' << stat.lastUsedUnix << '\n';
        }
    }

    std::error_code ec;
    std::filesystem::rename(std::filesystem::path(temp), path_, ec);
    if (ec) {
        std::filesystem::remove(path_, ec);
        ec.clear();
        std::filesystem::rename(std::filesystem::path(temp), path_, ec);
    }
}

} // namespace altrun
