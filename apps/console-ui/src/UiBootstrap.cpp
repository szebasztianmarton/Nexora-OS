#include "UiBootstrap.h"

#include "AppController.h"

#include "nexora/shared/Logging.h"

#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QUrl>

namespace nexora::ui {

QQuickWindow *loadMainWindow(QQmlApplicationEngine &engine, AppController *controller)
{
    // Qt 6.4 does not search the qrc module prefix by default.
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    engine.setInitialProperties({{QStringLiteral("app"), QVariant::fromValue(controller)}});
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Nexora/Console/Main.qml")));

    const auto roots = engine.rootObjects();
    auto *window = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(roots.first());
    if (!window)
        qCCritical(lcUi) << "Failed to load the main QML window";
    return window;
}

} // namespace nexora::ui
