#include <QtTest>

#include "championpresentation.h"
#include "weaponloader.h"
#include "weaponsearchmodel.h"

namespace {

QJsonObject weapon(const QString &name, const QJsonValue &antiChampionType)
{
    QJsonObject result {
        {QStringLiteral("name"), name},
        {QStringLiteral("hash"), name},
        {QStringLiteral("seasonNumber"), 29},
        {QStringLiteral("seasonName"), QStringLiteral("Test Season")},
        {QStringLiteral("weaponType"), QStringLiteral("Test Weapon")},
        {QStringLiteral("frameType"), QStringLiteral("Test Frame")}
    };
    result.insert(QStringLiteral("antiChampionType"), antiChampionType);
    return result;
}

} // namespace

class AntiChampionTests : public QObject
{
    Q_OBJECT

private slots:
    void presentationUsesOnlyKnownValues();
    void schemaDistinguishesNullFromMissing();
    void breakerFilterSupportsKnownTypesAndOrQueries();
    void breakerTypeDoesNotAffectWeaponOrder_data();
    void breakerTypeDoesNotAffectWeaponOrder();
};

void AntiChampionTests::presentationUsesOnlyKnownValues()
{
    QCOMPARE(ChampionPresentation::forType(QStringLiteral("barrier")).order, 0);
    QCOMPARE(ChampionPresentation::forType(QStringLiteral("overload")).order, 1);
    QCOMPARE(ChampionPresentation::forType(QStringLiteral("unstoppable")).order, 2);
    QVERIFY(!ChampionPresentation::forType(QStringLiteral("unknown")).isValid());
    QCOMPARE(ChampionPresentation::orderForType(QString()), 3);
}

void AntiChampionTests::schemaDistinguishesNullFromMissing()
{
    QJsonArray currentSchema {
        weapon(QStringLiteral("Null is valid"), QJsonValue::Null),
        weapon(QStringLiteral("Unknown remains current schema"), QStringLiteral("future-type"))
    };
    QVERIFY(WeaponLoader::hasCurrentAntiChampionSchema(currentSchema));

    QJsonObject oldWeapon = weapon(QStringLiteral("Old cache"), QJsonValue::Null);
    oldWeapon.remove(QStringLiteral("antiChampionType"));
    QVERIFY(!WeaponLoader::hasCurrentAntiChampionSchema(QJsonArray {oldWeapon}));
}

void AntiChampionTests::breakerFilterSupportsKnownTypesAndOrQueries()
{
    WeaponSearchModel model;
    model.setWeapons(QJsonArray {
        weapon(QStringLiteral("Barrier weapon"), QStringLiteral("barrier")),
        weapon(QStringLiteral("Overload weapon"), QStringLiteral("overload")),
        weapon(QStringLiteral("Unstoppable weapon"), QStringLiteral("unstoppable")),
        weapon(QStringLiteral("No breaker"), QJsonValue::Null)
    });

    model.setSearchQuery(QStringLiteral("-b barrier"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), WeaponSearchModel::AntiChampionTypeRole).toString(),
             QStringLiteral("barrier"));

    model.setSearchQuery(QStringLiteral("-b b"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.activeBreakerFilters().at(0).toMap().value(QStringLiteral("type")).toString(),
             QStringLiteral("barrier"));

    model.setSearchQuery(QStringLiteral("-b o"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.activeBreakerFilters().at(0).toMap().value(QStringLiteral("type")).toString(),
             QStringLiteral("overload"));

    model.setSearchQuery(QStringLiteral("-b u"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.activeBreakerFilters().at(0).toMap().value(QStringLiteral("type")).toString(),
             QStringLiteral("unstoppable"));

    model.setSearchQuery(QStringLiteral("-b uns"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.activeBreakerFilters().at(0).toMap().value(QStringLiteral("type")).toString(),
             QStringLiteral("unstoppable"));

    model.setSearchQuery(QStringLiteral("-b ove"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.activeBreakerFilters().at(0).toMap().value(QStringLiteral("type")).toString(),
             QStringLiteral("overload"));

    model.setSearchQuery(QStringLiteral("-b barr"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.activeBreakerFilters().at(0).toMap().value(QStringLiteral("type")).toString(),
             QStringLiteral("barrier"));

    model.setSearchQuery(QStringLiteral("-b overload -b barrier"));
    QCOMPARE(model.rowCount(), 2);
    const QVariantList filters = model.activeBreakerFilters();
    QCOMPARE(filters.size(), 2);
    QCOMPARE(filters.at(0).toMap().value(QStringLiteral("type")).toString(), QStringLiteral("barrier"));
    QCOMPARE(filters.at(1).toMap().value(QStringLiteral("type")).toString(), QStringLiteral("overload"));

    model.setSearchQuery(QStringLiteral("-b future-type"));
    QCOMPARE(model.rowCount(), 0);
}

void AntiChampionTests::breakerTypeDoesNotAffectWeaponOrder_data()
{
    QTest::addColumn<QString>("query");
    QTest::addColumn<QStringList>("expectedNames");
    const QStringList latest {"A test", "B test", "C test", "D test", "E test"};
    QStringList all = latest;
    all.append("A old");
    QTest::newRow("latest-season") << QString() << latest;
    QTest::newRow("all-weapons") << QStringLiteral("-*") << all;
    // The old weapon also matches "Test Weapon" metadata, after name matches.
    QTest::newRow("name-search") << QStringLiteral("test") << all;
    QTest::newRow("breaker-filter")
        << QStringLiteral("-b b -b o -b u")
        << QStringList {"B test", "C test", "D test", "A old"};
}

void AntiChampionTests::breakerTypeDoesNotAffectWeaponOrder()
{
    QFETCH(QString, query);
    QFETCH(QStringList, expectedNames);
    QJsonObject oldWeapon = weapon("A old", "barrier");
    oldWeapon["seasonNumber"] = 25;
    QJsonArray weapons {
        weapon("D test", "barrier"),
        weapon("E test", "future-type"),
        oldWeapon,
        weapon("B test", "unstoppable"),
        weapon("A test", QJsonValue::Null),
        weapon("C test", "overload")
    };

    WeaponSearchModel model;
    model.setSearchQuery(query);
    // Swap known breaker assignments and ensure the order stays unchanged.
    for (int pass = 0; pass < 2; ++pass) {
        model.setWeapons(weapons);
        model.setShowLatestSeason(true);
        QStringList names;
        for (int row = 0; row < model.rowCount(); ++row)
            names.append(model.data(model.index(row), WeaponSearchModel::NameRole).toString());
        QCOMPARE(names, expectedNames);

        for (int i = 0; i < weapons.size(); ++i) {
            QJsonObject entry = weapons[i].toObject();
            if (entry["antiChampionType"] == QJsonValue("barrier"))
                entry["antiChampionType"] = "unstoppable";
            else if (entry["antiChampionType"] == QJsonValue("unstoppable"))
                entry["antiChampionType"] = "barrier";
            weapons[i] = entry;
        }
    }

    model.setSearchQuery("-*");
    QVERIFY(!model.data(model.index(0), WeaponSearchModel::HasAntiChampionRole).toBool());
    QVERIFY(!model.data(model.index(4), WeaponSearchModel::HasAntiChampionRole).toBool());
}

QTEST_GUILESS_MAIN(AntiChampionTests)

#include "anti_champion_tests.moc"
