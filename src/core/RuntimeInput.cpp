#include "RuntimeInput.hpp"

#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <string>
#include <type_traits>

namespace altrun {
namespace {

[[nodiscard]] std::wstring_view
Trim(std::wstring_view value) {
    while (!value.empty() &&
           std::iswspace(value.front())) {
        value.remove_prefix(1);
    }

    while (!value.empty() &&
           std::iswspace(value.back())) {
        value.remove_suffix(1);
    }

    return value;
}

[[nodiscard]] bool
EqualInsensitive(
    std::wstring_view left,
    std::wstring_view right) {
    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t i = 0;
         i < left.size();
         ++i) {
        if (std::towlower(left[i]) !=
            std::towlower(right[i])) {
            return false;
        }
    }

    return true;
}

[[nodiscard]] bool
MatchesKeyword(
    const Command& command,
    std::wstring_view token) {
    if (EqualInsensitive(
            command.keyword,
            token)) {
        return true;
    }

    return std::any_of(
        command.aliases.begin(),
        command.aliases.end(),
        [&](const std::wstring& alias) {
            return EqualInsensitive(
                alias,
                token);
        });
}

void AppendUtf8(
    std::string& output,
    std::uint32_t codePoint) {
    if (codePoint <= 0x7Fu) {
        output.push_back(
            static_cast<char>(
                codePoint));
    } else if (codePoint <= 0x7FFu) {
        output.push_back(
            static_cast<char>(
                0xC0u |
                (codePoint >> 6)));
        output.push_back(
            static_cast<char>(
                0x80u |
                (codePoint & 0x3Fu)));
    } else if (codePoint <= 0xFFFFu) {
        output.push_back(
            static_cast<char>(
                0xE0u |
                (codePoint >> 12)));
        output.push_back(
            static_cast<char>(
                0x80u |
                ((codePoint >> 6) &
                 0x3Fu)));
        output.push_back(
            static_cast<char>(
                0x80u |
                (codePoint & 0x3Fu)));
    } else {
        output.push_back(
            static_cast<char>(
                0xF0u |
                (codePoint >> 18)));
        output.push_back(
            static_cast<char>(
                0x80u |
                ((codePoint >> 12) &
                 0x3Fu)));
        output.push_back(
            static_cast<char>(
                0x80u |
                ((codePoint >> 6) &
                 0x3Fu)));
        output.push_back(
            static_cast<char>(
                0x80u |
                (codePoint & 0x3Fu)));
    }
}

[[nodiscard]] std::string
Utf8Bytes(
    std::wstring_view value) {
    std::string output;
    output.reserve(
        value.size() * 2);

    for (std::size_t i = 0;
         i < value.size();
         ++i) {
        const auto unit =
            static_cast<
                std::make_unsigned_t<
                    wchar_t>>(
                        value[i]);

        std::uint32_t codePoint =
            static_cast<std::uint32_t>(
                unit);

        if (codePoint >= 0xD800u &&
            codePoint <= 0xDBFFu &&
            i + 1 < value.size()) {
            const auto low =
                static_cast<
                    std::uint32_t>(
                        value[i + 1]);

            if (low >= 0xDC00u &&
                low <= 0xDFFFu) {
                codePoint =
                    0x10000u +
                    ((codePoint -
                      0xD800u)
                     << 10) +
                    (low - 0xDC00u);
                ++i;
            } else {
                codePoint = 0xFFFDu;
            }
        } else if (
            (codePoint >= 0xDC00u &&
             codePoint <= 0xDFFFu) ||
            codePoint > 0x10FFFFu) {
            codePoint = 0xFFFDu;
        }

        AppendUtf8(
            output,
            codePoint);
    }

    return output;
}

[[nodiscard]] std::wstring
PercentEncodeUtf8(
    std::wstring_view value) {
    constexpr wchar_t kHex[] =
        L"0123456789ABCDEF";

    const auto bytes =
        Utf8Bytes(value);

    std::wstring output;
    output.reserve(
        bytes.size() * 3);

    for (const unsigned char byte :
         bytes) {
        const bool unreserved =
            (byte >= 'A' &&
             byte <= 'Z') ||
            (byte >= 'a' &&
             byte <= 'z') ||
            (byte >= '0' &&
             byte <= '9') ||
            byte == '-' ||
            byte == '.' ||
            byte == '_' ||
            byte == '~';

        if (unreserved) {
            output.push_back(
                static_cast<wchar_t>(
                    byte));
            continue;
        }

        output.push_back(L'%');
        output.push_back(
            kHex[(byte >> 4) &
                 0x0F]);
        output.push_back(
            kHex[byte & 0x0F]);
    }

    return output;
}

bool ReplaceAll(
    std::wstring& value,
    std::wstring_view token,
    std::wstring_view replacement) {
    bool replaced = false;
    std::size_t position = 0;

    while ((position =
                value.find(
                    token,
                    position)) !=
           std::wstring::npos) {
        value.replace(
            position,
            token.size(),
            replacement);

        position +=
            replacement.size();
        replaced = true;
    }

    return replaced;
}

[[nodiscard]] bool
FieldHasPlaceholder(
    std::wstring_view value) {
    return value.find(
               kRuntimeInputPlaceholder) !=
            std::wstring_view::npos ||
        value.find(
               kLegacyQueryPlaceholder) !=
            std::wstring_view::npos;
}

} // namespace

bool HasRuntimeInputPlaceholder(
    const Command& command) {
    return FieldHasPlaceholder(
               command.target) ||
        FieldHasPlaceholder(
            command.arguments) ||
        FieldHasPlaceholder(
            command.workingDirectory);
}

bool CanAcceptRuntimeInput(
    const Command& command) {
    if (command.runtimeInputMode ==
        RuntimeInputMode::None) {
        return false;
    }

    if (HasRuntimeInputPlaceholder(
            command)) {
        return true;
    }

    return command.type ==
            CommandType::Application ||
        command.type ==
            CommandType::CommandLine;
}

std::wstring EncodeRuntimeInput(
    RuntimeInputMode mode,
    std::wstring_view input) {
    switch (mode) {
    case RuntimeInputMode::UrlEncoded:
        return PercentEncodeUtf8(
            input);
    case RuntimeInputMode::Raw:
        return std::wstring(input);
    case RuntimeInputMode::None:
    default:
        return {};
    }
}

Command ResolveRuntimeInput(
    const Command& command,
    std::wstring_view input) {
    if (command.runtimeInputMode ==
        RuntimeInputMode::None) {
        return command;
    }

    Command resolved = command;

    const std::wstring encoded =
        EncodeRuntimeInput(
            command.runtimeInputMode,
            input);

    bool replaced = false;

    const auto resolveField =
        [&](std::wstring& field) {
            bool changed =
                ReplaceAll(
                    field,
                    kRuntimeInputPlaceholder,
                    encoded);

            // {query} is retained as a compatibility alias for v0.6/v0.7
            // URL search shortcuts. New UI/docs use {input}.
            changed =
                ReplaceAll(
                    field,
                    kLegacyQueryPlaceholder,
                    encoded) ||
                changed;

            replaced =
                replaced ||
                changed;
        };

    resolveField(
        resolved.target);
    resolveField(
        resolved.arguments);
    resolveField(
        resolved.workingDirectory);

    if (!replaced &&
        (resolved.type ==
             CommandType::Application ||
         resolved.type ==
             CommandType::CommandLine) &&
        !encoded.empty()) {
        if (!resolved.arguments.empty()) {
            resolved.arguments += L" ";
        }

        resolved.arguments +=
            encoded;
    }

    return resolved;
}

std::vector<LauncherResult>
BuildRuntimeInputActionResults(
    std::span<const Command> commands,
    std::wstring_view query,
    std::size_t limit) {
    std::vector<LauncherResult>
        results;

    if (limit == 0) {
        return results;
    }

    const std::wstring_view trimmed =
        Trim(query);

    if (trimmed.empty()) {
        return results;
    }

    const auto separator =
        trimmed.find_first_of(
            L" \t\r\n");

    if (separator ==
        std::wstring_view::npos) {
        return results;
    }

    const std::wstring_view token =
        trimmed.substr(
            0,
            separator);

    const std::wstring_view input =
        Trim(
            trimmed.substr(
                separator + 1));

    if (input.empty()) {
        return results;
    }

    for (std::size_t index = 0;
         index < commands.size() &&
         results.size() < limit;
         ++index) {
        const auto& command =
            commands[index];

        if (!command.enabled ||
            command.source !=
                CommandSource::User ||
            !CanAcceptRuntimeInput(
                command) ||
            !MatchesKeyword(
                command,
                token)) {
            continue;
        }

        LauncherResult result;
        result.id =
            L"runtime.input:" +
            command.id;
        result.providerId =
            "user.commands";
        result.kind =
            ResultKind::UserCommand;
        result.title =
            command.title.empty()
                ? command.keyword
                : command.title;
        result.subtitle =
            std::wstring(input);
        result.target =
            command.target;
        result.iconSource =
            command.icon.empty() ||
            command.icon == L"auto"
                ? command.target
                : command.icon;
        result.detail =
            command.target;
        result.score = 1320;
        result.action.kind =
            LauncherActionKind::
                ExecuteCommand;
        result.action.commandIndex =
            index;
        result.action.payload =
            std::wstring(input);

        results.push_back(
            std::move(result));
    }

    return results;
}

} // namespace altrun
