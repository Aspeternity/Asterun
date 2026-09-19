#include "ConfigIO.hpp"

#include <fstream>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace altrun::config {

namespace {

std::optional<nlohmann::json> LoadOne(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return std::nullopt;

    nlohmann::json value = nlohmann::json::parse(input, nullptr, false);
    if (value.is_discarded() || !value.is_object()) return std::nullopt;
    return value;
}

bool ReplaceFile(const std::filesystem::path& temp, const std::filesystem::path& target) {
#ifdef _WIN32
    return MoveFileExW(
        temp.c_str(),
        target.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
#else
    std::error_code ec;
    std::filesystem::rename(temp, target, ec);
    return !ec;
#endif
}

} // namespace

std::optional<nlohmann::json> LoadJsonWithBackup(const std::filesystem::path& path) {
    if (auto current = LoadOne(path)) {
        return current;
    }

    std::filesystem::path backup = path;
    backup += ".bak";
    return LoadOne(backup);
}

bool SaveJsonAtomic(const std::filesystem::path& path, const nlohmann::json& value) {
    std::error_code ec;
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) return false;
    }

    const std::string serialized = value.dump(2);

    // Validate our own serialized payload before touching the live file.
    const auto validation = nlohmann::json::parse(serialized, nullptr, false);
    if (validation.is_discarded() || !validation.is_object()) return false;

    std::filesystem::path temp = path;
    temp += ".tmp";

    {
        std::ofstream output(temp, std::ios::binary | std::ios::trunc);
        if (!output) return false;
        output.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
        output.put('\n');
        output.flush();
        if (!output) return false;
    }

    // Keep one known-good previous version. Legacy source files are never
    // renamed or deleted by migration.
    if (std::filesystem::exists(path, ec) && !ec) {
        std::filesystem::path backup = path;
        backup += ".bak";
        ec.clear();
        std::filesystem::copy_file(
            path,
            backup,
            std::filesystem::copy_options::overwrite_existing,
            ec);
        if (ec) {
            std::filesystem::remove(temp, ec);
            return false;
        }
    }

    if (!ReplaceFile(temp, path)) {
        std::filesystem::remove(temp, ec);
        return false;
    }

    return true;
}

} // namespace altrun::config
