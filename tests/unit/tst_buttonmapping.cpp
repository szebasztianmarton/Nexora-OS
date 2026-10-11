#include "input/ButtonMapping.h"
#include "nexora/shared/Settings.h"

#include <QTest>

using namespace nexora::input;

class TestButtonMapping : public QObject
{
    Q_OBJECT

private slots:
    void defaults()
    {
        const ButtonMapping m = ButtonMapping::defaults();
        QCOMPARE(m.actionFor(GamepadButton::South), std::optional(NavAction::Accept));
        QCOMPARE(m.actionFor(GamepadButton::East), std::optional(NavAction::Back));
        QCOMPARE(m.actionFor(GamepadButton::Start), std::optional(NavAction::Menu));
        QCOMPARE(m.actionFor(GamepadButton::North), std::optional(NavAction::Favorite));
        QCOMPARE(m.actionFor(GamepadButton::West), std::optional(NavAction::Search));
        QCOMPARE(m.actionFor(GamepadButton::L1), std::optional(NavAction::PrevTab));
        QCOMPARE(m.actionFor(GamepadButton::R1), std::optional(NavAction::NextTab));
        QCOMPARE(m.actionFor(GamepadButton::Select), std::optional(NavAction::Options));
        // Unassigned Guide falls back to Menu; directions are never part of a mapping.
        QCOMPARE(m.actionFor(GamepadButton::Guide), std::optional(NavAction::Menu));
        QCOMPARE(m.actionFor(GamepadButton::DpadUp), std::nullopt);
    }

    void settingsDefaultMatchesBuiltIn()
    {
        // The settings schema default and ButtonMapping::defaults() must agree.
        const auto *spec = nexora::shared::Settings::spec(nexora::shared::Settings::InputMapping);
        QVERIFY(spec);
        QStringList errors;
        const auto fromSettings = ButtonMapping::fromVariantMap(spec->defaultValue.toMap(), &errors);
        QVERIFY2(fromSettings, qPrintable(errors.join(u'\n')));
        QVERIFY(*fromSettings == ButtonMapping::defaults());
    }

    void roundTripsThroughVariantMap()
    {
        ButtonMapping m = ButtonMapping::defaults();
        m.bind(NavAction::Accept, GamepadButton::East); // Nintendo-style confirm
        const auto again = ButtonMapping::fromVariantMap(m.toVariantMap(), nullptr);
        QVERIFY(again);
        QVERIFY(*again == m);
    }

    void olderMappingsGainLaterActions()
    {
        // Stored before "options" existed: Select is free, so options gets it.
        const QVariantMap legacy{{QStringLiteral("accept"), QStringLiteral("east")},
                                 {QStringLiteral("back"), QStringLiteral("south")}};
        auto m = ButtonMapping::fromVariantMap(legacy, nullptr);
        QVERIFY(m);
        QCOMPARE(m->buttonFor(NavAction::Options), std::optional(GamepadButton::Select));
        QCOMPARE(m->buttonFor(NavAction::Accept), std::optional(GamepadButton::East));

        // Select already used by another action: options stays unbound and the stored
        // binding wins; an unassigned Guide still opens the menu.
        QVariantMap selectTaken = legacy;
        selectTaken.insert(QStringLiteral("search"), QStringLiteral("select"));
        m = ButtonMapping::fromVariantMap(selectTaken, nullptr);
        QVERIFY(m);
        QCOMPARE(m->buttonFor(NavAction::Options), std::nullopt);
        QCOMPARE(m->actionFor(GamepadButton::Select), std::optional(NavAction::Search));
        QCOMPARE(m->actionFor(GamepadButton::Guide), std::optional(NavAction::Menu));
    }

    void bindSwapsConflicts()
    {
        ButtonMapping m = ButtonMapping::defaults();
        m.bind(NavAction::Accept, GamepadButton::East);
        QCOMPARE(m.buttonFor(NavAction::Accept), std::optional(GamepadButton::East));
        QCOMPARE(m.buttonFor(NavAction::Back), std::optional(GamepadButton::South)); // swapped
        QCOMPARE(m.actionFor(GamepadButton::East), std::optional(NavAction::Accept));
        QCOMPARE(m.actionFor(GamepadButton::South), std::optional(NavAction::Back));
    }

    void rejectsInvalidMappings_data()
    {
        QTest::addColumn<QVariantMap>("map");
        QTest::addColumn<QString>("expected");
        const QString s = QStringLiteral("south");
        const QString e = QStringLiteral("east");
        QTest::newRow("unknown action") << QVariantMap{{QStringLiteral("accept"), s},
                                                       {QStringLiteral("back"), e},
                                                       {QStringLiteral("jump"), QStringLiteral("west")}}
                                        << QStringLiteral("unknown action");
        QTest::newRow("unknown button") << QVariantMap{{QStringLiteral("accept"), QStringLiteral("x")},
                                                       {QStringLiteral("back"), e}}
                                        << QStringLiteral("unknown button");
        QTest::newRow("double binding") << QVariantMap{{QStringLiteral("accept"), s}, {QStringLiteral("back"), s}}
                                        << QStringLiteral("assigned to both");
        QTest::newRow("no accept") << QVariantMap{{QStringLiteral("back"), e}}
                                   << QStringLiteral("\"accept\" must be assigned");
        QTest::newRow("no back") << QVariantMap{{QStringLiteral("accept"), s}}
                                 << QStringLiteral("\"back\" must be assigned");
        QTest::newRow("direction as button")
            << QVariantMap{{QStringLiteral("accept"), QStringLiteral("dpadUp")}, {QStringLiteral("back"), e}}
            << QStringLiteral("unknown button");
    }

    void rejectsInvalidMappings()
    {
        QFETCH(QVariantMap, map);
        QFETCH(QString, expected);
        QStringList errors;
        QVERIFY(!ButtonMapping::fromVariantMap(map, &errors));
        QVERIFY2(errors.join(u'\n').contains(expected), qPrintable(errors.join(u'\n')));
    }

    void namesRoundTrip()
    {
        for (const QString &name : ButtonMapping::actionNames())
            QCOMPARE(ButtonMapping::actionName(*ButtonMapping::actionFromName(name)), name);
        for (const QString &name : ButtonMapping::buttonNames())
            QCOMPARE(ButtonMapping::buttonName(*ButtonMapping::buttonFromName(name)), name);
        QCOMPARE(ButtonMapping::actionNames().size(), 8);
    }
};

QTEST_GUILESS_MAIN(TestButtonMapping)
#include "tst_buttonmapping.moc"
