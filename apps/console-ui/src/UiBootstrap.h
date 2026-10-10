#pragma once

class QQmlApplicationEngine;
class QQuickWindow;

namespace nexora::ui {

class UiContext;

// Loads the main QML window with the backends of |context|. Used by main() and the QML
// smoke test so both exercise the same start-up path. Returns nullptr on failure.
QQuickWindow *loadMainWindow(QQmlApplicationEngine &engine, UiContext *context);

} // namespace nexora::ui
