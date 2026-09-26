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
    void resultsUsePresentationOrder();
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

void AntiChampionTests::resultsUsePresentationOrder()
{
    WeaponSearchModel model;
    model.setWeapons(QJsonArray {
        weapon(QStringLiteral("A null"), QJsonValue::Null),
        weapon(QStringLiteral("B unstoppable"), QStringLiteral("unstoppable")),
        weapon(QStringLiteral("C overload"), QStringLiteral("overload")),
        weapon(QStringLiteral("D barrier"), QStringLiteral("barrier")),
        weapon(QStringLiteral("E unknown"), QStringLiteral("future-type"))
    });
    model.setSearchQuery(QStringLiteral("-*"));

    QCOMPARE(model.rowCount(), 5);
    QCOMPARE(model.data(model.index(0), WeaponSearchModel::AntiChampionTypeRole).toString(),
             QStringLiteral("barrier"));
    QCOMPARE(model.data(model.index(1), WeaponSearchModel::AntiChampionTypeRole).toString(),
             QStringLiteral("overload"));
    QCOMPARE(model.data(model.index(2), WeaponSearchModel::AntiChampionTypeRole).toString(),
             QStringLiteral("unstoppable"));
    QVERIFY(!model.data(model.index(3), WeaponSearchModel::HasAntiChampionRole).toBool());
    QVERIFY(!model.data(model.index(4), WeaponSearchModel::HasAntiChampionRole).toBool());

    model.setSearchQuery(QString());
    model.setShowLatestSeason(true);
    QCOMPARE(model.rowCount(), 5);
    QCOMPARE(model.data(model.index(0), WeaponSearchModel::AntiChampionTypeRole).toString(),
             QStringLiteral("barrier"));
    QCOMPARE(model.data(model.index(1), WeaponSearchModel::AntiChampionTypeRole).toString(),
             QStringLiteral("overload"));
    QCOMPARE(model.data(model.index(2), WeaponSearchModel::AntiChampionTypeRole).toString(),
             QStringLiteral("unstoppable"));
}

QTEST_GUILESS_MAIN(AntiChampionTests)

#include "anti_champion_tests.moc"
