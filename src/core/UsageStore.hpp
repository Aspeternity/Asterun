#pragma once

#include "SearchEngine.hpp"

#include <filesystem>
#include <string_view>

namespace altrun {

class UsageStore {
public:
    explicit UsageStore(std::filesystem::path path);

    void Load();
    void Record(std::wstring_view commandId);
    [[nodiscard]] const UsageMap& Data() const noexcept { return usage_; }

private:
    void Save() const;

    std::filesystem::path path_;
    UsageMap usage_;
};

} // namespace altrun
