// Loads the real Main.qml (offscreen) and checks start-up, navigation and empty state.
#include "AppController.h"
#include "UiBootstrap.h"

#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>
#include <QtQml/QQmlExtensionPlugin>

Q_IMPORT_QML_PLUGIN(Nexora_ConsolePlugin)

using nexora::ui::AppController;

namespace {

constexpr int Timeout = 10000;

void writeConfig(const QTemporaryDir &dir, const QString &id, const QString &arguments = {})
{
    QFile file(dir.filePath(id + QStringLiteral(".json")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QStringLiteral("{\"id\":\"%1\",\"name\":\"Emu %1\",\"executable\":\"%2\",\"arguments\":[%3]}")
                   .arg(id, QStringLiteral(NEXORA_FAKE_EMULATOR), arguments)
                   .toUtf8());
}

} // namespace

class TestQmlSmoke : public QObject
{
    Q_OBJECT

private:
    struct Ui
    {
        std::unique_ptr<AppController> app;
        std::unique_ptr<QQmlApplicationEngine> engine;
        QQuickWindow *window = nullptr;
        QList<QQmlError> warnings;
    };

    static void load(Ui &ui, const QString &configDir)
    {
        ui.app = std::make_unique<AppController>(
            AppController::Options{configDir, QStringLiteral("test"), {}, false});
        ui.engine = std::make_unique<QQmlApplicationEngine>();
        QObject::connect(ui.engine.get(), &QQmlEngine::warnings, ui.engine.get(),
                         [&ui](const QList<QQmlError> &warnings) { ui.warnings.append(warnings); });
        ui.window = nexora::ui::loadMainWindow(*ui.engine, ui.app.get());
        QVERIFY(ui.window);
        QVERIFY(QTest::qWaitForWindowExposed(ui.window));
    }

    static QString describe(const QList<QQmlError> &errors)
    {
        QStringList lines;
        for (const auto &e : errors)
            lines.append(e.toString());
        return lines.join(u'\n');
    }

    template<typename T = QQuickItem>
    static T *find(Ui &ui, const char *name)
    {
        return ui.window->findChild<T *>(QString::fromLatin1(name));
    }

private slots:
    void initTestCase()
    {
        QQuickStyle::setStyle(QStringLiteral("Basic"));
    }

    void libraryNavigationAndDiagnostics()
    {
        QTemporaryDir dir;
        writeConfig(dir, QStringLiteral("alpha"));
        writeConfig(dir, QStringLiteral("beta"));
        Ui ui;
        load(ui, dir.path());
        if (QTest::currentTestFailed())
            return;

        auto *list = find(ui, "emulatorList");
        QVERIFY(list);
        QVERIFY(list->isVisible());
        QTRY_VERIFY_WITH_TIMEOUT(list->hasActiveFocus(), Timeout);
        QCOMPARE(list->property("count").toInt(), 2);
        QCOMPARE(list->property("currentIndex").toInt(), 0);
        QVERIFY(!find(ui, "emptyState")->isVisible());

        QTest::keyClick(ui.window, Qt::Key_Right);
        QCOMPARE(list->property("currentIndex").toInt(), 1);
        QTest::keyClick(ui.window, Qt::Key_Left);
        QCOMPARE(list->property("currentIndex").toInt(), 0);

        auto *stack = find(ui, "stack");
        QVERIFY(stack);
        QTest::keyClick(ui.window, Qt::Key_F1);
        QTRY_COMPARE_WITH_TIMEOUT(stack->property("depth").toInt(), 2, Timeout);
        QTRY_VERIFY_WITH_TIMEOUT(find(ui, "diagnosticsScreen") != nullptr, Timeout);

        QTest::keyClick(ui.window, Qt::Key_Escape);
        QTRY_COMPARE_WITH_TIMEOUT(stack->property("depth").toInt(), 1, Timeout);
        QTRY_VERIFY_WITH_TIMEOUT(list->hasActiveFocus(), Timeout);

        // Back on the root screen is a no-op (a console UI never quits on Back).
        QTest::keyClick(ui.window, Qt::Key_Escape);
        QCOMPARE(stack->property("depth").toInt(), 1);

        QVERIFY2(ui.warnings.isEmpty(), qPrintable(describe(ui.warnings)));
    }

    void launchShowsOverlayAndReturns()
    {
        QTemporaryDir dir;
        writeConfig(dir, QStringLiteral("long"), QStringLiteral("\"--sleep-ms\",\"30000\""));
        Ui ui;
        load(ui, dir.path());
        if (QTest::currentTestFailed())
            return;
        auto *list = find(ui, "emulatorList");
        QTRY_VERIFY_WITH_TIMEOUT(list->hasActiveFocus(), Timeout);

        QTest::keyClick(ui.window, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("running"), Timeout);
        auto *overlay = find(ui, "runningOverlay");
        QVERIFY(overlay);
        QTRY_VERIFY_WITH_TIMEOUT(overlay->isVisible(), Timeout);
        QTRY_VERIFY_WITH_TIMEOUT(find(ui, "stopButton")->hasActiveFocus(), Timeout);

        // Back must not leave the overlay or quit while an emulator runs.
        QTest::keyClick(ui.window, Qt::Key_Escape);
        QCOMPARE(ui.app->state(), QStringLiteral("running"));

        QTest::keyClick(ui.window, Qt::Key_Return); // "Stop emulator"
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("exited"), Timeout);
        QTRY_VERIFY_WITH_TIMEOUT(!overlay->isVisible(), Timeout);
        QTRY_VERIFY_WITH_TIMEOUT(list->hasActiveFocus(), Timeout);

        QVERIFY2(ui.warnings.isEmpty(), qPrintable(describe(ui.warnings)));
    }

    void emptyConfigurationShowsMessage()
    {
        QTemporaryDir dir;
        Ui ui;
        load(ui, dir.path());
        if (QTest::currentTestFailed())
            return;
        auto *empty = find(ui, "emptyState");
        QVERIFY(empty);
        QVERIFY(empty->isVisible());
        QVERIFY(!find(ui, "emulatorList")->isVisible());
        QVERIFY2(ui.warnings.isEmpty(), qPrintable(describe(ui.warnings)));
    }
};

QTEST_MAIN(TestQmlSmoke)
#include "tst_qmlsmoke.moc"
