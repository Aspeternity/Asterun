#include "ConfigIO.hpp"

#include <fstream>
#include <limits>
#include <string>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace altrun::config {

namespace {

std::optional<nlohmann::json>
LoadOne(
    const std::filesystem::path& path) {

    std::ifstream input(
        path,
        std::ios::binary);

    if (!input) {
        return std::nullopt;
    }

    nlohmann::json value =
        nlohmann::json::parse(
            input,
            nullptr,
            false);

    if (value.is_discarded() ||
        !value.is_object()) {
        return std::nullopt;
    }

    return value;
}

int SchemaVersion(
    const nlohmann::json& value) {

    try {
        if (!value.contains(
                "schemaVersion")) {
            return 0;
        }

        const auto& schema =
            value["schemaVersion"];

        if (!schema.is_number_integer()) {
            return 0;
        }

        if (schema.is_number_unsigned() && schema.get<std::uint64_t>() >
            static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            return std::numeric_limits<int>::max();
        }
        const auto version = schema.get<std::int64_t>();
        if (version < 0 || version > std::numeric_limits<int>::max()) {
            return std::numeric_limits<int>::max();
        }
        return static_cast<int>(version);
    } catch (...) {
        return 0;
    }
}

bool ReplaceFile(
    const std::filesystem::path& temp,
    const std::filesystem::path& target) {

#ifdef _WIN32
    return MoveFileExW(
        temp.c_str(),
        target.c_str(),
        MOVEFILE_REPLACE_EXISTING |
            MOVEFILE_WRITE_THROUGH) != FALSE;
#else
    std::error_code ec;

    std::filesystem::rename(
        temp,
        target,
        ec);

    return !ec;
#endif
}

} // namespace

JsonLoadResult
LoadJsonWithBackup(
    const std::filesystem::path& path,
    int maxSupportedSchemaVersion,
    JsonValidator validator) {

    auto current = LoadOne(path);
    if (current) {
        const int version = SchemaVersion(*current);
        if (version > maxSupportedSchemaVersion) {
            return {JsonLoadStatus::UnsupportedSchema, std::move(current), version};
        }
        if (!validator || validator(*current)) {
            return {JsonLoadStatus::LoadedPrimary, std::move(current), version};
        }
    }

    auto backup = path;
    backup += ".bak";
    auto recovered = LoadOne(backup);
    if (recovered) {
        const int version = SchemaVersion(*recovered);
        if (version > maxSupportedSchemaVersion) {
            return {JsonLoadStatus::UnsupportedSchema, std::move(recovered), version};
        }
        if (!validator || validator(*recovered)) {
            // Only a semantically valid primary may replace the good backup.
            const bool repaired = SaveJsonAtomic(path, *recovered, validator);
            return {JsonLoadStatus::RecoveredBackup, std::move(recovered), version, repaired};
        }
    }

    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);
    if (exists || ec || std::filesystem::exists(backup, ec) || ec) {
        // A caller may salvage valid records for this session, but must not
        // default-save over either recovery input.
        return {JsonLoadStatus::InvalidExisting, std::move(current), 0};
    }
    return {};
}

std::optional<nlohmann::json>
LoadJsonWithBackup(
    const std::filesystem::path& path) {

    auto result =
        LoadJsonWithBackup(
            path,
            std::numeric_limits<int>::max());

    return std::move(
        result.value);
}

bool SaveJsonAtomic(
    const std::filesystem::path& path,
    const nlohmann::json& value,
    JsonValidator validator) {

    std::error_code ec;

    const auto parent =
        path.parent_path();

    if (!parent.empty()) {
        std::filesystem::
            create_directories(
                parent,
                ec);

        if (ec) {
            return false;
        }
    }

    const std::string serialized =
        value.dump(2);

    // Validate our own serialized payload before touching the live file.
    const auto validation =
        nlohmann::json::parse(
            serialized,
            nullptr,
            false);

    if (validation.is_discarded() ||
        !validation.is_object() ||
        (validator && !validator(validation))) {
        return false;
    }

    std::filesystem::path temp =
        path;

    temp += ".tmp";

    {
        std::ofstream output(
            temp,
            std::ios::binary |
                std::ios::trunc);

        if (!output) {
            return false;
        }

        output.write(
            serialized.data(),
            static_cast<
                std::streamsize>(
                    serialized.size()));

        output.put('\n');
        output.flush();

        if (!output) {
            return false;
        }
    }

    // Keep one known-good previous version. A corrupt primary must never
    // replace a valid .bak during recovery/self-healing.
    if (std::filesystem::exists(
            path,
            ec) &&
        !ec) {

        const auto previous = LoadOne(path);
        if (previous && (!validator || validator(*previous))) {
            std::filesystem::path
                backup = path;

            backup += ".bak";

            ec.clear();

            std::filesystem::copy_file(
                path,
                backup,
                std::filesystem::
                    copy_options::
                        overwrite_existing,
                ec);

            if (ec) {
                std::filesystem::remove(
                    temp,
                    ec);

                return false;
            }
        }
    }

    if (!ReplaceFile(
            temp,
            path)) {

        std::filesystem::remove(
            temp,
            ec);

        return false;
    }

    return true;
}

} // namespace altrun::config
