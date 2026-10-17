#include "UiCapabilities.h"

#include <QLibraryInfo>
#include <QSGRendererInterface>
#include <QtTest>

using nexora::ui::UiCapabilities;

// The GPU glass path is only allowed on a hardware OpenGL/Vulkan/... renderer.
class TestUiCapabilities : public QObject
{
    Q_OBJECT
private slots:
    void hardwareRendererAllowsGpuEffects()
    {
        if (QLibraryInfo::version() < QVersionNumber(6, 5))
            QSKIP("QtQuick.Effects needs Qt >= 6.5; see qtOlderThan65DisablesGpuEffects");
        QVERIFY(UiCapabilities::decide(QSGRendererInterface::OpenGL, QStringLiteral("Mesa Intel(R) UHD Graphics 620"), {}));
        QVERIFY(UiCapabilities::decide(QSGRendererInterface::Vulkan, {}, {}));
    }
    void qtOlderThan65DisablesGpuEffects()
    {
        // The gate: hardware GL is allowed exactly when the runtime Qt has QtQuick.Effects.
        const bool hasEffects = QLibraryInfo::version() >= QVersionNumber(6, 5);
        QCOMPARE(UiCapabilities::decide(QSGRendererInterface::OpenGL, QStringLiteral("AMD Radeon"), {}), hasEffects);
    }
    void softwareRenderersDisableGpuEffects()
    {
        QVERIFY(!UiCapabilities::decide(QSGRendererInterface::Software, {}, {}));
        QVERIFY(!UiCapabilities::decide(QSGRendererInterface::OpenGL, QStringLiteral("llvmpipe (LLVM 17.0.6, 256 bits)"), {}));
        QVERIFY(!UiCapabilities::decide(QSGRendererInterface::OpenGL, QStringLiteral("softpipe"), {}));
    }
    void environmentVariableDisablesGpuEffects()
    {
        QVERIFY(!UiCapabilities::decide(QSGRendererInterface::OpenGL, QStringLiteral("AMD Radeon"), QByteArrayLiteral("1")));
    }
    void defaultsToFalseUntilProbed()
    {
        QVERIFY(!UiCapabilities::instance()->gpuEffects());
        QSignalSpy spy(UiCapabilities::instance(), &UiCapabilities::gpuEffectsChanged);
        UiCapabilities::instance()->setGpuEffects(true);
        QCOMPARE(spy.count(), 1);
        UiCapabilities::instance()->setGpuEffects(true);
        QCOMPARE(spy.count(), 1);
        UiCapabilities::instance()->setGpuEffects(false);
    }
};

QTEST_MAIN(TestUiCapabilities)
#include "tst_uicapabilities.moc"
