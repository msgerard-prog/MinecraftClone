#pragma once

#include <filesystem>

namespace mc {

// An exclusive lock on a file, held while the object lives (vanilla's
// session.lock: one game instance per world). Other processes can't open the file
// while it is held.
class FileLock {
public:
    FileLock() = default;
    ~FileLock();
    FileLock(const FileLock&) = delete;
    FileLock& operator=(const FileLock&) = delete;

    // Creates the file if needed and locks it; false if another process holds it.
    bool acquire(const std::filesystem::path& path);
    bool held() const { return m_fd >= 0; }

private:
    int m_fd = -1;
};

} // namespace mc
