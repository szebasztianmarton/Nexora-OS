#include "UiBootstrap.h"

#include "UiCapabilities.h"
#include "UiContext.h"

#include "nexora/shared/Logging.h"

#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QUrl>

namespace nexora::ui {

QQuickWindow *loadMainWindow(QQmlApplicationEngine &engine, UiContext *context)
{
    // Qt 6.4 does not search the qrc module prefix by default.
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    engine.setInitialProperties({
        {QStringLiteral("app"), QVariant::fromValue(context->app())},
        {QStringLiteral("library"), QVariant::fromValue(context->library())},
        {QStringLiteral("settings"), QVariant::fromValue(context->settings())},
        {QStringLiteral("system"), QVariant::fromValue(context->system())},
        {QStringLiteral("gameBar"), QVariant::fromValue(context->gameBar())},
        {QStringLiteral("updates"), QVariant::fromValue(context->updates())},
        {QStringLiteral("notifications"), QVariant::fromValue(context->notifications())},
        {QStringLiteral("sessions"), QVariant::fromValue(context->sessions())},
    });
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Nexora/Console/Main.qml")));

    const auto roots = engine.rootObjects();
    auto *window = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(roots.first());
    if (!window)
        qCCritical(lcUi) << "Failed to load the main QML window";
    else
        UiCapabilities::probe(window);
    return window;
}

} // namespace nexora::ui
