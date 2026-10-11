#include "tesseract/open_media.h"

#include "tesseract/client.h"
#include "tesseract/paths.h"

#include <cstdio>
#include <exception>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#if defined(__APPLE__)
#include <sys/xattr.h>
#include <ctime>
#endif
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace tesseract
{

fs::path open_media_dir()
{
    return cache_dir() / "open-media";
}

namespace
{

std::string random_hex(std::size_t bytes)
{
    static constexpr char kHex[] = "0123456789abcdef";
    std::random_device rd;
    std::string out;
    out.reserve(bytes * 2);
    for (std::size_t i = 0; i < bytes; ++i)
    {
        const unsigned v = rd() & 0xFFu;
        out += kHex[v >> 4];
        out += kHex[v & 0xF];
    }
    return out;
}

bool make_private_dir(const fs::path& p)
{
    std::error_code ec;
    fs::create_directories(p, ec);
    if (ec)
        return false;
#if !defined(_WIN32)
    ::chmod(p.c_str(), 0700);
#endif
    return true;
}

bool is_inside(const fs::path& root, const fs::path& candidate)
{
    std::error_code ec;
    const fs::path r = fs::weakly_canonical(root, ec);
    if (ec)
        return false;
    const fs::path c = fs::weakly_canonical(candidate, ec);
    if (ec)
        return false;
    auto ri = r.begin();
    auto ci = c.begin();
    for (; ri != r.end(); ++ri, ++ci)
    {
        if (ci == c.end() || *ri != *ci)
            return false;
    }
    return ci != c.end();
}

} // namespace

std::optional<fs::path> write_open_media_file(const std::string& filename,
                                              const std::vector<std::uint8_t>& bytes)
{
    if (filename.empty() || filename == "." || filename == ".." ||
        filename.find_first_of("/\\:") != std::string::npos) // ':' = NTFS stream
        return std::nullopt;
    for (unsigned char c : filename)
    {
        if (c < 0x20 || c == 0x7F)
            return std::nullopt;
    }

    const fs::path root = open_media_dir();
    if (!make_private_dir(root))
        return std::nullopt;
    const fs::path dir = root / random_hex(8);
    if (!make_private_dir(dir))
        return std::nullopt;
    fs::path file;
    try
    {
        file = dir / fs::path(std::u8string(filename.begin(), filename.end()));
    }
    catch (const std::exception&) // not convertible to a native path (bad UTF-8)
    {
        return std::nullopt;
    }

#if defined(_WIN32)
    std::ofstream f(file, std::ios::binary | std::ios::trunc);
    if (!f)
        return std::nullopt;
    f.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    f.close();
    if (!f)
        return std::nullopt;
    // Mark as downloaded from the Internet so the shell applies its
    // untrusted-file protections (SmartScreen / Protected View).
    {
        std::ofstream zone(file.native() + L":Zone.Identifier", std::ios::binary);
        zone << "[ZoneTransfer]\r\nZoneId=3\r\n";
    }
#else
    const int fd = ::open(file.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (fd < 0)
        return std::nullopt;
    const std::uint8_t* p = bytes.data();
    std::size_t left = bytes.size();
    while (left > 0)
    {
        const ssize_t n = ::write(fd, p, left);
        if (n < 0)
        {
            ::close(fd);
            return std::nullopt;
        }
        p += n;
        left -= static_cast<std::size_t>(n);
    }
    if (::close(fd) != 0)
        return std::nullopt;
#if defined(__APPLE__)
    // Mark as downloaded so Gatekeeper treats it like a browser download
    // (quarantine flags;hex timestamp;agent name). Best-effort.
    {
        char q[64];
        std::snprintf(q, sizeof q, "0081;%lx;Tesseract;",
                      static_cast<unsigned long>(std::time(nullptr)));
        ::setxattr(file.c_str(), "com.apple.quarantine", q, std::strlen(q), 0, 0);
    }
#endif
#endif
    return file;
}

void purge_open_media_dir()
{
    std::error_code ec;
    const fs::path root = open_media_dir();
    if (!fs::is_directory(root, ec))
        return;
    for (const auto& entry : fs::directory_iterator(root, ec))
    {
        std::error_code rec;
        fs::remove_all(entry.path(), rec); // locked files are skipped
    }
}

bool open_local_file(const fs::path& path)
{
    std::error_code ec;
    if (!path.is_absolute() || !is_inside(open_media_dir(), path))
        return false;
    const auto st = fs::symlink_status(path, ec);
    if (ec || st.type() != fs::file_type::regular)
        return false;

    // Defence in depth: callers already vet the file, but never hand anything
    // that can run code to the OS launcher, whoever asks.
    {
        std::vector<std::uint8_t> head(8192);
        std::ifstream in(path, std::ios::binary);
        in.read(reinterpret_cast<char*>(head.data()),
                static_cast<std::streamsize>(head.size()));
        head.resize(static_cast<std::size_t>(in.gcount()));
        // u8string, not string(): on Windows string() throws for names with
        // characters the ANSI code page can't hold, and the name is the sender's.
        const auto u8 = path.filename().u8string();
        const auto chk = Client::check_media_file(
            std::string(u8.begin(), u8.end()), "", head);
        if (chk.verdict == Client::MediaTypeVerdict::Executable)
            return false;
    }

#if defined(_WIN32)
    HINSTANCE hi = ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr,
                                 SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(hi) > 32;
#else
#if defined(__APPLE__)
    const char* opener = "open";
#else
    const char* opener = "xdg-open";
#endif
    const std::string arg = path.string();
    // Double-fork so the UI thread never waits on the viewer; see
    // Client::open_in_browser for the same pattern.
    pid_t pid = fork();
    if (pid < 0)
        return false;
    if (pid == 0)
    {
        pid_t inner = fork();
        if (inner == 0)
        {
            execlp(opener, opener, arg.c_str(), static_cast<const char*>(nullptr));
            _exit(127);
        }
        _exit(inner < 0 ? 1 : 0);
    }
    int status = 0;
    return waitpid(pid, &status, 0) == pid && WIFEXITED(status) &&
           WEXITSTATUS(status) == 0;
#endif
}

} // namespace tesseract
