#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace altrun {

struct PinyinForms {
    std::wstring full;
    std::wstring initials;
    std::vector<std::wstring> syllables;
};

class PinyinSearch {
public:
    explicit PinyinSearch(
        std::filesystem::path dictionaryDirectory = {});

    ~PinyinSearch();

    PinyinSearch(PinyinSearch&&) noexcept;
    PinyinSearch& operator=(PinyinSearch&&) noexcept;

    PinyinSearch(const PinyinSearch&) = delete;
    PinyinSearch& operator=(const PinyinSearch&) = delete;

    [[nodiscard]] bool Available() const noexcept;

    [[nodiscard]] const PinyinForms* FormsFor(
        std::wstring_view text) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace altrun
