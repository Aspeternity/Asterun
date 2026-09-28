#include "core/ArchiveExtractor.hpp"

#include <miniz.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

static void Zip(const fs::path& path,
                std::string_view first, std::string_view second) {
    mz_zip_archive zip{};
    assert(mz_zip_writer_init_heap(&zip, 0, 0));
    assert(mz_zip_writer_add_mem(&zip, first.data(), "one", 3, MZ_DEFAULT_COMPRESSION));
    assert(mz_zip_writer_add_mem(&zip, second.data(), "two", 3, MZ_DEFAULT_COMPRESSION));
    void* data = nullptr;
    size_t length = 0;
    assert(mz_zip_writer_finalize_heap_archive(&zip, &data, &length));
    mz_zip_writer_end(&zip);
    std::ofstream output(path, std::ios::binary);
    output.write(static_cast<char*>(data), static_cast<std::streamsize>(length));
    output.close();
    mz_free(data);
}

int main() {
    const auto root = fs::temp_directory_path() / "Asterun-ArchiveExtractorTests";
    std::error_code error;
    fs::remove_all(root, error);
    fs::create_directories(root);
    const auto archive = fs::absolute(root / "good.zip");
    Zip(archive, "dict/mandarin/word.txt", "Asterun.exe");
    assert(altrun::ExtractArchive(archive, fs::absolute(root / "good"), error));
    assert(fs::file_size(root / "good" / "Asterun.exe") == 3);
    assert(fs::file_size(root / "good" / "dict" / "mandarin" / "word.txt") == 3);
    assert(!altrun::ExtractArchive(archive, fs::absolute(root / "good"), error));

    const auto bad = fs::absolute(root / "escape.zip");
    Zip(bad, "safe.txt", "../escape.txt");
    assert(!altrun::ExtractArchive(bad, fs::absolute(root / "escape"), error));
    assert(!fs::exists(root / "escape"));
    assert(!fs::exists(root / "escape.txt"));

    const auto outside = root / "outside";
    fs::create_directories(outside);
    const auto linked = root / "linked";
    error.clear();
    fs::create_directory_symlink(outside, linked, error);
    if (!error) {
        assert(!altrun::ExtractArchive(archive, fs::absolute(linked), error));
        assert(!fs::exists(outside / "one"));
    }

    const auto crc = fs::absolute(root / "corrupt.zip");
    Zip(crc, "first.txt", "second.txt");
    {
        std::fstream io(crc, std::ios::in | std::ios::out | std::ios::binary);
        io.seekp(40);
        io.put('x');
    }
    assert(!altrun::ExtractArchive(crc, fs::absolute(root / "corrupt"), error));
    std::stop_source stop;
    stop.request_stop();
    assert(!altrun::ExtractArchive(archive, fs::absolute(root / "cancel"), error, stop.get_token()));
    assert(error == std::errc::operation_canceled);
    fs::remove_all(root, error);
}
