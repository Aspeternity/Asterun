#include "ArchiveExtractor.hpp"

#include <miniz.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <new>
#include <string>
#include <unordered_set>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace altrun {
namespace {
constexpr std::uint64_t kMaxArchive = 128ULL * 1024 * 1024;
constexpr std::uint64_t kMaxExpanded = 512ULL * 1024 * 1024;
constexpr mz_uint kMaxEntries = 20000;

bool PlainPath(const std::filesystem::path& path) {
    std::error_code ec;
    auto current = std::filesystem::absolute(path, ec);
    if (ec) return false;
    while (!current.empty()) {
#ifdef _WIN32
        const auto attributes = GetFileAttributesW(current.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            const auto reason = GetLastError();
            if (reason != ERROR_FILE_NOT_FOUND &&
                reason != ERROR_PATH_NOT_FOUND) return false;
        }
        if (attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
#else
        const auto status = std::filesystem::symlink_status(current, ec);
        if (ec && ec != std::errc::no_such_file_or_directory) return false;
        if (std::filesystem::is_symlink(status)) return false;
        ec.clear();
#endif
        const auto parent = current.parent_path();
        if (parent == current) break;
        current = parent;
    }
    return true;
}

bool SafeMember(std::string_view name) {
    if (name.empty() || name.front() == '/' || name.front() == '\\') return false;
    std::size_t start = 0;
    while (start < name.size()) {
        const auto end = name.find('/', start);
        const auto part = name.substr(start, end == name.npos ? name.size() - start : end - start);
        if (part.empty() || part == "." || part == ".." || part.back() == '.' || part.back() == ' ')
            return false;
        for (unsigned char ch : part) if (ch < 32 || ch == 127 || std::string_view("\\:*?\"<>|").find(ch) != std::string_view::npos) return false;
        std::string stem(part.substr(0, part.find('.')));
        std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL" ||
            (stem.size() == 4 && (stem.starts_with("COM") || stem.starts_with("LPT")) &&
             stem[3] >= '0' && stem[3] <= '9')) return false;
        if (end == name.npos) break;
        start = end + 1;
    }
    return true;
}

struct Reader {
    std::ifstream stream;
    mz_zip_archive zip{};
    bool opened{false};
    ~Reader() { if (opened) mz_zip_reader_end(&zip); }
};

struct Output {
    std::ofstream stream;
    std::stop_token stop;
    std::uint64_t expected{};
    std::uint64_t written{};
};
}

bool ExtractArchive(const std::filesystem::path& archive,
    const std::filesystem::path& destination, std::error_code& error,
    std::stop_token stop) {
    try {
    error.clear();
    const auto fail = [&](std::errc reason) { error = std::make_error_code(reason); return false; };
    if (stop.stop_requested()) return fail(std::errc::operation_canceled);
    if (!archive.is_absolute() || !destination.is_absolute() ||
        !PlainPath(archive) || !PlainPath(destination)) return fail(std::errc::invalid_argument);
    const auto size = std::filesystem::file_size(archive, error);
    if (error) return false;
    if (size == 0 || size > kMaxArchive) return fail(std::errc::file_too_large);
    Reader reader;
    reader.stream.open(archive, std::ios::binary);
    if (!reader.stream) return fail(std::errc::io_error);
    reader.zip.m_pIO_opaque = &reader;
    reader.zip.m_pRead = [](void* opaque, mz_uint64 offset, void* buffer, size_t count) -> size_t {
        auto& input = static_cast<Reader*>(opaque)->stream;
        input.clear();
        input.seekg(static_cast<std::streamoff>(offset));
        input.read(static_cast<char*>(buffer), static_cast<std::streamsize>(count));
        return static_cast<size_t>(input.gcount());
    };
    if (!mz_zip_reader_init(&reader.zip, size, 0)) return fail(std::errc::illegal_byte_sequence);
    reader.opened = true;
    const auto count = mz_zip_reader_get_num_files(&reader.zip);
    if (count == 0 || count > kMaxEntries) return fail(std::errc::file_too_large);
    std::uint64_t expanded = 0;
    std::unordered_set<std::string> names;
    struct Entry { std::filesystem::path relative; mz_zip_archive_file_stat stat; };
    std::vector<Entry> entries;
    entries.reserve(count);
    // Validate the entire directory before creating any output.
    for (mz_uint index = 0; index < count; ++index) {
        if (stop.stop_requested()) return fail(std::errc::operation_canceled);
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&reader.zip, index, &stat) || stat.m_is_encrypted ||
            !stat.m_is_supported) return fail(std::errc::illegal_byte_sequence);
        const auto length = mz_zip_reader_get_filename(&reader.zip, index, nullptr, 0);
        if (length < 2 || length > 32768) return fail(std::errc::filename_too_long);
        std::string name(length, '\0');
        mz_zip_reader_get_filename(&reader.zip, index, name.data(), length);
        name.pop_back();
        if (name.find('\0') != name.npos || !SafeMember(name)) return fail(std::errc::invalid_argument);
        const auto kind = (stat.m_external_attr >> 16) & 0170000;
        if (kind != 0 && kind != 0100000 && kind != 0040000) return fail(std::errc::invalid_argument);
        if (stat.m_uncomp_size > kMaxExpanded - expanded) return fail(std::errc::file_too_large);
        expanded += stat.m_uncomp_size;
        if (name.back() == '/') name.pop_back();
        auto key = name;
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (!names.insert(key).second) return fail(std::errc::file_exists);
#ifdef _WIN32
        if (name.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            return fail(std::errc::filename_too_long);
        const int chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            name.data(), static_cast<int>(name.size()), nullptr, 0);
        if (chars == 0) return fail(std::errc::illegal_byte_sequence);
        std::wstring wide(static_cast<std::size_t>(chars), L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.data(),
            static_cast<int>(name.size()), wide.data(), chars) != chars)
            return fail(std::errc::illegal_byte_sequence);
        entries.push_back({std::filesystem::path(wide), stat});
#else
        entries.push_back({std::filesystem::path(std::u8string(
            reinterpret_cast<const char8_t*>(name.data()), name.size())), stat});
#endif
    }
    std::filesystem::create_directories(destination, error);
    if (error) return false;
    for (const auto& entry : entries) {
        if (stop.stop_requested()) return fail(std::errc::operation_canceled);
        const auto path = destination / entry.relative;
        if (!PlainPath(path)) return fail(std::errc::permission_denied);
        if (entry.stat.m_is_directory) {
            std::filesystem::create_directories(path, error);
            if (error) return false;
            continue;
        }
        if (std::filesystem::exists(path, error) || error) return fail(std::errc::file_exists);
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) return false;
        Output output{std::ofstream(path, std::ios::binary), stop, entry.stat.m_uncomp_size, 0};
        if (!output.stream) return fail(std::errc::io_error);
        const bool ok = mz_zip_reader_extract_to_callback(&reader.zip, entry.stat.m_file_index,
            [](void* opaque, mz_uint64 offset, const void* buffer, size_t bytes) -> size_t {
                auto& out = *static_cast<Output*>(opaque);
                if (out.stop.stop_requested() || offset != out.written || bytes > out.expected - out.written) return 0;
                out.stream.write(static_cast<const char*>(buffer), static_cast<std::streamsize>(bytes));
                if (!out.stream) return 0;
                out.written += bytes;
                return bytes;
            }, &output, 0) != 0;
        output.stream.flush();
        const bool written = output.stream.good() && output.written == output.expected;
        output.stream.close();
        if (!ok || !written || output.stream.fail()) return fail(stop.stop_requested() ?
            std::errc::operation_canceled : std::errc::io_error);
    }
    return true;
    } catch (const std::filesystem::filesystem_error& exception) {
        error = exception.code();
    } catch (const std::bad_alloc&) {
        error = std::make_error_code(std::errc::not_enough_memory);
    } catch (...) {
        error = std::make_error_code(std::errc::io_error);
    }
    return false;
}
}
