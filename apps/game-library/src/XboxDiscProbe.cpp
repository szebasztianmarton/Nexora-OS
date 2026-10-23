#include "nexora/library/XboxDiscProbe.h"

#include <QFile>

namespace nexora::library {

namespace {

constexpr char Magic[] = "MICROSOFT*XBOX*MEDIA";
constexpr qint64 MagicSize = sizeof(Magic) - 1;
constexpr qint64 SignatureOffset = 0x10000;
constexpr qint64 PartitionBases[] = {0x0, 0x2080000, 0xFD90000};

} // namespace

bool isXboxDisc(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const qint64 size = file.size();
    for (const qint64 base : PartitionBases) {
        const qint64 at = base + SignatureOffset;
        if (at + MagicSize > size)
            continue;
        if (!file.seek(at))
            return false;
        const QByteArray bytes = file.read(MagicSize);
        if (bytes.size() != MagicSize)
            return false;
        if (bytes == QByteArrayView(Magic, MagicSize))
            return true;
    }
    return false;
}

} // namespace nexora::library
