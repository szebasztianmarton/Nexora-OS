#include "nexora/library/XboxDiscProbe.h"

#include <QFile>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace nexora::library {

namespace {

constexpr char Magic[] = "MICROSOFT*XBOX*MEDIA";
constexpr qint64 MagicSize = sizeof(Magic) - 1;
constexpr qint64 SignatureOffset = 0x10000;
constexpr qint64 PartitionBases[] = {0x0, 0x2080000, 0xFD90000};

// Closes the descriptor on every return path.
class FileDescriptor
{
public:
    explicit FileDescriptor(int fd)
        : m_fd(fd)
    { }
    ~FileDescriptor()
    {
        if (m_fd >= 0)
            ::close(m_fd);
    }
    FileDescriptor(const FileDescriptor &) = delete;
    FileDescriptor &operator=(const FileDescriptor &) = delete;

private:
    int m_fd;
};

} // namespace

bool isXboxDisc(const QString &path)
{
    // O_NONBLOCK so that a FIFO swapped in for the image cannot block the open.
    const int fd = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
        return false;
    const FileDescriptor guard(fd);
    struct stat st = {};
    if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode))
        return false;
    const qint64 size = st.st_size;
    for (const qint64 base : PartitionBases) {
        const qint64 at = base + SignatureOffset;
        if (at + MagicSize > size)
            continue;
        char bytes[MagicSize];
        if (::pread(fd, bytes, MagicSize, at) != MagicSize)
            return false;
        if (QByteArrayView(bytes, MagicSize) == QByteArrayView(Magic, MagicSize))
            return true;
    }
    return false;
}

} // namespace nexora::library
