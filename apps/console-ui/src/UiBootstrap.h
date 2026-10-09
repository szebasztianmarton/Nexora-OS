#pragma once

class QQmlApplicationEngine;
class QQuickWindow;

namespace nexora::ui {

class AppController;

// Loads the main QML window with |controller| as its backend. Used by main() and the QML
// smoke test so both exercise the same start-up path. Returns nullptr on failure.
QQuickWindow *loadMainWindow(QQmlApplicationEngine &engine, AppController *controller);

} // namespace nexora::ui
