#include <QGuiApplication>
#include <QKeyEvent>
#include <QSignalSpy>
#include <QTest>
#include <QWindow>

#include "controllermanager.h"

using nexora::ControllerManager;

namespace {

// Records key presses delivered to the watched window.
class KeyRecorder : public QObject
{
public:
    struct Press
    {
        int key;
        quint32 nativeModifiers;
    };
    QList<Press> presses;
    int releases = 0;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::KeyPress) {
            auto *key = static_cast<QKeyEvent *>(event);
            presses.append({key->key(), key->nativeModifiers()});
        } else if (event->type() == QEvent::KeyRelease) {
            ++releases;
        }
        return QObject::eventFilter(watched, event);
    }
};

} // namespace

class TestControllerManager : public QObject
{
    Q_OBJECT

private slots:
    void worksWithoutDevices();
    void keyboardLabels();
    void injectActionPostsKeys();
    void suppressedBlocksInjection();
    void mappings();
    void repeatSettings();
};

void TestControllerManager::worksWithoutDevices()
{
    ControllerManager manager;
    if (manager.available())
        QVERIFY(manager.unavailableReason().isEmpty());
    else
        QVERIFY(!manager.unavailableReason().isEmpty());
    QCOMPARE(manager.count(), int(manager.controllers().size()));
    QCOMPARE(manager.lastInputDevice(), QStringLiteral("keyboard"));
    QTest::qWait(50); // let the poll timer run a few times
    QCOMPARE(manager.count(), int(manager.controllers().size()));
}

void TestControllerManager::keyboardLabels()
{
    ControllerManager manager;
    QCOMPARE(manager.buttonLabel("confirm"), QStringLiteral("Enter"));
    QCOMPARE(manager.buttonLabel("back"), QStringLiteral("Esc"));
    QCOMPARE(manager.buttonLabel("secondary"), QStringLiteral("F"));
    QCOMPARE(manager.buttonLabel("search"), QStringLiteral("/"));
    QCOMPARE(manager.buttonLabel("options"), QStringLiteral("M"));
    QCOMPARE(manager.buttonLabel("prevTab"), QStringLiteral("PgUp"));
    QCOMPARE(manager.buttonLabel("nextTab"), QStringLiteral("PgDn"));
    QCOMPARE(manager.buttonLabel("unknown"), QString());

    QSignalSpy deviceChanged(&manager, &ControllerManager::lastInputDeviceChanged);
    manager.injectAction("down"); // no window: nothing posted, but device switches
    QCOMPARE(manager.lastInputDevice(), QStringLiteral("controller"));
    QCOMPARE(manager.controllerType(), QStringLiteral("generic"));
    QCOMPARE(manager.buttonLabel("confirm"), QStringLiteral("South"));
    QCOMPARE(manager.buttonLabel("options"), QStringLiteral("Start"));
    manager.noteKeyboardInput();
    QCOMPARE(manager.lastInputDevice(), QStringLiteral("keyboard"));
    QCOMPARE(deviceChanged.count(), 2);
}

void TestControllerManager::injectActionPostsKeys()
{
    QWindow window;
    KeyRecorder recorder;
    window.installEventFilter(&recorder);
    ControllerManager manager;
    manager.setTargetWindow(&window);

    const QList<QPair<QString, int>> expected{
        {"confirm", Qt::Key_Return}, {"back", Qt::Key_Escape},  {"secondary", Qt::Key_F},
        {"search", Qt::Key_Slash},   {"options", Qt::Key_Menu}, {"prevTab", Qt::Key_PageUp},
        {"nextTab", Qt::Key_PageDown}, {"up", Qt::Key_Up},      {"down", Qt::Key_Down},
        {"left", Qt::Key_Left},      {"right", Qt::Key_Right}};
    for (const auto &e : expected)
        manager.injectAction(e.first);
    manager.injectAction("not-an-action");
    QCoreApplication::processEvents();

    QCOMPARE(recorder.presses.size(), expected.size());
    QCOMPARE(recorder.releases, int(expected.size()));
    for (int i = 0; i < expected.size(); ++i) {
        QCOMPARE(recorder.presses.at(i).key, expected.at(i).second);
        QCOMPARE(recorder.presses.at(i).nativeModifiers, ControllerManager::SyntheticMarker);
    }
}

void TestControllerManager::suppressedBlocksInjection()
{
    QWindow window;
    KeyRecorder recorder;
    window.installEventFilter(&recorder);
    ControllerManager manager;
    manager.setTargetWindow(&window);

    QSignalSpy suppressedChanged(&manager, &ControllerManager::suppressedChanged);
    manager.setSuppressed(true);
    QCOMPARE(suppressedChanged.count(), 1);
    manager.injectAction("confirm");
    QCoreApplication::processEvents();
    QCOMPARE(recorder.presses.size(), 0);

    manager.setSuppressed(false);
    manager.injectAction("confirm");
    QCoreApplication::processEvents();
    QCOMPARE(recorder.presses.size(), 1);
}

void TestControllerManager::mappings()
{
    ControllerManager manager;
    const QVariantList list = manager.mappings();
    QCOMPARE(list.size(), 11);
    QStringList actions;
    for (const QVariant &v : list) {
        const QVariantMap m = v.toMap();
        actions.append(m.value("action").toString());
        QVERIFY(!m.value("label").toString().isEmpty());
        QVERIFY(!m.value("controllerInput").toString().isEmpty());
        QVERIFY(!m.value("keyboardInput").toString().isEmpty());
    }
    QCOMPARE(actions, QStringList({"confirm", "back", "secondary", "search", "options", "prevTab", "nextTab", "up",
                                   "down", "left", "right"}));
}

void TestControllerManager::repeatSettings()
{
    ControllerManager manager;
    QSignalSpy repeatChanged(&manager, &ControllerManager::repeatChanged);
    manager.setRepeatDelayMs(300);
    manager.setRepeatIntervalMs(80);
    manager.setRepeatIntervalMs(80);
    QCOMPARE(repeatChanged.count(), 2);
    QCOMPARE(manager.repeatDelayMs(), 300);
    QCOMPARE(manager.repeatIntervalMs(), 80);
}

QTEST_MAIN(TestControllerManager)
#include "tst_controllermanager.moc"
