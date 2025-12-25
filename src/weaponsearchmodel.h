#ifndef WEAPONSEARCHMODEL_H
#define WEAPONSEARCHMODEL_H

#include <QObject>
#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

class WeaponSearchModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged)
    Q_PROPERTY(bool showLatestSeason READ showLatestSeason WRITE setShowLatestSeason NOTIFY showLatestSeasonChanged)
    Q_PROPERTY(bool autoShowLatestSeason READ autoShowLatestSeason WRITE setAutoShowLatestSeason NOTIFY autoShowLatestSeasonChanged)
    Q_PROPERTY(bool openInPWA READ openInPWA WRITE setOpenInPWA NOTIFY openInPWAChanged)
    Q_PROPERTY(QStringList activeSourceFilters READ activeSourceFilters NOTIFY activeSourceFiltersChanged)

public:
    enum WeaponRoles {
        NameRole = Qt::UserRole + 1,
        HashRole,
        IconRole,
        WeaponTypeRole,
        FrameTypeRole,
        SeasonNumberRole,
        SeasonNameRole,
        MatchedFieldRole,  // Which field matched: "name", "weaponType", "frameType", "season", or ""
        IsHolofoilRole,
        IsExoticRole,
        DamageTypeRole,
        DamageTypeIconRole,
        AmmoTypeRole,
        AmmoTypeIconRole
    };

    explicit WeaponSearchModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString searchQuery() const { return m_searchQuery; }
    void setSearchQuery(const QString &query);

    bool showLatestSeason() const { return m_showLatestSeason; }
    void setShowLatestSeason(bool show);

    bool autoShowLatestSeason() const { return m_autoShowLatestSeason; }
    void setAutoShowLatestSeason(bool autoShow);

    bool openInPWA() const { return m_openInPWA; }
    void setOpenInPWA(bool open);

    QStringList activeSourceFilters() const { return m_activeSourceFilters; }

    void setWeapons(const QJsonArray &weapons);

    Q_INVOKABLE void openWeapon(int index);
    Q_INVOKABLE void clearSearch();

signals:
    void searchQueryChanged();
    void showLatestSeasonChanged();
    void autoShowLatestSeasonChanged();
    void openInPWAChanged();
    void activeSourceFiltersChanged();
    void weaponsLoaded();

private:
    void filterWeapons();
    
    // Fuse.js-style fuzzy matching functions
    int fuzzyScore(const QString &text, const QString &query) const;
    double fuseFuzzyMatch(const QString &text, const QString &pattern) const;
    int levenshteinDistance(const QString &s1, const QString &s2) const;
    QString normalizeText(const QString &text) const;
    
    // Weapon name helpers
    QString getBaseWeaponName(const QString &name) const;  // Removes (Adept), (Harrowed), etc.
    bool isAdeptWeapon(const QJsonObject &weapon) const;   // Checks if weapon is adept (API field or name suffix)

    QJsonArray m_allWeapons;
    QJsonArray m_filteredWeapons;
    QString m_searchQuery;
    int m_latestSeason = 0;
    bool m_showLatestSeason = false;
    bool m_autoShowLatestSeason = true;
    bool m_openInPWA = true;      // Open links in Chrome PWA mode (default: true)
    QStringList m_activeSourceFilters;  // Currently active source filter display names
};

#endif // WEAPONSEARCHMODEL_H
