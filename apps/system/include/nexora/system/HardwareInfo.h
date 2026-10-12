#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace nexora::system {

struct GpuInfo
{
    QString card; // "card0"
    QString vendorId; // "0x8086"
    QString deviceId;
    QString driver; // from uevent DRIVER=
    QString vendorName; // empty if the vendor is not in the small built-in table
};

struct DisplayOutput
{
    QString name; // "HDMI-A-1"
    bool connected = false;
    QStringList modes;
};

struct AudioCard
{
    int index = 0;
    QString id;
    QString name;
};

struct StorageInfo
{
    QString path;
    QString device;
    QString fsType;
    qint64 totalBytes = -1;
    qint64 availableBytes = -1;
};

struct HardwareSnapshot
{
    QString cpuModel;
    int cpuThreads = 0;
    qint64 memTotalKiB = -1;
    qint64 memAvailableKiB = -1;
    QList<GpuInfo> gpus;
    QList<DisplayOutput> outputs;
    QList<AudioCard> audio;
    QList<StorageInfo> storage;
};

struct HardwareRoots
{
    QString proc = QStringLiteral("/proc");
    QString sys = QStringLiteral("/sys");
};

// "0x8086" -> "Intel" (case-insensitive); empty for an unknown vendor.
QString gpuVendorName(const QString &vendorId);

// Pure reads of proc/sys (each file at most 1 MiB; larger files are ignored). Missing or
// unreadable files give empty fields; never throws. |storagePaths| are queried with statvfs.
HardwareSnapshot probeHardware(const HardwareRoots &roots, const QStringList &storagePaths);

} // namespace nexora::system
