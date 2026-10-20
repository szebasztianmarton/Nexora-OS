#include "UiCapabilities.h"

#include <QJSEngine>
#include <QLibraryInfo>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QQuickWindow>

namespace nexora::ui {

UiCapabilities::UiCapabilities(QObject *parent)
    : QObject(parent)
{
}

UiCapabilities *UiCapabilities::instance()
{
    static UiCapabilities *self = new UiCapabilities;
    return self;
}

UiCapabilities *UiCapabilities::create(QQmlEngine *, QJSEngine *engine)
{
    // One process-wide instance shared with C++; QML must not delete it.
    QJSEngine::setObjectOwnership(instance(), QJSEngine::CppOwnership);
    Q_UNUSED(engine)
    return instance();
}

bool UiCapabilities::decide(QSGRendererInterface::GraphicsApi api, const QString &glRenderer, const QByteArray &disableEnv)
{
    // QtQuick.Effects is a QML module loaded at run time, so the installed Qt decides (not the
    // version we were compiled against): before 6.5 the GPU glass files cannot load at all.
    if (QLibraryInfo::version() < QVersionNumber(6, 5))
        return false;
    if (!disableEnv.isEmpty() || api == QSGRendererInterface::Software || api == QSGRendererInterface::Unknown)
        return false;
    const QString renderer = glRenderer.toLower();
    return !renderer.contains(QLatin1String("llvmpipe")) && !renderer.contains(QLatin1String("softpipe"));
}

void UiCapabilities::probe(QQuickWindow *window)
{
    if (!window)
        return;
    // Runs on the render thread: read the GL renderer string there, publish on the GUI thread.
    QObject::connect(
        window, &QQuickWindow::sceneGraphInitialized, window,
        [window] {
            const auto api = window->rendererInterface()->graphicsApi();
            QString glRenderer;
            if (api == QSGRendererInterface::OpenGL) {
                if (auto *context = QOpenGLContext::currentContext())
                    glRenderer = QString::fromLatin1(
                        reinterpret_cast<const char *>(context->functions()->glGetString(GL_RENDERER)));
            }
            const bool allowed = decide(api, glRenderer, qgetenv("NEXORA_NO_GLASS_BLUR"));
            QMetaObject::invokeMethod(instance(), [allowed] { instance()->setGpuEffects(allowed); }, Qt::QueuedConnection);
        },
        Qt::DirectConnection);
}

void UiCapabilities::setGpuEffects(bool value)
{
    if (m_gpuEffects == value)
        return;
    m_gpuEffects = value;
    emit gpuEffectsChanged();
}

} // namespace nexora::ui
