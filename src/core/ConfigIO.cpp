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

        return schema.get<int>();
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
    int maxSupportedSchemaVersion) {

    if (auto current =
            LoadOne(path)) {

        const int version =
            SchemaVersion(*current);

        if (version >
            maxSupportedSchemaVersion) {
            return {
                JsonLoadStatus::
                    UnsupportedSchema,
                std::move(current),
                version,
            };
        }

        return {
            JsonLoadStatus::
                LoadedPrimary,
            std::move(current),
            version,
        };
    }

    std::filesystem::path backup =
        path;

    backup += ".bak";

    auto recovered =
        LoadOne(backup);

    if (!recovered) {
        return {};
    }

    const int version =
        SchemaVersion(*recovered);

    if (version >
        maxSupportedSchemaVersion) {
        return {
            JsonLoadStatus::
                UnsupportedSchema,
            std::move(recovered),
            version,
        };
    }

    const nlohmann::json
        repairValue =
            *recovered;

    // Repair a missing/corrupt primary immediately. SaveJsonAtomic preserves
    // an existing good backup when the primary itself is invalid, so this
    // cannot replace the recovery copy with corrupt data.
    SaveJsonAtomic(
        path,
        repairValue);

    return {
        JsonLoadStatus::
            RecoveredBackup,
        std::move(recovered),
        version,
    };
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
    const nlohmann::json& value) {

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
        !validation.is_object()) {
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

        if (LoadOne(path)) {
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
