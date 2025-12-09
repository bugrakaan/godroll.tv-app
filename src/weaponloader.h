#ifndef WEAPONLOADER_H
#define WEAPONLOADER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonArray>
#include <QTimer>
#include <functional>

class WeaponLoader : public QObject
{
    Q_OBJECT

public:
    explicit WeaponLoader(QObject *parent = nullptr);

    void loadWeapons(std::function<void(const QJsonArray&)> callback);
    
    // QML-callable reload method
    Q_INVOKABLE void reload();

signals:
    void weaponsLoaded(const QJsonArray &weapons);
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
    static const int MAX_RETRIES = 3;
    static const int TIMEOUT_MS = 15000; // 15 seconds
};

#endif // WEAPONLOADER_H
