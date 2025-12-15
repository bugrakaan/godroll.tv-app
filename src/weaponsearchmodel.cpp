#include "weaponsearchmodel.h"
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
    
    // Apply user preference for auto-showing latest season
    m_showLatestSeason = m_autoShowLatestSeason;
    
    // Filter weapons based on current state
    filterWeapons();
    
    // Notify that weapons are loaded
    emit weaponsLoaded();
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
    
    QString queryLower = m_searchQuery.toLower().trimmed();
    
    // Check for flags in various formats:
    // - Combined: -!*ha, -h!*, etc.
    // - Separate: -h -* -!, -h-*-!, etc.
    // Pattern matches any -X where X contains !, *, h, a, or e
    QRegularExpression flagPattern("-([!*hae]+)");
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

    // If query is empty (after removing flags), show latest season weapons with filters applied
    // The -* flag allows showing ALL weapons (not just latest season)
    // Other flags (-!, -h, -a) are filters that apply to the current view (latest season by default)
    bool showAllWeapons = noLimit; // Only -* flag shows all weapons
    
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
                    
                    // If uniqueByName is enabled, skip if we've seen this base name
                    // Prefer non-holofoil, non-adept versions
                    if (uniqueByName) {
                        // Use base name (without Adept/Harrowed/Timelost suffix) for comparison
                        std::string nameKey = getBaseWeaponName(name).toStdString();
                        if (seenWeaponNames.count(nameKey) > 0) {
                            continue;
                        }
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
            
            // Apply uniqueByName filter - skip if we've already seen this base weapon name
            // Prefer non-holofoil, non-adept versions (they typically come first)
            if (uniqueByName) {
                if (seenWeaponNames.find(baseName) != seenWeaponNames.end()) {
                    continue; // Skip duplicate weapon base names
                }
                seenWeaponNames.insert(baseName);
            }
            
            QString name = weaponName.toLower();
            QString weaponType = weapon["weaponType"].toString().toLower();
            QString frameType = weapon["frameType"].toString().toLower();
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
            
            // If only flags were provided (no search terms), show all weapons
            if (searchTerms.isEmpty()) {
                weapon["matchedField"] = "";
                scoredWeapons.append({500, seasonNum, name, weapon});
                continue;
            }
            
            // For each term, check if it matches any field
            bool allTermsMatch = true;
            int totalScore = 0;
            QStringList matchedFields;
            
            for (const QString &term : searchTerms) {
                int termScore = 0;
                QString termMatchedField = "";
                
                // Priority order (highest to lowest):
                // 1. Name (weapon name) - 1.0x + 1000 bonus (highest priority)
                // 2. Weapon type - 0.9x
                // 3. Frame type - 0.8x
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
                
                // Check frame type (0.8x multiplier)
                int frameTypeScore = fuzzyScore(frameType, term);
                if (frameTypeScore > 0 && static_cast<int>(frameTypeScore * 0.8) > termScore) {
                    termScore = static_cast<int>(frameTypeScore * 0.8);
                    termMatchedField = "frameType";
                }
                
                // Check weapon type (0.9x multiplier)
                int weaponTypeScore = fuzzyScore(weaponType, term);
                if (weaponTypeScore > 0 && static_cast<int>(weaponTypeScore * 0.9) > termScore) {
                    termScore = static_cast<int>(weaponTypeScore * 0.9);
                    termMatchedField = "weaponType";
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
                if (!termMatchedField.isEmpty() && termMatchedField != "name" && !matchedFields.contains(termMatchedField)) {
                    matchedFields.append(termMatchedField);
                }
            }
            
            if (allTermsMatch && totalScore > 0) {
                // Store matched fields as comma-separated string
                weapon["matchedField"] = matchedFields.join(",");
                
                // Add season bonus: newer seasons get higher score
                // This ensures that among similar name matches, newer season weapons rank higher
                // Season bonus: seasonNum * 10 (e.g., S28 = +280, S24 = +240, difference = 40 points)
                int seasonBonus = seasonNum * 10;
                int finalScore = totalScore + seasonBonus;
                
                scoredWeapons.append({finalScore, seasonNum, name, weapon});
            }
        }

        // Sort by: score (descending), then season (descending), then alphabetically
        // Since season bonus is already included in score, this naturally prioritizes newer seasons
        std::sort(scoredWeapons.begin(), scoredWeapons.end(),
                  [](const auto &a, const auto &b) {
                      int scoreA = std::get<0>(a);
                      int scoreB = std::get<0>(b);
                      
                      // Primary: sort by score (higher first)
                      if (scoreA != scoreB) {
                          return scoreA > scoreB;
                      }
                      
                      // Secondary: sort by season number (higher/newer first)
                      int seasonA = std::get<1>(a);
                      int seasonB = std::get<1>(b);
                      if (seasonA != seasonB) {
                          return seasonA > seasonB;
                      }
                      
                      // Tertiary: sort alphabetically by name
                      return std::get<2>(a).toLower() < std::get<2>(b).toLower();
                  });

        // Determine result limit:
        // - noLimit flag (-*): no limit
        // - isSeasonSearch (s27, Season 27): no limit  
        // - holofoilOnly, uniqueByName, adeptOnly, or exoticOnly with no other search: no limit
        // - Otherwise: limit to 50
        m_filteredWeapons = QJsonArray();
        bool shouldRemoveLimit = noLimit || isSeasonSearch || ((holofoilOnly || uniqueByName || adeptOnly || exoticOnly) && searchTerms.isEmpty());
        int maxResults = shouldRemoveLimit ? scoredWeapons.size() : qMin(50, static_cast<int>(scoredWeapons.size()));
        for (int i = 0; i < maxResults; ++i) {
            m_filteredWeapons.append(std::get<3>(scoredWeapons[i]));
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
    
    // Contains match - high score based on position
    int containsIndex = normalizedText.indexOf(normalizedPattern);
    if (containsIndex != -1) {
        // Earlier position = higher score
        double positionBonus = 1.0 - (static_cast<double>(containsIndex) / normalizedText.length() * 0.1);
        // Longer pattern relative to text = higher score
        double lengthRatio = static_cast<double>(normalizedPattern.length()) / normalizedText.length();
        return 0.85 + (positionBonus * 0.1) + (lengthRatio * 0.05);
    }
    
    // Word starts with pattern (e.g., "pul" matches "Pulse Rifle")
    QStringList words = normalizedText.split(' ', Qt::SkipEmptyParts);
    for (const QString &word : words) {
        if (word.startsWith(normalizedPattern)) {
            return 0.8;
        }
    }
    
    // Prefix match on any word
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
