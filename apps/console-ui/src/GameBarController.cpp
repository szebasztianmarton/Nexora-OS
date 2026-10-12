#include "GameBarController.h"

#include "AppController.h"
#include "input/InputController.h"

#include "nexora/shared/Logging.h"

#include <cmath>

namespace nexora::ui {

GameBarController::GameBarController(AppController *app, system::SystemIntegration *system,
                                     input::InputController *input, QObject *parent)
    : QObject(parent)
    , m_app(app)
    , m_system(system)
{
    m_sampleTimer.setInterval(1000);
    connect(&m_sampleTimer, &QTimer::timeout, this, &GameBarController::refresh);
    if (input)
        connect(input, &input::InputController::gameBarRequested, this, &GameBarController::toggle);
    if (m_app) {
        connect(m_app, &AppController::stateChanged, this, &GameBarController::onAppStateChanged);
        onAppStateChanged();
    }
}

void GameBarController::setOpen(bool open)
{
    if (m_open == open)
        return;
    m_open = open;
    qCInfo(lcUi) << "Game bar" << (open ? "opened" : "closed");
    if (open) {
        // The first figure needs two CPU samples; the baseline is taken now.
        m_lastCpu = {};
        m_cpuPercent = -1;
        m_cpuHistory.clear();
        refresh();
        m_sampleTimer.start();
    } else {
        m_sampleTimer.stop();
    }
    if (m_app)
        m_app->setGameBarOpen(open);
    emit openChanged();
}

int GameBarController::sessionSeconds() const
{
    if (!m_app || !m_app->isBusy() || !m_session.isValid())
        return -1;
    return int(m_session.elapsed() / 1000);
}

void GameBarController::refresh()
{
    const system::CpuTimes cpu = m_system->cpuTimes();
    if (cpu.valid && m_lastCpu.valid && cpu.total > m_lastCpu.total) {
        const double total = double(cpu.total - m_lastCpu.total);
        const double idle = cpu.idle >= m_lastCpu.idle ? double(cpu.idle - m_lastCpu.idle) : 0.0;
        m_cpuPercent = qBound(0, int(std::lround(100.0 * (1.0 - idle / total))), 100);
        m_cpuHistory.append(m_cpuPercent);
        while (m_cpuHistory.size() > kHistoryLength)
            m_cpuHistory.removeFirst();
    }
    m_lastCpu = cpu;

    const system::SystemStatus status = m_system->status();
    if (status.memoryTotalKiB > 0 && status.memoryAvailableKiB >= 0) {
        const qint64 used = qMax<qint64>(0, status.memoryTotalKiB - status.memoryAvailableKiB);
        m_memoryTotalMiB = int(status.memoryTotalKiB / 1024);
        m_memoryUsedMiB = int(used / 1024);
        m_memoryPercent = int(std::lround(100.0 * double(used) / double(status.memoryTotalKiB)));
    } else {
        m_memoryTotalMiB = m_memoryUsedMiB = m_memoryPercent = -1;
    }
    m_load1 = status.load1;
    m_device = m_system->deviceStatus();
    emit statsChanged();
}

void GameBarController::onAppStateChanged()
{
    if (!m_app || !m_app->isBusy()) {
        m_session.invalidate();
    } else if (!m_session.isValid()) {
        m_session.start();
    }
    emit statsChanged();
}

} // namespace nexora::ui
