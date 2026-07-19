#include "weaponsearchmodel.h"
#include "perkaliases.h"
#include <QDesktopServices>
#include <QUrl>
#include <QSettings>
#include <QRegularExpression>
#include <QProcess>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>
#include <algorithm>
#include <tuple>
#include <set>

WeaponSearchModel::WeaponSearchModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // Load saved preferences
    QSettings settings("Godroll.tv", "GodrollLauncher");
    m_autoShowLatestSeason = settings.value("autoShowLatestSeason", true).toBool();
    m_openInPWA = settings.value("openInPWA", true).toBool();
}

int WeaponSearchModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_filteredWeapons.size();
}

QVariant WeaponSearchModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_filteredWeapons.size())
        return QVariant();

    QJsonObject weapon = m_filteredWeapons[index.row()].toObject();

    switch (role) {
    case NameRole:
        return weapon["name"].toString();
    case HashRole:
        return weapon["hash"].toVariant();
    case IconRole:
        return weapon["icon"].toString();
    case WeaponTypeRole:
        return weapon["weaponType"].toString();
    case FrameTypeRole:
        return weapon["frameType"].toString();
    case SeasonNumberRole:
        return weapon["seasonNumber"].toInt();
    case SeasonNameRole:
        return weapon["seasonName"].toString();
    case MatchedFieldRole:
        return weapon["matchedField"].toString();
    case IsHolofoilRole:
        return weapon["isHolofoil"].toBool();
    case IsExoticRole:
        return weapon["isExotic"].toBool();
    case DamageTypeRole:
        return weapon["damageType"].toString();
    case DamageTypeIconRole:
        return weapon["damageTypeIcon"].toString();
    case AmmoTypeRole:
        return weapon["ammoType"].toString();
    case AmmoTypeIconRole:
        return weapon["ammoTypeIcon"].toString();
    case IconWatermarkRole:
        return weapon["iconWatermark"].toString();
    case IsTier3WeaponRole:
        return weapon["isTier3Weapon"].toBool();
    case IsTier5WeaponRole:
        return weapon["isTier5Weapon"].toBool();
    case TierTypeNameRole:
        return weapon["tierTypeName"].toString();
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> WeaponSearchModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[HashRole] = "hash";
    roles[IconRole] = "icon";
    roles[WeaponTypeRole] = "weaponType";
    roles[FrameTypeRole] = "frameType";
    roles[SeasonNumberRole] = "seasonNumber";
    roles[SeasonNameRole] = "seasonName";
    roles[MatchedFieldRole] = "matchedField";
    roles[IsHolofoilRole] = "isHolofoil";
    roles[IsExoticRole] = "isExotic";
    roles[DamageTypeRole] = "damageType";
    roles[DamageTypeIconRole] = "damageTypeIcon";
    roles[AmmoTypeRole] = "ammoType";
    roles[AmmoTypeIconRole] = "ammoTypeIcon";
    roles[IconWatermarkRole] = "iconWatermark";
    roles[IsTier3WeaponRole] = "isTier3Weapon";
    roles[IsTier5WeaponRole] = "isTier5Weapon";
    roles[TierTypeNameRole] = "tierTypeName";
    return roles;
}

void WeaponSearchModel::setSearchQuery(const QString &query)
{
    if (m_searchQuery == query)
        return;

    m_searchQuery = query;
    filterWeapons();
    emit searchQueryChanged();
}

void WeaponSearchModel::setWeapons(const QJsonArray &weapons)
{
    m_allWeapons = weapons;
    
    // Find latest season number
    m_latestSeason = 0;
    for (const QJsonValue &value : m_allWeapons) {
        QJsonObject weapon = value.toObject();
        int seasonNum = weapon["seasonNumber"].toInt();
        if (seasonNum > m_latestSeason) {
            m_latestSeason = seasonNum;
        }
    }
    
    // Build trait list from all weapons
    buildTraitList();
    
    // Apply user preference for auto-showing latest season
    m_showLatestSeason = m_autoShowLatestSeason;
    
    // Filter weapons based on current state
    filterWeapons();
    
    // Notify that weapons are loaded
    emit weaponsLoaded();
}

// Build unique trait list from all weapons' perkColumns
void WeaponSearchModel::buildTraitList()
{
    std::set<QString> uniqueTraits;
    
    for (const QJsonValue &value : m_allWeapons) {
        QJsonObject weapon = value.toObject();
        QJsonObject perkColumns = weapon["perkColumns"].toObject();
        
        // Iterate through all columns (1, 2, 3, 4, 7, etc.)
        for (const QString &columnKey : perkColumns.keys()) {
            QJsonArray perks = perkColumns[columnKey].toArray();
            for (const QJsonValue &perkVal : perks) {
                QString perkName = perkVal.toString().trimmed();
                if (!perkName.isEmpty()) {
                    uniqueTraits.insert(perkName);
                }
            }
        }

        // Origin traits are searchable through -t even when the backend does
        // not mirror them into perkColumns.
        for (const QJsonValue &originVal : weapon["originTraits"].toArray()) {
            QString originName = originVal.toString().trimmed();
            if (!originName.isEmpty()) {
                uniqueTraits.insert(originName);
            }
        }
    }
    
    m_allTraits.clear();
    for (const QString &trait : uniqueTraits) {
        m_allTraits.append(trait);
    }
    
    // Sort alphabetically for consistent ordering
    std::sort(m_allTraits.begin(), m_allTraits.end(), [](const QString &a, const QString &b) {
        return a.toLower() < b.toLower();
    });
}

// Find best matching trait name using fuzzy matching
// First checks perk aliases, then does fuzzy matching
QString WeaponSearchModel::findBestTraitMatch(const QString &partial) const
{
    if (partial.isEmpty() || m_allTraits.isEmpty()) {
        return QString();
    }
    
    QString partialLower = partial.toLower();
    
    // First, check if it's a known alias (highest priority)
    if (PERK_ALIASES.contains(partialLower)) {
        QString aliasTarget = PERK_ALIASES.value(partialLower);
        // Verify the alias target exists in our trait list
        for (const QString &trait : m_allTraits) {
            if (trait.toLower() == aliasTarget.toLower()) {
                return trait;  // Return the actual trait name from our list
            }
        }
        // If alias target not found exactly, continue with fuzzy matching on the alias target
        partialLower = aliasTarget.toLower();
    }
    
    QString bestMatch;
    int bestScore = 0;
    
    for (const QString &trait : m_allTraits) {
        QString traitLower = trait.toLower();
        
        // Exact match - highest priority
        if (traitLower == partialLower) {
            return trait;
        }
        
        // Starts with - very high priority
        if (traitLower.startsWith(partialLower)) {
            int score = 1000 + (100 - trait.length()); // Prefer shorter matches
            if (score > bestScore) {
                bestScore = score;
                bestMatch = trait;
            }
            continue;
        }
        
        // Contains - medium priority
        if (traitLower.contains(partialLower)) {
            int score = 500 + (100 - trait.length());
            if (score > bestScore) {
                bestScore = score;
                bestMatch = trait;
            }
            continue;
        }
        
        // Fuzzy match - lower priority
        int fuzzy = fuzzyScore(traitLower, partialLower);
        if (fuzzy > 400 && fuzzy > bestScore) { // Threshold for fuzzy match
            bestScore = fuzzy;
            bestMatch = trait;
        }
    }
    
    return bestMatch;
}

// Get the column number where a trait exists in a weapon's perkColumns
// Returns -1 if trait not found
int WeaponSearchModel::getTraitColumn(const QString &traitName, const QJsonObject &weapon) const
{
    QJsonObject perkColumns = weapon["perkColumns"].toObject();
    QString traitLower = traitName.toLower();
    
    for (const QString &columnKey : perkColumns.keys()) {
        QJsonArray perks = perkColumns[columnKey].toArray();
        for (const QJsonValue &perkVal : perks) {
            if (perkVal.toString().toLower() == traitLower) {
                return columnKey.toInt();
            }
        }
    }
    
    return -1;
}

// Helper: Get base weapon name by removing parenthetical suffixes like (Adept), (Harrowed), (Timelost)
QString WeaponSearchModel::getBaseWeaponName(const QString &name) const
{
    // Remove any parenthetical suffix: "Nullify (Adept)" -> "Nullify"
    QString baseName = name;
    int parenIndex = baseName.indexOf('(');
    if (parenIndex > 0) {
        baseName = baseName.left(parenIndex).trimmed();
    }
    return baseName.toLower();
}

// Helper: Check if weapon has a special suffix (Adept, Harrowed, Timelost, etc.)
// This checks both the API field and the weapon name
bool WeaponSearchModel::isAdeptWeapon(const QJsonObject &weapon) const
{
    // Check API's isAdept field first
    if (weapon["isAdept"].toBool()) {
        return true;
    }
    
    // Also check weapon name for (Adept), (Harrowed), (Timelost) suffixes
    // This covers cases where API field might not be set for all variants
    QString nameLower = weapon["name"].toString().toLower();
    return nameLower.contains("(adept)") || 
           nameLower.contains("(harrowed)") || 
           nameLower.contains("(timelost)");
}

void WeaponSearchModel::filterWeapons()
{
    beginResetModel();

    // Parse special flags first: -! (unique by name), -* (no limit), -h (holofoil only), -a (adept only), -e (exotic only)
    bool uniqueByName = false;    // -! flag: show only one weapon per name (prefer non-holofoil, non-adept)
    bool noLimit = false;         // -* flag: remove result limit
    bool holofoilOnly = false;    // -h flag or "holofoil"/"holo" keyword: show only holofoil weapons
    bool adeptOnly = false;       // -a flag or "adept" keyword: show only adept/harrowed/timelost weapons
    bool exoticOnly = false;      // -e flag or "exotic" keyword: show only exotic weapons
    bool craftableOnly = false;   // -c flag: show only craftable weapons
    QStringList sourceFilters;    // -s flag: filter by source (e.g., -s gambit, -s vog)
    QList<QPair<QString, int>> traitFilters;  // -t flag: filter by traits with column index
    QString damageTypeFilter;     // Damage type filter: solar, arc, void, stasis, strand, kinetic
    QString ammoTypeFilter;       // Ammo type filter: primary, special, heavy
    
    // Damage type keywords
    static const QMap<QString, QString> DAMAGE_TYPES = {
        {"solar", "Solar"},
        {"arc", "Arc"},
        {"void", "Void"},
        {"stasis", "Stasis"},
        {"strand", "Strand"},
        {"kinetic", "Kinetic"}
    };
    
    // Ammo type keywords
    static const QMap<QString, QString> AMMO_TYPES = {
        {"primary", "Primary"},
        {"special", "Special"},
        {"heavy", "Heavy"}
    };
    
    // Weapon type community abbreviations (e.g. "lfr" -> "linear fusion rifle")
    static const QMap<QString, QString> WEAPON_TYPE_ALIASES = {
        {"smg",                 "submachine gun"},
        {"hc",                  "hand cannon"},
        {"lmg",                 "machine gun"},
        {"mg",                  "machine gun"},
        {"ar",                  "auto rifle"},
        {"pr",                  "pulse rifle"},
        {"sr",                  "scout rifle"},
        {"sniper",              "sniper rifle"},
        {"sg",                  "shotgun"},
        {"shottie",             "shotgun"},
        {"shotty",              "shotgun"},
        {"fr",                  "fusion rifle"},
        {"lfr",                 "linear fusion rifle"},
        {"linear",              "linear fusion rifle"},
        {"gl",                  "grenade launcher"},
        {"rl",                  "rocket launcher"},
        {"rocket",              "rocket launcher"},
        {"sw",                  "sword"},
        {"tr",                  "trace rifle"},
        {"trace",               "trace rifle"},
        {"bow",                 "combat bow"},
        {"sa",                  "sidearm"},
        {"glaive",              "glaive"},
        {"handcannon",          "hand cannon"},
        {"autorifle",           "auto rifle"},
        {"pulserifle",          "pulse rifle"},
        {"scoutrifle",          "scout rifle"},
        {"sniperrifle",         "sniper rifle"},
        {"fusionrifle",         "fusion rifle"},
        {"linearfusion",        "linear fusion rifle"},
        {"linearfusionrifle",   "linear fusion rifle"},
        {"grenadelauncher",     "grenade launcher"},
        {"rocketlauncher",      "rocket launcher"},
        {"tracerifle",          "trace rifle"},
        {"machinegun",          "machine gun"},
        {"submachinegun",       "submachine gun"},
        {"combatbow",           "combat bow"},
    };
    
    QString queryLower = m_searchQuery.toLower().trimmed();
    
    // Parse -t trait filters - words after -t are trait names until we hit another flag
    // "-t firefly headstone -e" means filter by Firefly AND Headstone, with exotic flag
    QStringList traitTerms;
    int traitFlagIndex = queryLower.indexOf("-t ");
    if (traitFlagIndex != -1) {
        // Everything after "-t " initially
        QString traitPart = queryLower.mid(traitFlagIndex + 3).trimmed();
        // Remove the -t and everything after from the main query (we'll add back non-trait flags)
        queryLower = queryLower.left(traitFlagIndex).trimmed();
        
        // Parse trait part - handle quoted strings as single terms
        // "bait and switch" or 'bait and switch' should be treated as one trait, not three
        QStringList parts;
        
        // Match both double-quoted and single-quoted strings
        QRegularExpression quotedPattern("[\"']([^\"']+)[\"']");
        QRegularExpressionMatchIterator quotedMatches = quotedPattern.globalMatch(traitPart);
        
        // Extract quoted terms first
        while (quotedMatches.hasNext()) {
            QRegularExpressionMatch match = quotedMatches.next();
            QString quotedTerm = match.captured(1).trimmed();
            if (!quotedTerm.isEmpty()) {
                parts.append(quotedTerm);
            }
        }
        
        // Remove quoted parts from traitPart to get remaining unquoted terms
        QString unquotedPart = traitPart;
        unquotedPart.remove(quotedPattern);
        
        // Split remaining unquoted part by spaces
        QStringList unquotedParts = unquotedPart.split(' ', Qt::SkipEmptyParts);
        
        QStringList flagsToAddBack;
        
        for (const QString &part : unquotedParts) {
            // Check if it looks like a flag (-e, -h, -a, -!, -*, -s)
            // Flags are: single dash followed by flag characters
            if (part.startsWith("-") && part.length() >= 2) {
                // Check if it's a known flag pattern
                QString flagChars = part.mid(1);
                bool isFlag = true;
                
                // Check if all characters are valid flag chars or if it's -s
                if (flagChars == "s" || part.startsWith("-s ")) {
                    // Source flag - add back to query with everything after it
                    int idx = unquotedParts.indexOf(part);
                    for (int i = idx; i < unquotedParts.size(); ++i) {
                        flagsToAddBack.append(unquotedParts[i]);
                    }
                    break;
                }
                
                // Check for standard flags (!*haec)
                for (const QChar &c : flagChars) {
                    if (c != '!' && c != '*' && c != 'h' && c != 'a' && c != 'e' && c != 'c') {
                        isFlag = false;
                        break;
                    }
                }
                
                if (isFlag) {
                    flagsToAddBack.append(part);
                    continue;
                }
            }
            
            // Not a flag, treat as trait term
            // Skip lone quote characters but allow short terms like "ff", "kc"
            QString cleanPart = part;
            cleanPart.remove('"');  // Remove any stray double quote characters
            cleanPart.remove('\''); // Remove any stray single quote characters
            cleanPart = cleanPart.trimmed();
            if (!cleanPart.isEmpty() && !parts.contains(cleanPart)) {
                parts.append(cleanPart);
            }
        }
        
        // Now parts contains both quoted (full phrases) and unquoted terms
        traitTerms = parts;
        
        // Add back any flags that were after -t
        if (!flagsToAddBack.isEmpty()) {
            queryLower = queryLower + " " + flagsToAddBack.join(" ");
            queryLower = queryLower.trimmed();
        }
    }
    
    // Match partial trait names to full trait names (max 4 traits)
    const int MAX_TRAIT_FILTERS = 4;
    for (const QString &term : traitTerms) {
        if (traitFilters.size() >= MAX_TRAIT_FILTERS) {
            break; // Stop at max trait limit
        }
        QString matchedTrait = findBestTraitMatch(term);
        if (!matchedTrait.isEmpty()) {
            // Check if we already have this trait
            bool exists = false;
            for (const auto &pair : traitFilters) {
                if (pair.first.toLower() == matchedTrait.toLower()) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                traitFilters.append(qMakePair(matchedTrait, -1)); // Column will be determined per weapon
            }
        }
    }
    
    // Parse -s source filters (e.g., "-s gambit", "-s vog", "-s trials")
    // Can have multiple: "-s gambit -s trials"
    QRegularExpression sourcePattern("-s\\s+(\\S+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator sourceMatches = sourcePattern.globalMatch(queryLower);
    while (sourceMatches.hasNext()) {
        QRegularExpressionMatch match = sourceMatches.next();
        QString sourceAlias = match.captured(1).toLower();
        if (!sourceFilters.contains(sourceAlias)) {
            sourceFilters.append(sourceAlias);
        }
    }
    // Remove source patterns from query
    queryLower = queryLower.remove(sourcePattern).trimmed();
    
    // Check for flags in various formats:
    // - Combined: -!*ha, -h!*, etc.
    // - Separate: -h -* -!, -h-*-!, etc.
    // Pattern matches any -X where X contains !, *, h, a, or e
    QRegularExpression flagPattern("-([!*haec]+)");
    QRegularExpressionMatchIterator flagMatches = flagPattern.globalMatch(queryLower);
    
    while (flagMatches.hasNext()) {
        QRegularExpressionMatch match = flagMatches.next();
        QString flags = match.captured(1);
        if (flags.contains('!')) {
            uniqueByName = true;
        }
        if (flags.contains('*')) {
            noLimit = true;
        }
        if (flags.contains('h')) {
            holofoilOnly = true;
        }
        if (flags.contains('a')) {
            adeptOnly = true;
        }
        if (flags.contains('e')) {
            exoticOnly = true;
        }
        if (flags.contains('c')) {
            craftableOnly = true;
        }
    }
    
    // Remove all flag patterns from the query
    queryLower = queryLower.remove(flagPattern).trimmed();
    // Clean up any leftover dashes from patterns like -h-*-!
    queryLower = queryLower.replace(QRegularExpression("-+"), " ").trimmed();
    
    // Check for "holofoil" or "holo" keyword
    if (queryLower.contains("holofoil")) {
        holofoilOnly = true;
        queryLower = queryLower.replace("holofoil", "").trimmed();
    } else if (queryLower.contains("holo")) {
        holofoilOnly = true;
        queryLower = queryLower.replace("holo", "").trimmed();
    }
    
    // Check for "adept" keyword
    if (queryLower == "adept" || queryLower.startsWith("adept ") || queryLower.endsWith(" adept") || queryLower.contains(" adept ")) {
        adeptOnly = true;
        queryLower = queryLower.replace(QRegularExpression("\\badept\\b"), "").trimmed();
    }
    
    // Check for "exotic" keyword
    if (queryLower == "exotic" || queryLower.startsWith("exotic ") || queryLower.endsWith(" exotic") || queryLower.contains(" exotic ")) {
        exoticOnly = true;
        queryLower = queryLower.replace(QRegularExpression("\\bexotic\\b"), "").trimmed();
    }
    
    // Check for damage type keywords (solar, arc, void, stasis, strand, kinetic)
    for (auto it = DAMAGE_TYPES.constBegin(); it != DAMAGE_TYPES.constEnd(); ++it) {
        QString keyword = it.key();
        QRegularExpression pattern("\\b" + keyword + "\\b", QRegularExpression::CaseInsensitiveOption);
        if (queryLower.contains(pattern)) {
            damageTypeFilter = it.value();
            queryLower = queryLower.remove(pattern).simplified();
            break;  // Only one damage type filter at a time
        }
    }
    
    // Check for ammo type keywords (primary, special, heavy)
    for (auto it = AMMO_TYPES.constBegin(); it != AMMO_TYPES.constEnd(); ++it) {
        QString keyword = it.key();
        QRegularExpression pattern("\\b" + keyword + "\\b", QRegularExpression::CaseInsensitiveOption);
        if (queryLower.contains(pattern)) {
            ammoTypeFilter = it.value();
            queryLower = queryLower.remove(pattern).simplified();
            break;  // Only one ammo type filter at a time
        }
    }

    // Equipment slot keywords. "kinetic" is ambiguous with the damage type,
    // so it is accepted as either Kinetic damage or Kinetic slot below.
    QString equipmentSlotFilter;
    bool kineticDamageOrSlot = (damageTypeFilter.compare("Kinetic", Qt::CaseInsensitive) == 0);
    for (const QString &slot : {QString("energy"), QString("power")}) {
        QRegularExpression pattern("\\b" + slot + "\\b", QRegularExpression::CaseInsensitiveOption);
        if (queryLower.contains(pattern)) {
            equipmentSlotFilter = slot;
            queryLower = queryLower.remove(pattern).simplified();
            break;
        }
    }
    
    // Build a map of source aliases to display names for active filters
    // We need to scan weapons to find matching sourceDisplayNames
    // Priority: exact match > starts-with match > contains match
    QStringList matchedSourceDisplayNames;
    if (!sourceFilters.isEmpty()) {
        // For each filter, find the best matching source(s)
        // Exact match = only that source, starts-with/contains = all matching sources
        for (const QString &filterAlias : sourceFilters) {
            std::set<QString> exactMatches;
            std::set<QString> startsWithMatches;
            std::set<QString> containsMatches;
            
            for (const QJsonValue &value : m_allWeapons) {
                QJsonObject weapon = value.toObject();
                QJsonArray aliases = weapon["sourceSearchAliases"].toArray();
                QString displayName = weapon["sourceDisplayName"].toString();
                
                if (displayName.isEmpty()) continue;
                
                for (const QJsonValue &aliasVal : aliases) {
                    QString alias = aliasVal.toString().toLower();
                    
                    // Check for exact match first (highest priority)
                    if (alias == filterAlias) {
                        exactMatches.insert(displayName);
                        break;
                    }
                    // Check for starts-with match (medium priority)
                    else if (alias.startsWith(filterAlias)) {
                        startsWithMatches.insert(displayName);
                    }
                    // Check for contains match (lowest priority)
                    else if (alias.contains(filterAlias) || filterAlias.contains(alias)) {
                        containsMatches.insert(displayName);
                    }
                }
            }
            
            // Use matches in priority order: exact > starts-with > contains
            std::set<QString>* bestMatches = nullptr;
            if (!exactMatches.empty()) {
                bestMatches = &exactMatches;
            } else if (!startsWithMatches.empty()) {
                bestMatches = &startsWithMatches;
            } else if (!containsMatches.empty()) {
                bestMatches = &containsMatches;
            }
            
            if (bestMatches) {
                for (const QString &displayName : *bestMatches) {
                    if (!matchedSourceDisplayNames.contains(displayName)) {
                        matchedSourceDisplayNames.append(displayName);
                    }
                }
            }
        }
    }
    
    // Update active source filters for QML
    if (m_activeSourceFilters != matchedSourceDisplayNames) {
        m_activeSourceFilters = matchedSourceDisplayNames;
        emit activeSourceFiltersChanged();
    }
    
    // Update active trait filters for QML
    // Create variant list with name and color index based on typical column mapping
    QVariantList traitFiltersList;
    for (int i = 0; i < traitFilters.size(); ++i) {
        QVariantMap traitInfo;
        traitInfo["name"] = traitFilters[i].first;
        traitInfo["colorIndex"] = i % 4;  // Cycle through 4 colors
        traitFiltersList.append(traitInfo);
    }
    if (m_activeTraitFilters != traitFiltersList) {
        m_activeTraitFilters = traitFiltersList;
        emit activeTraitFiltersChanged();
    }
    
    // Helper lambda to check if a weapon matches the source filters
    // Uses the same priority logic: exact > starts-with > contains
    auto matchesSourceFilter = [&sourceFilters, &matchedSourceDisplayNames](const QJsonObject &weapon) -> bool {
        if (sourceFilters.isEmpty()) return true;
        
        // If we found specific sources, only match those
        QString weaponSource = weapon["sourceDisplayName"].toString();
        if (!matchedSourceDisplayNames.isEmpty()) {
            return matchedSourceDisplayNames.contains(weaponSource);
        }
        
        // Fallback to alias matching
        QJsonArray aliases = weapon["sourceSearchAliases"].toArray();
        for (const QString &filterAlias : sourceFilters) {
            bool found = false;
            for (const QJsonValue &aliasVal : aliases) {
                QString alias = aliasVal.toString().toLower();
                if (alias == filterAlias || alias.contains(filterAlias) || filterAlias.contains(alias)) {
                    found = true;
                    break;
                }
            }
            if (!found) return false; // All filters must match
        }
        return true;
    };
    
    // Helper lambda to check if a weapon matches the trait filters
    // Each trait must be in a DIFFERENT column (one perk per column rule)
    auto matchesTraitFilter = [this, &traitFilters](const QJsonObject &weapon) -> bool {
        if (traitFilters.isEmpty()) return true;
        
        QJsonObject perkColumns = weapon["perkColumns"].toObject();
        std::set<int> usedColumns;  // Track which columns we've matched
        
        for (const auto &traitPair : traitFilters) {
            QString traitName = traitPair.first.toLower();
            bool found = false;
            
            // Look for this trait in any column
            for (const QString &columnKey : perkColumns.keys()) {
                int columnNum = columnKey.toInt();
                
                // Skip if this column is already used by another trait
                if (usedColumns.find(columnNum) != usedColumns.end()) {
                    continue;
                }
                
                QJsonArray perks = perkColumns[columnKey].toArray();
                for (const QJsonValue &perkVal : perks) {
                    if (perkVal.toString().toLower() == traitName) {
                        found = true;
                        usedColumns.insert(columnNum);
                        break;
                    }
                }
                
                if (found) break;
            }

            if (!found) {
                for (const QJsonValue &originVal : weapon["originTraits"].toArray()) {
                    if (originVal.toString().toLower() == traitName) {
                        found = true;
                        break;
                    }
                }
            }
            
            if (!found) return false;  // Trait not found in any available column
        }
        
        return true;
    };

    // If query is empty (after removing flags), show latest season weapons with filters applied
    // The -* flag allows showing ALL weapons (not just latest season)
    // Filter flags (-h, -a, -e) when used alone should search ALL weapons
    // -! (unique) alone still shows latest season only
    bool hasFilterFlags = holofoilOnly || adeptOnly || exoticOnly || craftableOnly;
    bool hasDamageOrAmmoFilter = !damageTypeFilter.isEmpty() || !ammoTypeFilter.isEmpty() || !equipmentSlotFilter.isEmpty();
    bool showAllWeapons = noLimit || !sourceFilters.isEmpty() || !traitFilters.isEmpty() || hasFilterFlags || hasDamageOrAmmoFilter; // -* flag, -s flag, -t flag, damage/ammo type, or filter flags shows all weapons
    
    if (queryLower.isEmpty() && !showAllWeapons) {
        if (!m_showLatestSeason) {
            // Show nothing when not searching and showLatestSeason is false
            m_filteredWeapons = QJsonArray();
        } else {
            // Show only latest season weapons, sorted alphabetically by name
            QList<QPair<QString, QJsonValue>> latestSeasonWeapons;
            std::set<std::string> seenWeaponNames;
            
            for (const QJsonValue &value : m_allWeapons) {
                QJsonObject weapon = value.toObject();
                int seasonNum = weapon["seasonNumber"].toInt();
                
                if (seasonNum == m_latestSeason) {
                    QString name = weapon["name"].toString();
                    bool isHolofoil = weapon["isHolofoil"].toBool();
                    bool isExotic = weapon["isExotic"].toBool();
                    bool isAdept = isAdeptWeapon(weapon);
                    
                    // Apply holofoil filter
                    if (holofoilOnly && !isHolofoil) {
                        continue; // Skip non-holofoil weapons when holofoil filter is active
                    }
                    
                    // Apply exotic filter
                    if (exoticOnly && !isExotic) {
                        continue; // Skip non-exotic weapons when exotic filter is active
                    }
                    
                    // Apply adept filter
                    if (adeptOnly && !isAdept) {
                        continue; // Skip non-adept weapons when adept filter is active
                    }

                    if (craftableOnly && !weapon["isCraftable"].toBool()) {
                        continue;
                    }
                    
                    // Apply damage type filter
                    if (!damageTypeFilter.isEmpty()) {
                        QString weaponDamage = weapon["damageType"].toString();
                        QString weaponSlot = weapon["equipmentSlot"].toString();
                        bool matchesDamage = weaponDamage.compare(damageTypeFilter, Qt::CaseInsensitive) == 0;
                        bool matchesKineticSlot = kineticDamageOrSlot && weaponSlot.compare("Kinetic", Qt::CaseInsensitive) == 0;
                        if (!matchesDamage && !matchesKineticSlot) {
                            continue;
                        }
                    }

                    if (!equipmentSlotFilter.isEmpty() &&
                        weapon["equipmentSlot"].toString().compare(equipmentSlotFilter, Qt::CaseInsensitive) != 0) {
                        continue;
                    }
                    
                    // Apply ammo type filter
                    if (!ammoTypeFilter.isEmpty()) {
                        QString weaponAmmo = weapon["ammoType"].toString();
                        if (weaponAmmo.toLower() != ammoTypeFilter.toLower()) {
                            continue;
                        }
                    }
                    
                    // If uniqueByName is enabled, skip if we've seen this base name
                    // When holofoilOnly is active, we keep holofoil versions
                    // When not holofoilOnly, prefer non-holofoil, non-adept versions
                    if (uniqueByName) {
                        // Use base name (without Adept/Harrowed/Timelost suffix) for comparison
                        std::string nameKey = getBaseWeaponName(name).toStdString();
                        if (seenWeaponNames.count(nameKey) > 0) {
                            continue;
                        }
                        
                        // If holofoilOnly is active, we're already filtering to holofoil only
                        // So just add this weapon (first holofoil with this base name)
                        // Same for adeptOnly - just add the first matching weapon
                        if (!holofoilOnly && !adeptOnly) {
                            // If this is holofoil or adept, check if a base version exists
                            if (isHolofoil || isAdept) {
                                bool hasBaseVersion = false;
                                for (const QJsonValue &other : m_allWeapons) {
                                    QJsonObject otherWeapon = other.toObject();
                                    QString otherName = otherWeapon["name"].toString();
                                    if (otherWeapon["seasonNumber"].toInt() == m_latestSeason &&
                                        getBaseWeaponName(otherName) == getBaseWeaponName(name) &&
                                        !otherWeapon["isHolofoil"].toBool() &&
                                        !isAdeptWeapon(otherWeapon)) {
                                        hasBaseVersion = true;
                                        break;
                                    }
                                }
                                if (hasBaseVersion) {
                                    continue; // Skip variant, we'll add base version
                                }
                            }
                        }
                        seenWeaponNames.insert(nameKey);
                    }
                    
                    latestSeasonWeapons.append({name, value});
                }
            }
            
            // Sort alphabetically by name
            std::sort(latestSeasonWeapons.begin(), latestSeasonWeapons.end(),
                      [](const auto &a, const auto &b) { return a.first.toLower() < b.first.toLower(); });
            
            m_filteredWeapons = QJsonArray();
            for (const auto &pair : latestSeasonWeapons) {
                m_filteredWeapons.append(pair.second);
            }
        }
    } else {
        // Search mode: support multi-term search (e.g., "pulse micro-missile")
        // Each term must match at least one field
        QList<std::tuple<int, int, QString, QJsonObject>> scoredWeapons; // score, seasonNumber, name, weapon with matchedFields
        
        // First, check if query is a season-specific search like "Season 28" or "s28"
        bool isSeasonSearch = false;
        int searchedSeasonNum = -1;
        
        // Check for "Season X" or "Season of..." pattern
        QRegularExpression seasonPattern("^season\\s*(\\d+)$", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch seasonMatch = seasonPattern.match(queryLower);
        if (seasonMatch.hasMatch()) {
            isSeasonSearch = true;
            searchedSeasonNum = seasonMatch.captured(1).toInt();
        }
        
        // Check for "sX" pattern (e.g., "s28")
        QRegularExpression sPattern("^s(\\d+)$", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch sMatch = sPattern.match(queryLower);
        if (sMatch.hasMatch()) {
            isSeasonSearch = true;
            searchedSeasonNum = sMatch.captured(1).toInt();
        }
        
        // Split query into terms for normal search
        QStringList searchTerms = queryLower.split(' ', Qt::SkipEmptyParts);
        
        // Enable ID search only when the complete numeric query exactly equals
        // a hash in the current dataset. Otherwise it remains a normal name/RPM
        // term; partial hashes are never considered matches.
        bool isIdSearch = false;
        quint64 searchedHash = 0;
        {
            bool ok = false;
            quint64 hashVal = queryLower.trimmed().toULongLong(&ok);
            if (ok && !queryLower.trimmed().isEmpty()) {
                for (const QJsonValue &value : m_allWeapons) {
                    QJsonValue hashValue = value.toObject()["hash"];
                    quint64 weaponHash = hashValue.isDouble()
                        ? static_cast<quint64>(hashValue.toDouble())
                        : hashValue.toString().toULongLong();
                    if (weaponHash == hashVal) {
                        isIdSearch = true;
                        searchedHash = hashVal;
                        break;
                    }
                }
            }
        }
        
        // Track seen weapon base names for uniqueByName filter
        std::set<QString> seenWeaponNames;
        
        for (const QJsonValue &value : m_allWeapons) {
            QJsonObject weapon = value.toObject();
            bool isHolofoil = weapon["isHolofoil"].toBool();
            bool isExotic = weapon["isExotic"].toBool();
            QString weaponName = weapon["name"].toString();
            bool isAdept = isAdeptWeapon(weapon);
            QString baseName = getBaseWeaponName(weaponName);
            
            // Apply holofoil filter
            if (holofoilOnly && !isHolofoil) {
                continue; // Skip non-holofoil weapons when holofoil filter is active
            }
            
            // Apply exotic filter
            if (exoticOnly && !isExotic) {
                continue; // Skip non-exotic weapons when exotic filter is active
            }
            
            // Apply adept filter
            if (adeptOnly && !isAdept) {
                continue; // Skip non-adept weapons when adept filter is active
            }

            if (craftableOnly && !weapon["isCraftable"].toBool()) {
                continue;
            }
            
            // Apply damage type filter
            if (!damageTypeFilter.isEmpty()) {
                QString weaponDamage = weapon["damageType"].toString();
                QString weaponSlot = weapon["equipmentSlot"].toString();
                bool matchesDamage = weaponDamage.compare(damageTypeFilter, Qt::CaseInsensitive) == 0;
                bool matchesKineticSlot = kineticDamageOrSlot && weaponSlot.compare("Kinetic", Qt::CaseInsensitive) == 0;
                if (!matchesDamage && !matchesKineticSlot) {
                    continue;
                }
            }

            if (!equipmentSlotFilter.isEmpty() &&
                weapon["equipmentSlot"].toString().compare(equipmentSlotFilter, Qt::CaseInsensitive) != 0) {
                continue;
            }
            
            // Apply ammo type filter
            if (!ammoTypeFilter.isEmpty()) {
                QString weaponAmmo = weapon["ammoType"].toString();
                if (weaponAmmo.toLower() != ammoTypeFilter.toLower()) {
                    continue;
                }
            }
            
            // Apply source filter
            if (!matchesSourceFilter(weapon)) {
                continue; // Skip weapons that don't match source filter
            }
            
            // Apply trait filter
            if (!matchesTraitFilter(weapon)) {
                continue; // Skip weapons that don't match trait filter
            }
            
            // Note: uniqueByName filter is applied AFTER sorting to prefer newer season weapons
            // See the result collection loop below
            
            QString name = weaponName.toLower();
            QString weaponType = weapon["weaponType"].toString().toLower();
            QString frameType = weapon["frameType"].toString().toLower();
            QString equipmentSlot = weapon["equipmentSlot"].toString().toLower();
            QString seasonName = weapon["seasonName"].toString().toLower();
            QString season = weapon["season"].toString().toLower(); // "Season X" format
            QString seasonDisplay = weapon["seasonDisplay"].toString().toLower(); // Full display name
            int seasonNum = weapon["seasonNumber"].toInt();
            QString seasonNumStr = QString::number(seasonNum);
            
            // If this is a specific season search, only include weapons from that season
            if (isSeasonSearch) {
                if (seasonNum == searchedSeasonNum) {
                    weapon["matchedField"] = "seasonNumber";
                    // Sort by name alphabetically within the season
                    scoredWeapons.append({1000, seasonNum, name, weapon});
                }
                continue; // Skip normal term matching for season-specific searches
            }
            
            // If this is an ID/hash search, match only the weapon with the exact hash
            if (isIdSearch) {
                quint64 weaponHash = weapon["hash"].isDouble()
                    ? static_cast<quint64>(weapon["hash"].toDouble())
                    : weapon["hash"].toString().toULongLong();
                if (weaponHash == searchedHash) {
                    weapon["matchedField"] = "id";
                    scoredWeapons.append({10000, seasonNum, name, weapon});
                }
                continue;
            }
            
            // If only flags were provided (no search terms), show all weapons
            if (searchTerms.isEmpty()) {
                weapon["matchedField"] = "";
                scoredWeapons.append({500, seasonNum, name, weapon});
                continue;
            }
            
            // For each term, check if it matches any field
            bool allTermsMatch = true;
            int totalScore = 0;
            int nameMatchCount = 0;
            int fullNameScore = fuzzyScore(name, queryLower);
            QStringList matchedFields;
            
            for (const QString &term : searchTerms) {
                int termScore = 0;
                QString termMatchedField = "";
                
                // Priority order (highest to lowest):
                // 1. Name (weapon name) - 1.0x + 1000 bonus (highest priority)
                // 2. Frame type - 0.97x (partial matching, e.g. "adaptive" -> "Adaptive Frame")
                // 3. Weapon type - 0.85x (with aliases, e.g. "lfr", "hc", "sniper")
                // 4. Season number ("Season X" format) - 0.6x
                // 5. Season name/display - 0.5x (lowest priority)
                
                // Check season name first (lowest priority - 0.5x multiplier)
                int seasonNameScore = fuzzyScore(seasonName, term);
                if (seasonNameScore > 0) {
                    termScore = static_cast<int>(seasonNameScore * 0.5);
                    termMatchedField = "seasonName";
                }
                
                // Check seasonDisplay (full display name like "Lightfall • Season of Defiance")
                int seasonDisplayScore = fuzzyScore(seasonDisplay, term);
                if (seasonDisplayScore > 0 && static_cast<int>(seasonDisplayScore * 0.5) > termScore) {
                    termScore = static_cast<int>(seasonDisplayScore * 0.5);
                    termMatchedField = "seasonName";
                }
                
                // Check season ("Season X" format) - higher than seasonName (0.6x)
                int seasonScore = fuzzyScore(season, term);
                if (seasonScore > 0 && static_cast<int>(seasonScore * 0.6) > termScore) {
                    termScore = static_cast<int>(seasonScore * 0.6);
                    termMatchedField = "seasonNumber";
                }
                
                // Check season number exact match (bonus for exact "28" or "s28")
                if (seasonNumStr == term || term == "s" + seasonNumStr) {
                    int exactSeasonScore = static_cast<int>(700 * 0.6);
                    if (exactSeasonScore > termScore) {
                        termScore = exactSeasonScore;
                        termMatchedField = "seasonNumber";
                    }
                }
                
                // Check weapon type - 0.85x (with community aliases)
                int weaponTypeScore = fuzzyScore(weaponType, term);
                if (WEAPON_TYPE_ALIASES.contains(term) && weaponType.contains(WEAPON_TYPE_ALIASES.value(term))) {
                    weaponTypeScore = qMax(weaponTypeScore, 900); // Alias exact match — high score
                }
                if (weaponTypeScore > 0 && static_cast<int>(weaponTypeScore * 0.85) > termScore) {
                    termScore = static_cast<int>(weaponTypeScore * 0.85);
                    termMatchedField = "weaponType";
                }
                
                // Check frame type - 0.97x (highest after name, partial matching prioritized)
                int frameTypeScore = fuzzyScore(frameType, term);
                if (frameTypeScore > 0 && static_cast<int>(frameTypeScore * 0.97) > termScore) {
                    termScore = static_cast<int>(frameTypeScore * 0.97);
                    termMatchedField = "frameType";
                }

                // Check equipment slot (Kinetic, Energy, Power).
                int equipmentSlotScore = fuzzyScore(equipmentSlot, term);
                if (equipmentSlotScore > 0 && static_cast<int>(equipmentSlotScore * 0.8) > termScore) {
                    termScore = static_cast<int>(equipmentSlotScore * 0.8);
                    termMatchedField = "equipmentSlot";
                }

                // RPM is an additional candidate for terms containing at least
                // two digits, so numeric weapon names remain searchable too.
                static const QRegularExpression rpmPattern("^\\d{2,4}$");
                if (rpmPattern.match(term).hasMatch() && weapon["rpm"].toInt() == term.toInt()) {
                    if (900 > termScore) {
                        termScore = 900;
                        termMatchedField = "rpm";
                    }
                }
                
                // Check name (highest priority - 1.0x + 1000 bonus)
                int nameScore = fuzzyScore(name, term);
                if (nameScore > 0 && (nameScore + 1000) > termScore) {
                    termScore = nameScore + 1000;
                    termMatchedField = "name";
                }
                
                if (termScore == 0) {
                    allTermsMatch = false;
                    break;
                }
                
                totalScore += termScore;
                if (termMatchedField == "name") {
                    nameMatchCount++;
                }
                if (!termMatchedField.isEmpty() && termMatchedField != "name" && !matchedFields.contains(termMatchedField)) {
                    matchedFields.append(termMatchedField);
                }
            }
            
            if (allTermsMatch && totalScore > 0) {
                // Store matched fields as comma-separated string
                weapon["matchedField"] = matchedFields.join(",");
                weapon["_nameMatchCount"] = nameMatchCount;
                weapon["_fullNameScore"] = fullNameScore;
                
                scoredWeapons.append({totalScore, seasonNum, name, weapon});
            }
        }

        // An exact full-name match wins even across seasons. Otherwise,
        // name-field matches win over other fields and newer seasons come
        // before fine-grained fuzzy relevance.
        std::sort(scoredWeapons.begin(), scoredWeapons.end(),
                  [](const auto &a, const auto &b) {
                      const QJsonObject &weaponA = std::get<3>(a);
                      const QJsonObject &weaponB = std::get<3>(b);

                      int nameMatchesA = weaponA["_nameMatchCount"].toInt();
                      int nameMatchesB = weaponB["_nameMatchCount"].toInt();

                      int fullNameScoreA = weaponA["_fullNameScore"].toInt();
                      int fullNameScoreB = weaponB["_fullNameScore"].toInt();
                      bool exactNameA = (fullNameScoreA == 1000);
                      bool exactNameB = (fullNameScoreB == 1000);
                      if (exactNameA != exactNameB) {
                          return exactNameA;
                      }

                      if (nameMatchesA != nameMatchesB) {
                          return nameMatchesA > nameMatchesB;
                      }

                      int seasonA = std::get<1>(a);
                      int seasonB = std::get<1>(b);
                      if (seasonA != seasonB) {
                          return seasonA > seasonB;
                      }

                      if (fullNameScoreA != fullNameScoreB) {
                          return fullNameScoreA > fullNameScoreB;
                      }

                      QString nameA = std::get<2>(a).toLower();
                      QString nameB = std::get<2>(b).toLower();
                      if (nameA != nameB) {
                          return nameA < nameB;
                      }

                      return std::get<0>(a) > std::get<0>(b);
                  });

        // Determine result limit:
        // - noLimit flag (-*): no limit
        // - isSeasonSearch (s27, Season 27): no limit  
        // - sourceFilters active (-s gambit): no limit
        // - traitFilters active (-t firefly): no limit
        // - damageTypeFilter or ammoTypeFilter active: no limit
        // - holofoilOnly, uniqueByName, adeptOnly, or exoticOnly with no other search: no limit
        // - Otherwise: limit to 50
        m_filteredWeapons = QJsonArray();
        bool shouldRemoveLimit = noLimit || isSeasonSearch || isIdSearch || !sourceFilters.isEmpty() || !traitFilters.isEmpty() || hasDamageOrAmmoFilter || ((holofoilOnly || uniqueByName || adeptOnly || exoticOnly || craftableOnly) && searchTerms.isEmpty());
        int maxResults = shouldRemoveLimit ? scoredWeapons.size() : qMin(50, static_cast<int>(scoredWeapons.size()));
        
        // Apply uniqueByName filter AFTER sorting - this ensures newer season weapons are preferred
        // Since weapons are sorted by season first, the first occurrence of
        // each base name is the newest version.
        std::set<QString> seenUniqueNames;
        
        for (int i = 0; i < scoredWeapons.size(); ++i) {
            if (!uniqueByName && static_cast<int>(m_filteredWeapons.size()) >= maxResults) {
                break;
            }
            
            QJsonObject weapon = std::get<3>(scoredWeapons[i]);
            
            if (uniqueByName) {
                QString weaponName = weapon["name"].toString();
                QString baseName = getBaseWeaponName(weaponName);
                
                // Skip if we've already seen this base weapon name
                if (seenUniqueNames.find(baseName) != seenUniqueNames.end()) {
                    continue;
                }
                seenUniqueNames.insert(baseName);
            }
            
            m_filteredWeapons.append(weapon);
        }
    }

    endResetModel();
}

// Normalize text by replacing hyphens with spaces for better matching
QString WeaponSearchModel::normalizeText(const QString &text) const
{
    QString normalized = text;
    normalized.replace('-', ' ');
    normalized.replace('_', ' ');
    normalized.replace('\'', ' ');
    normalized.replace('"', ' ');
    
    // Unicode normalization: decompose characters (NFD) and remove diacritics
    // This handles all languages: Turkish İ/ı, German ü/ö, French é/è, Spanish ñ, etc.
    normalized = normalized.normalized(QString::NormalizationForm_D);
    
    // Remove all combining diacritical marks (Unicode category Mn)
    QString result;
    result.reserve(normalized.size());
    for (const QChar &ch : normalized) {
        // Keep only base characters, skip combining marks (category Mark_NonSpacing)
        if (ch.category() != QChar::Mark_NonSpacing) {
            result.append(ch);
        }
    }
    
    // Special handling for Turkish dotless ı (doesn't decompose)
    result.replace(QChar(0x0131), 'i');  // ı -> i
    
    // Remove extra spaces and convert to lowercase
    return result.simplified().toLower();
}

// Levenshtein distance calculation for typo tolerance (Fuse.js style)
int WeaponSearchModel::levenshteinDistance(const QString &s1, const QString &s2) const
{
    const int len1 = s1.length();
    const int len2 = s2.length();
    
    // Quick optimization for empty strings
    if (len1 == 0) return len2;
    if (len2 == 0) return len1;
    
    // Use single row optimization for memory efficiency
    QVector<int> prevRow(len2 + 1);
    QVector<int> currRow(len2 + 1);
    
    // Initialize first row
    for (int j = 0; j <= len2; ++j) {
        prevRow[j] = j;
    }
    
    for (int i = 1; i <= len1; ++i) {
        currRow[0] = i;
        
        for (int j = 1; j <= len2; ++j) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            currRow[j] = qMin(qMin(
                currRow[j - 1] + 1,      // insertion
                prevRow[j] + 1),         // deletion
                prevRow[j - 1] + cost    // substitution
            );
        }
        
        std::swap(prevRow, currRow);
    }
    
    return prevRow[len2];
}

// Fuse.js-style fuzzy matching with configurable threshold
// Returns a score between 0.0 (no match) and 1.0 (perfect match)
double WeaponSearchModel::fuseFuzzyMatch(const QString &text, const QString &pattern) const
{
    if (pattern.isEmpty()) return 1.0;
    if (text.isEmpty()) return 0.0;
    
    QString normalizedText = normalizeText(text);
    QString normalizedPattern = normalizeText(pattern);
    
    // Perfect match
    if (normalizedText == normalizedPattern) {
        return 1.0;
    }
    
    // Check if text STARTS with pattern - highest priority after perfect match
    if (normalizedText.startsWith(normalizedPattern)) {
        // Longer pattern relative to text = higher score
        double lengthRatio = static_cast<double>(normalizedPattern.length()) / normalizedText.length();
        return 0.96 + (lengthRatio * 0.03);  // Range: 0.96 - 0.99
    }
    
    // Check if any WORD starts with pattern
    QStringList words = normalizedText.split(' ', Qt::SkipEmptyParts);
    for (int i = 0; i < words.size(); ++i) {
        if (words[i].startsWith(normalizedPattern)) {
            if (i == 0) {
                // First word starts with pattern - high priority but below full name match
                double lengthRatio = static_cast<double>(normalizedPattern.length()) / words[i].length();
                return 0.92 + (lengthRatio * 0.03);  // Range: 0.92 - 0.95
            } else {
                // Later word starts with pattern - much lower priority
                double wordPositionPenalty = static_cast<double>(i) / words.size() * 0.05;
                return 0.65 - wordPositionPenalty;  // Range: 0.60 - 0.65
            }
        }
    }
    
    // Contains match - lower priority than starts-with
    int containsIndex = normalizedText.indexOf(normalizedPattern);
    if (containsIndex != -1) {
        // Earlier position = higher score
        double positionBonus = 1.0 - (static_cast<double>(containsIndex) / normalizedText.length() * 0.1);
        // Longer pattern relative to text = higher score
        double lengthRatio = static_cast<double>(normalizedPattern.length()) / normalizedText.length();
        return 0.50 + (positionBonus * 0.08) + (lengthRatio * 0.04);  // Range: 0.50 - 0.62
    }
    
    // Prefix match on any word with typo tolerance
    for (const QString &word : words) {
        if (normalizedPattern.length() <= word.length()) {
            QString wordPrefix = word.left(normalizedPattern.length());
            int dist = levenshteinDistance(wordPrefix, normalizedPattern);
            int maxDist = qMax(1, normalizedPattern.length() / 3); // Allow ~33% errors
            if (dist <= maxDist) {
                double score = 0.7 * (1.0 - static_cast<double>(dist) / normalizedPattern.length());
                return score;
            }
        }
    }
    
    // Levenshtein distance on full text for typo tolerance
    // Only consider if pattern is reasonably sized
    if (normalizedPattern.length() >= 3) {
        // Check each word for close matches
        for (const QString &word : words) {
            if (qAbs(word.length() - normalizedPattern.length()) <= 2) {
                int dist = levenshteinDistance(word, normalizedPattern);
                int maxAllowedDist = qMax(1, normalizedPattern.length() / 3);
                
                if (dist <= maxAllowedDist) {
                    // Score based on how close the match is
                    double score = 0.6 * (1.0 - static_cast<double>(dist) / qMax(word.length(), normalizedPattern.length()));
                    return score;
                }
            }
        }
    }
    
    // Subsequence matching (all characters appear in order)
    int textIdx = 0;
    int patternIdx = 0;
    int consecutiveBonus = 0;
    double subsequenceScore = 0;
    
    while (textIdx < normalizedText.length() && patternIdx < normalizedPattern.length()) {
        if (normalizedText[textIdx] == normalizedPattern[patternIdx]) {
            // Bonus for consecutive matches
            subsequenceScore += 1.0 + consecutiveBonus * 0.5;
            consecutiveBonus++;
            patternIdx++;
        } else {
            consecutiveBonus = 0;
        }
        textIdx++;
    }
    
    // All pattern characters must be found
    if (patternIdx == normalizedPattern.length()) {
        // Normalize score based on pattern length and add penalty for gaps
        double maxPossibleScore = normalizedPattern.length() * 1.5; // Max with all consecutive
        double normalizedScore = subsequenceScore / maxPossibleScore;
        // Penalty for long gaps (text much longer than pattern)
        double gapPenalty = 1.0 - (static_cast<double>(normalizedText.length() - normalizedPattern.length()) / normalizedText.length() * 0.3);
        return qMax(0.0, qMin(0.5, normalizedScore * gapPenalty * 0.5));
    }
    
    return 0.0; // No match
}

// Legacy wrapper - converts Fuse.js style score (0-1) to old integer format for compatibility
int WeaponSearchModel::fuzzyScore(const QString &text, const QString &query) const
{
    double fuseScore = fuseFuzzyMatch(text, query);
    
    // Threshold: require at least 0.3 (30%) match
    const double threshold = 0.3;
    if (fuseScore < threshold) {
        return 0;
    }
    
    // Convert 0.0-1.0 score to 0-1000 integer score
    return static_cast<int>(fuseScore * 1000);
}

void WeaponSearchModel::openWeapon(int index)
{
    if (index < 0 || index >= m_filteredWeapons.size())
        return;

    QJsonObject weapon = m_filteredWeapons[index].toObject();
    QString hash = weapon["hash"].toVariant().toString();
    
    QString url = QString("https://godroll.tv/%1").arg(hash);
    
    // If PWA mode is disabled, just open in default browser
    if (!m_openInPWA) {
        QDesktopServices::openUrl(QUrl(url));
        return;
    }
    
    // Try to find Chrome
    QString chromePath;
    
    // Check common Chrome locations on Windows
    QStringList chromePaths = {
        QStandardPaths::findExecutable("chrome"),
        "C:/Program Files/Google/Chrome/Application/chrome.exe",
        "C:/Program Files (x86)/Google/Chrome/Application/chrome.exe",
        QDir::homePath() + "/AppData/Local/Google/Chrome/Application/chrome.exe"
    };
    
    for (const QString &path : chromePaths) {
        if (!path.isEmpty() && QFileInfo::exists(path)) {
            chromePath = path;
            break;
        }
    }
    
    if (!chromePath.isEmpty()) {
        // Open in Chrome app mode (PWA-like standalone window)
        QProcess::startDetached(chromePath, {"--app=" + url});
    } else {
        // Fallback to default browser
        QDesktopServices::openUrl(QUrl(url));
    }
}

void WeaponSearchModel::clearSearch()
{
    // Only clear the search query, keep showLatestSeason as-is
    setSearchQuery("");
}

void WeaponSearchModel::setShowLatestSeason(bool show)
{
    if (m_showLatestSeason == show)
        return;

    m_showLatestSeason = show;
    filterWeapons();
    emit showLatestSeasonChanged();
}

void WeaponSearchModel::setAutoShowLatestSeason(bool autoShow)
{
    if (m_autoShowLatestSeason == autoShow)
        return;

    m_autoShowLatestSeason = autoShow;
    
    // Save preference
    QSettings settings("Godroll.tv", "GodrollLauncher");
    settings.setValue("autoShowLatestSeason", autoShow);
    
    emit autoShowLatestSeasonChanged();
}

void WeaponSearchModel::setOpenInPWA(bool open)
{
    if (m_openInPWA == open)
        return;

    m_openInPWA = open;
    
    // Save preference
    QSettings settings("Godroll.tv", "GodrollLauncher");
    settings.setValue("openInPWA", open);
    
    emit openInPWAChanged();
}
