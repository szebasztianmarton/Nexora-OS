#pragma once

#include "nexora/shared/EmulatorConfig.h"

#include <QDateTime>
#include <QString>

namespace nexora::emulator {

enum class Health { Available, Disabled, ExecutableMissing, NotExecutable, InvalidConfig };

struct HealthReport
{
    Health health = Health::InvalidConfig;
    QString detail;
    QDateTime checkedAt;
};

// Inspects the configured executable with stat() only; the emulator is never run.
HealthReport checkHealth(const shared::EmulatorConfig &config);

QString nameOf(Health health);

} // namespace nexora::emulator
