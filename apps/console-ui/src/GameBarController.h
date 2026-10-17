#pragma once

#include "nexora/system/SystemIntegration.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace nexora::input {
class InputController;
}

namespace nexora::ui {

class AppController;

// State behind the game bar: a quick overlay (Guide button, Ctrl+G) with widgets for the
// running game, performance, audio and power, available on top of a running emulator.
//
// Performance figures are only sampled while the bar is open, so a closed bar costs nothing.
//
// Also owns the in-game quick menu state: while an emulator runs, the controller's Guide
// button (gameBarRequested) or holding Start for a second (quickMenuRequested) opens the
// quick menu instead of the bar; Guide again closes it. With no emulator running Guide
// toggles the bar as before. Ctrl+G (QML, calls toggle()) always toggles the bar. Either
// overlay being open keeps controller input flowing to the shell (AppController).
class GameBarController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the application")

    Q_PROPERTY(bool open READ isOpen WRITE setOpen NOTIFY openChanged)
    Q_PROPERTY(bool quickMenuOpen READ isQuickMenuOpen NOTIFY quickMenuOpenChanged)
    Q_PROPERTY(int cpuPercent READ cpuPercent NOTIFY statsChanged)
    Q_PROPERTY(QVariantList cpuHistory READ cpuHistory NOTIFY statsChanged)
    Q_PROPERTY(int memoryPercent READ memoryPercent NOTIFY statsChanged)
    Q_PROPERTY(int memoryUsedMiB READ memoryUsedMiB NOTIFY statsChanged)
    Q_PROPERTY(int memoryTotalMiB READ memoryTotalMiB NOTIFY statsChanged)
    Q_PROPERTY(double load1 READ load1 NOTIFY statsChanged)
    Q_PROPERTY(int sessionSeconds READ sessionSeconds NOTIFY statsChanged)
    Q_PROPERTY(int historyLength READ historyLength CONSTANT)
    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY statsChanged)
    Q_PROPERTY(bool batteryCharging READ batteryCharging NOTIFY statsChanged)
    Q_PROPERTY(QString network READ network NOTIFY statsChanged)

public:
    static constexpr int kHistoryLength = 30;

    GameBarController(AppController *app, system::SystemIntegration *system, input::InputController *input,
                      QObject *parent = nullptr);

    bool isOpen() const { return m_open; }
    void setOpen(bool open);
    Q_INVOKABLE void toggle() { setOpen(!m_open); }
    Q_INVOKABLE void close() { setOpen(false); }

    bool isQuickMenuOpen() const { return m_quickMenuOpen; }
    // Opens the quick menu (closing the bar) when an emulator is active; false otherwise.
    Q_INVOKABLE bool openQuickMenu();
    Q_INVOKABLE void closeQuickMenu();
    // The controller's Guide button: see the class comment.
    void onGuidePressed();

    // -1 while unknown (first sample after opening, or no /proc).
    int cpuPercent() const { return m_cpuPercent; }
    // Oldest first, at most kHistoryLength entries.
    QVariantList cpuHistory() const { return m_cpuHistory; }
    int historyLength() const { return kHistoryLength; }
    int memoryPercent() const { return m_memoryPercent; }
    int memoryUsedMiB() const { return m_memoryUsedMiB; }
    int memoryTotalMiB() const { return m_memoryTotalMiB; }
    double load1() const { return m_load1; }
    // Status icons: -1 / "none" when there is no battery / no network link.
    int batteryPercent() const { return m_device.batteryPercent; }
    bool batteryCharging() const { return m_device.batteryCharging; }
    QString network() const { return m_device.network; }
    // How long the current emulator has been running; -1 when none runs.
    int sessionSeconds() const;

    // Takes a sample now (also done once per interval while open).
    Q_INVOKABLE void refresh();
    void setSampleInterval(int milliseconds) { m_sampleTimer.setInterval(milliseconds); }

signals:
    void openChanged();
    void quickMenuOpenChanged();
    void statsChanged();

private:
    void onAppStateChanged();
    void setQuickMenuOpen(bool open);
    void updateAppOverlayState();

    QPointer<AppController> m_app;
    system::SystemIntegration *m_system;
    QTimer m_sampleTimer;
    QElapsedTimer m_session;
    system::CpuTimes m_lastCpu;
    system::DeviceStatus m_device;
    QVariantList m_cpuHistory;
    bool m_open = false;
    bool m_quickMenuOpen = false;
    int m_cpuPercent = -1;
    int m_memoryPercent = -1;
    int m_memoryUsedMiB = -1;
    int m_memoryTotalMiB = -1;
    double m_load1 = -1;
};

} // namespace nexora::ui
