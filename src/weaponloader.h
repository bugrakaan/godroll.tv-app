#ifndef WEAPONLOADER_H
#define WEAPONLOADER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonArray>
#include <QTimer>
#include <QList>
#include <functional>

class WeaponLoader : public QObject
{
    Q_OBJECT

public:
    explicit WeaponLoader(QObject *parent = nullptr);

    void loadWeapons(std::function<void(const QJsonArray&)> callback,
                     int maxRetries = 3);
    
    // QML-callable reload method
    Q_INVOKABLE void reload();

signals:
    void weaponsLoaded(const QJsonArray &weapons);
    void loadFailed(const QString &message);
    void loadStatusChanged(const QString &message);
    void reloadStarted();

private slots:
    void onNetworkReply(QNetworkReply *reply);
    void onTimeout();

private:
    void startRequest();
    void cleanupCurrentRequest();

    QNetworkAccessManager *m_networkManager;
    std::function<void(const QJsonArray&)> m_callback;
    QNetworkReply *m_currentReply;
    QTimer *m_timeoutTimer;
    int m_retryCount;
    int m_maxRetries;
    static const int MAX_RETRIES = 3;
    static const int TIMEOUT_MS = 15000; // 15 seconds

    // F5 spam protection: max 3 reloads per 60 seconds
    QJsonArray m_cachedWeapons;
    QList<qint64> m_reloadTimestamps;
    static const int RELOAD_WINDOW_SECS = 60;
    static const int MAX_RELOADS_PER_WINDOW = 3;
};

#endif // WEAPONLOADER_H
