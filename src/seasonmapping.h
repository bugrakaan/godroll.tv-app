#ifndef SEASONMAPPING_H
#define SEASONMAPPING_H

#include <QString>
#include <QHash>
#include <QJsonArray>

struct SeasonInfo {
    QString identifier;
    QString name;
    int number;
    QString expansion;
    QString type;
    
    QString displayName() const {
        if (!expansion.isEmpty() && !name.isEmpty()) {
            return QString("%1 • %2").arg(expansion, name);
        } else if (!expansion.isEmpty()) {
            return expansion;
        } else if (!name.isEmpty()) {
            return name;
        }
        return QString("Season %1").arg(number);
    }
};

class SeasonMapping {
public:
    static SeasonMapping& instance() {
        static SeasonMapping instance;
        return instance;
    }
    
    QString getSeasonFromTraitIds(const QJsonArray& traitIds) const {
        for (const auto& trait : traitIds) {
            QString traitId = trait.toString();
            // Match pattern: releases.v[NUMBER] or releases.v[NUMBER].(season|annual|core|dlc)
            if (traitId.startsWith("releases.v")) {
                // Extract version: "releases.v300.annual" -> "v300"
                QString afterReleases = traitId.mid(9); // after "releases."
                int dotIndex = afterReleases.indexOf('.', 1);
                QString version = (dotIndex > 0) ? afterReleases.left(dotIndex) : afterReleases;
                
                if (m_seasons.contains(version)) {
                    return m_seasons[version].displayName();
                }
            }
        }
        return QString();
    }
    
    int getSeasonNumber(const QJsonArray& traitIds) const {
        for (const auto& trait : traitIds) {
            QString traitId = trait.toString();
            if (traitId.startsWith("releases.v")) {
                QString afterReleases = traitId.mid(9);
                int dotIndex = afterReleases.indexOf('.', 1);
                QString version = (dotIndex > 0) ? afterReleases.left(dotIndex) : afterReleases;
                
                if (m_seasons.contains(version)) {
                    return m_seasons[version].number;
                }
            }
        }
        return 0;
    }
    
    QString getSeasonName(const QJsonArray& traitIds) const {
        for (const auto& trait : traitIds) {
            QString traitId = trait.toString();
            if (traitId.startsWith("releases.v")) {
                QString afterReleases = traitId.mid(9);
                int dotIndex = afterReleases.indexOf('.', 1);
                QString version = (dotIndex > 0) ? afterReleases.left(dotIndex) : afterReleases;
                
                if (m_seasons.contains(version)) {
                    const SeasonInfo& info = m_seasons[version];
                    // Return name if exists, otherwise expansion
                    if (!info.name.isEmpty()) {
                        return info.name;
                    } else if (!info.expansion.isEmpty()) {
                        return info.expansion;
                    }
                }
            }
        }
        return QString();
    }
    
private:
    SeasonMapping() {
        initSeasons();
    }
    
    void initSeasons() {
        // Year 1
        m_seasons["v300"] = {"v300", "", 1, "Red War", "annual"};
        m_seasons["v310"] = {"v310", "", 2, "Curse of Osiris", "annual"};
        m_seasons["v320"] = {"v320", "", 3, "Warmind", "annual"};
        
        // Year 2
        m_seasons["v400"] = {"v400", "", 4, "Forsaken", "annual"};
        m_seasons["v410"] = {"v410", "Season of the Forge", 5, "", "season"};
        m_seasons["v420"] = {"v420", "Season of the Drifter", 6, "", "season"};
        m_seasons["v450"] = {"v450", "Season of Opulence", 7, "", "season"};
        
        // Year 3 - Shadowkeep Era
        m_seasons["v460"] = {"v460", "Season of the Undying", 8, "Shadowkeep", "season"};
        m_seasons["v470"] = {"v470", "Season of Dawn", 9, "", "season"};
        m_seasons["v480"] = {"v480", "Season of the Worthy", 10, "", "season"};
        m_seasons["v490"] = {"v490", "Season of Arrivals", 11, "", "season"};
        
        // Year 4 - Beyond Light Era
        m_seasons["v500"] = {"v500", "Season of the Hunt", 12, "Beyond Light", "season"};
        m_seasons["v510"] = {"v510", "Season of the Chosen", 13, "", "season"};
        m_seasons["v520"] = {"v520", "Season of the Splicer", 14, "", "season"};
        m_seasons["v530"] = {"v530", "Season of the Lost", 15, "", "season"};
        m_seasons["v540"] = {"v540", "", 15, "30th Anniversary Pack", "season"};
        
        // Year 5 - Witch Queen Era
        m_seasons["v600"] = {"v600", "Season of the Risen", 16, "The Witch Queen", "season"};
        m_seasons["v610"] = {"v610", "Season of the Haunted", 17, "", "season"};
        m_seasons["v620"] = {"v620", "Season of Plunder", 18, "", "season"};
        m_seasons["v630"] = {"v630", "Season of the Seraph", 19, "", "season"};
        
        // Year 6 - Lightfall Era
        m_seasons["v700"] = {"v700", "Season of Defiance", 20, "Lightfall", "season"};
        m_seasons["v710"] = {"v710", "Season of the Deep", 21, "", "season"};
        m_seasons["v720"] = {"v720", "Season of the Witch", 22, "", "season"};
        m_seasons["v730"] = {"v730", "Season of the Wish", 23, "", "season"};
        
        // Year 7 - The Final Shape Era
        m_seasons["v800"] = {"v800", "", 24, "The Final Shape", "season"};
        m_seasons["v810"] = {"v810", "Episode: Echoes", 25, "", "season"};
        m_seasons["v820"] = {"v820", "Episode: Revenant", 26, "", "season"};
        
        // Year 8 - Reclamation Era
        m_seasons["v900"] = {"v900", "Reclamation", 27, "Edge of Fate", "core"};
        m_seasons["v910"] = {"v910", "Ash & Iron", 27, "Edge of Fate", "core"};
        m_seasons["v950"] = {"v950", "Renegades", 28, "", "season"};
    }
    
    QHash<QString, SeasonInfo> m_seasons;
};

#endif // SEASONMAPPING_H
