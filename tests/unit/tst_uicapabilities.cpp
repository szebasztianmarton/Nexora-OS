#include "UiCapabilities.h"

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
        QVERIFY(UiCapabilities::decide(QSGRendererInterface::OpenGL, QStringLiteral("Mesa Intel(R) UHD Graphics 620"), {}));
        QVERIFY(UiCapabilities::decide(QSGRendererInterface::Vulkan, {}, {}));
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
