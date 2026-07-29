#include "weaponloader.h"
#include "seasonmapping.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QDebug>

WeaponLoader::WeaponLoader(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentReply(nullptr)
    , m_timeoutTimer(new QTimer(this))
    , m_retryCount(0)
    , m_maxRetries(MAX_RETRIES)
{
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &WeaponLoader::onNetworkReply);
    
    // Setup timeout timer
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &WeaponLoader::onTimeout);
}

void WeaponLoader::loadWeapons(std::function<void(const QJsonArray&)> callback,
                               int maxRetries)
{
    m_callback = callback;
    m_retryCount = 0;
    m_maxRetries = qMax(0, maxRetries);
    
    startRequest();
}

void WeaponLoader::startRequest()
{
    // Cleanup any previous request
    cleanupCurrentRequest();
    
    qDebug() << "Loading weapons from API..." << (m_retryCount > 0 ? QString("(retry %1/%2)").arg(m_retryCount).arg(m_maxRetries) : "");
    emit loadStatusChanged(m_retryCount > 0
        ? QString("Trying to load your weapons again (%1 of %2)...").arg(m_retryCount).arg(m_maxRetries)
        : QString("Connecting to Godroll TV..."));
    
    // Load weapons from your API with source=app to get perkColumns data
    QNetworkRequest request(QUrl("https://godroll.tv/api/weapons/list?source=app"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    m_currentReply = m_networkManager->get(request);
    
    // Start timeout timer
    m_timeoutTimer->start(TIMEOUT_MS);
}

void WeaponLoader::cleanupCurrentRequest()
{
    // Stop timeout timer
    m_timeoutTimer->stop();
    
    // Abort and cleanup current reply if exists
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
}

void WeaponLoader::onTimeout()
{
    qWarning() << "Request timed out after" << (TIMEOUT_MS / 1000) << "seconds";
    
    if (m_retryCount < m_maxRetries) {
        m_retryCount++;
        qDebug() << "Retrying... (" << m_retryCount << "/" << m_maxRetries << ")";
        startRequest();
    } else {
        qWarning() << "Max retries reached. Failed to load weapons.";
        cleanupCurrentRequest();
        emit loadFailed("Could not connect to the weapon service.");
        if (m_callback) {
            m_callback(QJsonArray());
        }
    }
}

void WeaponLoader::onNetworkReply(QNetworkReply *reply)
{
    // Stop timeout timer since we got a response
    m_timeoutTimer->stop();
    
    // Ignore aborted requests (from timeout handling)
    if (reply->error() == QNetworkReply::OperationCanceledError) {
        reply->deleteLater();
        return;
    }
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("weapons") && obj["success"].toBool()) {
                emit loadStatusChanged("Organizing your weapons...");
                QJsonArray weapons = obj["weapons"].toArray();
                
                // Process weapons to add season info from traitIds
                QJsonArray processedWeapons;
                for (const QJsonValue &value : weapons) {
                    QJsonObject weapon = value.toObject();
                    
                    // Extract exotic status from tierType (6 = Exotic) or tierTypeName
                    bool isExotic = (weapon["tierType"].toInt() == 6) || 
                                   (weapon["tierTypeName"].toString().toLower() == "exotic");
                    weapon["isExotic"] = isExotic;
                    
                    // Extract season from traitIds
                    if (weapon.contains("traitIds")) {
                        QJsonArray traitIds = weapon["traitIds"].toArray();
                        int seasonNumber = SeasonMapping::instance().getSeasonNumber(traitIds);
                        QString seasonName = SeasonMapping::instance().getSeasonName(traitIds);
                        QString seasonDisplay = SeasonMapping::instance().getSeasonFromTraitIds(traitIds);
                        weapon["seasonNumber"] = seasonNumber;
                        weapon["seasonName"] = seasonName;
                        // Add a searchable season field that always contains "Season X" format
                        // Even if seasonNumber is 0, we use "Season 0" so "Season" search works
                        weapon["season"] = QString("Season %1").arg(seasonNumber);
                        weapon["seasonDisplay"] = seasonDisplay;
                    } else {
                        // Fallback for weapons without traitIds
                        weapon["seasonNumber"] = 0;
                        weapon["seasonName"] = "";
                        weapon["season"] = "Season";
                        weapon["seasonDisplay"] = "";
                    }
                    
                    processedWeapons.append(weapon);
                }
                
                qDebug() << "Loaded" << processedWeapons.size() << "weapons";
                m_currentReply = nullptr;
                
                // Cache for spam protection
                m_cachedWeapons = processedWeapons;
                
                // Emit signal for QML connections
                emit weaponsLoaded(processedWeapons);
                
                if (m_callback) {
                    m_callback(processedWeapons);
                }
            } else {
                qWarning() << "API response does not contain weapons array";
                // Retry on invalid response
                if (m_retryCount < m_maxRetries) {
                    m_retryCount++;
                    qDebug() << "Invalid response, retrying... (" << m_retryCount << "/" << m_maxRetries << ")";
                    reply->deleteLater();
                    m_currentReply = nullptr;
                    startRequest();
                    return;
                } else {
                    emit loadFailed("The weapon service returned an unexpected response.");
                }
            }
        } else {
            qWarning() << "API response is not a JSON object";
            // Retry on invalid response
            if (m_retryCount < m_maxRetries) {
                m_retryCount++;
                qDebug() << "Invalid JSON, retrying... (" << m_retryCount << "/" << m_maxRetries << ")";
                reply->deleteLater();
                m_currentReply = nullptr;
                startRequest();
                return;
            } else {
                emit loadFailed("The weapon service returned unreadable data.");
            }
        }
    } else {
        qWarning() << "Failed to load weapons:" << reply->errorString();
        // Retry on network error
        if (m_retryCount < m_maxRetries) {
            m_retryCount++;
            qDebug() << "Network error, retrying... (" << m_retryCount << "/" << m_maxRetries << ")";
            reply->deleteLater();
            m_currentReply = nullptr;
            startRequest();
            return;
        } else {
            emit loadFailed("Could not connect to the weapon service.");
            if (m_callback) {
                m_callback(QJsonArray());
            }
        }
    }
    
    m_currentReply = nullptr;
    reply->deleteLater();
}

void WeaponLoader::reload()
{
    // Spam protection: max 3 reloads per 60 seconds
    qint64 now = QDateTime::currentSecsSinceEpoch();
    // Remove timestamps older than the window
    while (!m_reloadTimestamps.isEmpty() && (now - m_reloadTimestamps.first()) > RELOAD_WINDOW_SECS) {
        m_reloadTimestamps.removeFirst();
    }
    if (m_reloadTimestamps.size() >= MAX_RELOADS_PER_WINDOW && !m_cachedWeapons.isEmpty()) {
        qDebug() << "Reload throttled - serving cached data";
        emit reloadStarted();
        // Brief delay so loading animation is visible
        QTimer::singleShot(400, this, [this]() {
            emit weaponsLoaded(m_cachedWeapons);
        });
        return;
    }
    m_reloadTimestamps.append(now);

    qDebug() << "Reloading weapons...";
    emit reloadStarted();
    m_retryCount = 0;
    m_maxRetries = MAX_RETRIES;
    startRequest();
}
