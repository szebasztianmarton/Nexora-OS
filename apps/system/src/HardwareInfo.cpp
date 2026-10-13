#include "nexora/system/HardwareInfo.h"

#include "nexora/shared/AtomicFile.h"

#include <QDir>
#include <QRegularExpression>
#include <QStorageInfo>

#include <algorithm>

namespace nexora::system {

namespace {

constexpr qint64 MaxFileBytes = 1024 * 1024;

QString readText(const QString &path)
{
    const auto data = shared::readFileLimited(path, MaxFileBytes, nullptr);
    return data ? QString::fromUtf8(*data) : QString();
}

QStringList readLines(const QString &path)
{
    const QString text = readText(path);
    return text.isEmpty() ? QStringList() : text.split(u'\n');
}

void readCpu(const QString &procRoot, HardwareSnapshot &out)
{
    static const QStringList modelKeys{QStringLiteral("model name"), QStringLiteral("Model"),
                                       QStringLiteral("Hardware"), QStringLiteral("cpu model")};
    for (const QString &line : readLines(procRoot + QStringLiteral("/cpuinfo"))) {
        const qsizetype colon = line.indexOf(u':');
        if (colon < 0)
            continue;
        const QString key = line.left(colon).trimmed();
        const QString value = line.mid(colon + 1).trimmed();
        if (key == QLatin1String("processor"))
            ++out.cpuThreads;
        else if (out.cpuModel.isEmpty() && modelKeys.contains(key))
            out.cpuModel = value;
    }
}

void readMemory(const QString &procRoot, HardwareSnapshot &out)
{
    for (const QString &line : readLines(procRoot + QStringLiteral("/meminfo"))) {
        const bool total = line.startsWith(QLatin1String("MemTotal:"));
        const bool available = line.startsWith(QLatin1String("MemAvailable:"));
        if (!total && !available)
            continue;
        bool ok = false;
        const qint64 kib = line.section(u':', 1).trimmed().section(u' ', 0, 0).toLongLong(&ok);
        if (!ok)
            continue;
        (total ? out.memTotalKiB : out.memAvailableKiB) = kib;
    }
}

void readDrm(const QString &sysRoot, HardwareSnapshot &out)
{
    const QString drm = sysRoot + QStringLiteral("/class/drm");
    static const QRegularExpression cardRe(QStringLiteral("^card\\d+$"));
    static const QRegularExpression outputRe(QStringLiteral("^card\\d+-(.+)$"));

    QStringList names = QDir(drm).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    std::sort(names.begin(), names.end());
    for (const QString &name : names) {
        const QString dir = drm + u'/' + name;
        if (cardRe.match(name).hasMatch()) {
            GpuInfo gpu;
            gpu.card = name;
            gpu.vendorId = readText(dir + QStringLiteral("/device/vendor")).trimmed();
            gpu.deviceId = readText(dir + QStringLiteral("/device/device")).trimmed();
            for (const QString &line : readLines(dir + QStringLiteral("/device/uevent"))) {
                if (line.startsWith(QLatin1String("DRIVER=")))
                    gpu.driver = line.mid(7).trimmed();
            }
            gpu.vendorName = gpuVendorName(gpu.vendorId);
            out.gpus.append(gpu);
            continue;
        }
        const QRegularExpressionMatch m = outputRe.match(name);
        if (!m.hasMatch())
            continue;
        DisplayOutput output;
        output.name = m.captured(1);
        output.connected = readText(dir + QStringLiteral("/status")).trimmed() == QLatin1String("connected");
        for (const QString &mode : readLines(dir + QStringLiteral("/modes"))) {
            if (!mode.trimmed().isEmpty())
                output.modes.append(mode.trimmed());
        }
        out.outputs.append(output);
    }
}

void readAudio(const QString &procRoot, HardwareSnapshot &out)
{
    // " 0 [PCH            ]: HDA-Intel - HDA Intel PCH"
    static const QRegularExpression cardRe(QStringLiteral("^\\s*(\\d+)\\s+\\[([^\\]]*?)\\s*\\]:\\s*(.*)$"));
    for (const QString &line : readLines(procRoot + QStringLiteral("/asound/cards"))) {
        const QRegularExpressionMatch m = cardRe.match(line);
        if (!m.hasMatch())
            continue;
        AudioCard card;
        card.index = m.captured(1).toInt();
        card.id = m.captured(2);
        const QString rest = m.captured(3).trimmed();
        const qsizetype dash = rest.indexOf(QLatin1String(" - "));
        card.name = dash >= 0 ? rest.mid(dash + 3).trimmed() : rest;
        out.audio.append(card);
    }
}

} // namespace

QString gpuVendorName(const QString &vendorId)
{
    const QString id = vendorId.trimmed().toLower();
    if (id == QLatin1String("0x8086"))
        return QStringLiteral("Intel");
    if (id == QLatin1String("0x1002"))
        return QStringLiteral("AMD");
    if (id == QLatin1String("0x10de"))
        return QStringLiteral("NVIDIA");
    if (id == QLatin1String("0x1af4"))
        return QStringLiteral("virtio");
    if (id == QLatin1String("0x1234"))
        return QStringLiteral("QEMU std VGA");
    return {};
}

HardwareSnapshot probeHardware(const HardwareRoots &roots, const QStringList &storagePaths)
{
    HardwareSnapshot out;
    readCpu(roots.proc, out);
    readMemory(roots.proc, out);
    readDrm(roots.sys, out);
    readAudio(roots.proc, out);

    for (const QString &path : storagePaths) {
        StorageInfo info;
        info.path = path;
        const QStorageInfo storage(path);
        if (storage.isValid() && storage.isReady()) {
            info.device = QString::fromUtf8(storage.device());
            info.fsType = QString::fromUtf8(storage.fileSystemType());
            info.totalBytes = storage.bytesTotal();
            info.availableBytes = storage.bytesAvailable();
        }
        out.storage.append(info);
    }
    return out;
}

} // namespace nexora::system
