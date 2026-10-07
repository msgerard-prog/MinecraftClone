#include "core/FileLock.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace mc {

bool FileLock::acquire(const std::filesystem::path& path) {
    if (held()) return true;
#ifdef _WIN32
    // Deny all sharing: a second open (another instance) fails while we hold it.
    if (_wsopen_s(&m_fd, path.c_str(), _O_RDWR | _O_CREAT | _O_BINARY, _SH_DENYRW,
                  _S_IREAD | _S_IWRITE) != 0) {
        m_fd = -1;
        return false;
    }
    // Vanilla writes a snowman (U+2603, UTF-8) into session.lock.
    static const char kSnowman[] = "\xE2\x98\x83";
    _chsize_s(m_fd, 0);
    _write(m_fd, kSnowman, 3);
#else
    m_fd = ::open(path.c_str(), O_RDWR | O_CREAT, 0644);
    if (m_fd < 0) return false;
    if (::flock(m_fd, LOCK_EX | LOCK_NB) != 0) {
        ::close(m_fd);
        m_fd = -1;
        return false;
    }
#endif
    return true;
}

FileLock::~FileLock() {
    if (!held()) return;
#ifdef _WIN32
    _close(m_fd);
#else
    ::close(m_fd);
#endif
}

} // namespace mc
