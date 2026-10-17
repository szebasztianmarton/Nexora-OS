#pragma once

#include <QByteArray>
#include <QObject>
#include <QSGRendererInterface>
#include <QString>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;
class QQuickWindow;

namespace nexora::ui {

// Whether the console UI may use GPU effects (the Liquid Glass backdrop blur). False until the
// main window's scene graph is up, and always false on the software backend, on llvmpipe /
// softpipe and when NEXORA_NO_GLASS_BLUR is set. QML: UiCapabilities.gpuEffects.
class UiCapabilities : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool gpuEffects READ gpuEffects NOTIFY gpuEffectsChanged)

public:
    static UiCapabilities *instance();
    static UiCapabilities *create(QQmlEngine *, QJSEngine *);

    static bool decide(QSGRendererInterface::GraphicsApi api, const QString &glRenderer, const QByteArray &disableEnv);
    // Decides once the window's scene graph is initialised (renderer known).
    static void probe(QQuickWindow *window);

    bool gpuEffects() const { return m_gpuEffects; }
    void setGpuEffects(bool value);

signals:
    void gpuEffectsChanged();

private:
    explicit UiCapabilities(QObject *parent = nullptr);
    bool m_gpuEffects = false;
};

} // namespace nexora::ui
