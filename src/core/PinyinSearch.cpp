#include "PinyinSearch.hpp"

#include <cpp-pinyin/G2pglobal.h>
#include <cpp-pinyin/Pinyin.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <unordered_map>
#include <utility>

namespace altrun {

namespace {

bool ContainsSupportedHanzi(std::wstring_view text) {
    return std::any_of(
        text.begin(),
        text.end(),
        [](wchar_t ch) {
            const std::uint32_t value =
                static_cast<std::uint32_t>(ch);
            return value >= 0x4E00u &&
                   value <= 0x9FFFu;
        });
}

void AppendUtf8(
    std::string& output,
    std::uint32_t codePoint) {

    if (codePoint <= 0x7Fu) {
        output.push_back(
            static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FFu) {
        output.push_back(
            static_cast<char>(
                0xC0u |
                (codePoint >> 6u)));
        output.push_back(
            static_cast<char>(
                0x80u |
                (codePoint & 0x3Fu)));
    } else if (codePoint <= 0xFFFFu) {
        output.push_back(
            static_cast<char>(
                0xE0u |
                (codePoint >> 12u)));
        output.push_back(
            static_cast<char>(
                0x80u |
                ((codePoint >> 6u) & 0x3Fu)));
        output.push_back(
            static_cast<char>(
                0x80u |
                (codePoint & 0x3Fu)));
    } else if (codePoint <= 0x10FFFFu) {
        output.push_back(
            static_cast<char>(
                0xF0u |
                (codePoint >> 18u)));
        output.push_back(
            static_cast<char>(
                0x80u |
                ((codePoint >> 12u) & 0x3Fu)));
        output.push_back(
            static_cast<char>(
                0x80u |
                ((codePoint >> 6u) & 0x3Fu)));
        output.push_back(
            static_cast<char>(
                0x80u |
                (codePoint & 0x3Fu)));
    }
}

std::string WideToUtf8(std::wstring_view text) {
    std::string output;
    output.reserve(text.size() * 3);

    for (std::size_t i = 0; i < text.size(); ++i) {
        std::uint32_t codePoint =
            static_cast<std::uint32_t>(text[i]);

        if constexpr (sizeof(wchar_t) == 2) {
            if (codePoint >= 0xD800u &&
                codePoint <= 0xDBFFu &&
                i + 1 < text.size()) {

                const std::uint32_t low =
                    static_cast<std::uint32_t>(
                        text[i + 1]);

                if (low >= 0xDC00u &&
                    low <= 0xDFFFu) {
                    codePoint =
                        0x10000u +
                        ((codePoint - 0xD800u) << 10u) +
                        (low - 0xDC00u);
                    ++i;
                }
            }
        }

        AppendUtf8(output, codePoint);
    }

    return output;
}

std::wstring NormalizeAsciiPiece(
    std::string_view value) {

    std::wstring output;
    output.reserve(value.size());

    for (const unsigned char ch : value) {
        if (ch >= 0x80u) {
            continue;
        }

        if (std::isalnum(ch)) {
            output.push_back(
                static_cast<wchar_t>(
                    std::tolower(ch)));
        }
    }

    return output;
}

std::vector<std::string> SplitAsciiWords(
    std::string_view value) {

    std::vector<std::string> words;
    std::string current;

    auto flush = [&]() {
        if (!current.empty()) {
            words.push_back(current);
            current.clear();
        }
    };

    for (std::size_t i = 0; i < value.size(); ++i) {
        const unsigned char ch =
            static_cast<unsigned char>(value[i]);

        if (ch >= 0x80u ||
            !std::isalnum(ch)) {
            flush();
            continue;
        }

        const bool upper =
            std::isupper(ch) != 0;

        const bool previousLower =
            !current.empty() &&
            std::islower(
                static_cast<unsigned char>(
                    current.back())) != 0;

        const bool previousUpper =
            !current.empty() &&
            std::isupper(
                static_cast<unsigned char>(
                    current.back())) != 0;

        const bool nextLower =
            i + 1 < value.size() &&
            static_cast<unsigned char>(
                value[i + 1]) < 0x80u &&
            std::islower(
                static_cast<unsigned char>(
                    value[i + 1])) != 0;

        if (!current.empty() &&
            upper &&
            (previousLower ||
             (previousUpper &&
              nextLower &&
              current.size() > 1))) {
            flush();
        }

        current.push_back(
            static_cast<char>(ch));
    }

    flush();
    return words;
}

bool IsShortUpperAcronym(
    std::string_view value) {

    if (value.size() < 2 ||
        value.size() > 4) {
        return false;
    }

    return std::all_of(
        value.begin(),
        value.end(),
        [](unsigned char ch) {
            return std::isdigit(ch) ||
                   std::isupper(ch);
        });
}

void AppendAsciiForms(
    PinyinForms& forms,
    std::string_view raw) {

    const std::wstring compact =
        NormalizeAsciiPiece(raw);

    if (compact.empty()) {
        return;
    }

    forms.full += compact;

    const auto words =
        SplitAsciiWords(raw);

    if (words.empty()) {
        forms.initials += compact;
        forms.syllables.push_back(compact);
        return;
    }

    for (const auto& word : words) {
        const std::wstring normalized =
            NormalizeAsciiPiece(word);

        if (normalized.empty()) {
            continue;
        }

        forms.syllables.push_back(normalized);

        if (IsShortUpperAcronym(word)) {
            forms.initials += normalized;
        } else {
            forms.initials.push_back(
                normalized.front());
        }
    }
}

PinyinForms BuildForms(
    const Pinyin::Pinyin& converter,
    std::wstring_view text) {

    PinyinForms forms;

    const auto result =
        converter.hanziToPinyin(
            WideToUtf8(text),
            Pinyin::ManTone::Style::NORMAL,
            Pinyin::Error::Default,
            false,
            false,
            false);

    for (const auto& item : result) {
        if (item.error) {
            AppendAsciiForms(
                forms,
                item.hanzi);
            continue;
        }

        const std::wstring normalized =
            NormalizeAsciiPiece(
                item.pinyin);

        if (normalized.empty()) {
            continue;
        }

        forms.full += normalized;
        forms.initials.push_back(
            normalized.front());
        forms.syllables.push_back(
            normalized);
    }

    return forms;
}

} // namespace

struct PinyinSearch::Impl {
    explicit Impl(
        std::filesystem::path directory)
        : dictionaryDirectory(
              std::move(directory)) {

        std::error_code ec;
        const auto requiredDictionary =
            dictionaryDirectory /
            "mandarin" /
            "word.txt";

        if (dictionaryDirectory.empty() ||
            !std::filesystem::exists(
                requiredDictionary,
                ec)) {
            return;
        }

        try {
            Pinyin::setDictionaryPath(
                dictionaryDirectory);

            converter =
                std::make_unique<Pinyin::Pinyin>();

            available =
                converter &&
                converter->initialized();
        } catch (...) {
            converter.reset();
            available = false;
        }
    }

    std::filesystem::path dictionaryDirectory;
    std::unique_ptr<Pinyin::Pinyin> converter;
    bool available{false};

    mutable std::unordered_map<
        std::wstring,
        PinyinForms> cache;
};

PinyinSearch::PinyinSearch(
    std::filesystem::path dictionaryDirectory)
    : impl_(
          std::make_unique<Impl>(
              std::move(dictionaryDirectory))) {}

PinyinSearch::~PinyinSearch() = default;

PinyinSearch::PinyinSearch(
    PinyinSearch&&) noexcept = default;

PinyinSearch& PinyinSearch::operator=(
    PinyinSearch&&) noexcept = default;

bool PinyinSearch::Loaded() const noexcept {
    return impl_ &&
        impl_->converter != nullptr;
}

bool PinyinSearch::Available() const noexcept {
    return impl_ && impl_->available;
}

std::size_t
PinyinSearch::CacheEntryCount() const noexcept {
    return impl_
        ? impl_->cache.size()
        : 0;
}

const PinyinForms* PinyinSearch::FormsFor(
    std::wstring_view text) const {

    if (!Available() ||
        text.empty() ||
        !ContainsSupportedHanzi(text)) {
        return nullptr;
    }

    std::wstring key(text);

    const auto existing =
        impl_->cache.find(key);

    if (existing != impl_->cache.end()) {
        return &existing->second;
    }

    PinyinForms forms;

    try {
        forms =
            BuildForms(
                *impl_->converter,
                text);
    } catch (...) {
        return nullptr;
    }

    const auto [it, inserted] =
        impl_->cache.emplace(
            std::move(key),
            std::move(forms));

    (void) inserted;
    return &it->second;
}

} // namespace altrun
