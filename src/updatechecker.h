#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QUrl>
#include <QFile>

class UpdateChecker : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY updateInfoChanged)
    Q_PROPERTY(QString releaseNotes READ releaseNotes NOTIFY updateInfoChanged)
    Q_PROPERTY(QString downloadUrl READ downloadUrl NOTIFY updateInfoChanged)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateInfoChanged)
    Q_PROPERTY(bool checking READ checking NOTIFY checkingChanged)
    Q_PROPERTY(bool downloading READ downloading NOTIFY downloadingChanged)
    Q_PROPERTY(int downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString updatedToVersion READ updatedToVersion NOTIFY updatedToVersionChanged)

public:
    explicit UpdateChecker(QObject *parent = nullptr);
    
    QString currentVersion() const { return m_currentVersion; }
    QString latestVersion() const { return m_latestVersion; }
    QString releaseNotes() const { return m_releaseNotes; }
    QString downloadUrl() const { return m_downloadUrl; }
    bool updateAvailable() const { return m_updateAvailable; }
    bool checking() const { return m_checking; }
    bool downloading() const { return m_downloading; }
    int downloadProgress() const { return m_downloadProgress; }
    QString statusText() const { return m_statusText; }
    QString updatedToVersion() const { return m_updatedToVersion; }
    
    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void openDownloadPage();
    Q_INVOKABLE void skipVersion(const QString &version);
    Q_INVOKABLE bool isVersionSkipped(const QString &version);
    Q_INVOKABLE void downloadAndInstall();
    Q_INVOKABLE void clearUpdateNotification();

signals:
    void updateInfoChanged();
    void checkingChanged();
    void downloadingChanged();
    void downloadProgressChanged();
    void statusTextChanged();
    void updatedToVersionChanged();
    void updateCheckComplete(bool updateAvailable);
    void updateCheckFailed(const QString &error);
    void downloadComplete(const QString &filePath);
    void downloadFailed(const QString &error);
    void justUpdated(const QString &version);

private slots:
    void onNetworkReply(QNetworkReply *reply);
    void onDownloadReadyRead();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();

private:
    bool isNewerVersion(const QString &latest, const QString &current) const;
    void launchInstallerAndQuit(const QString &installerPath);
    bool extractZipAndReplace(const QString &zipPath);
    void setStatusText(const QString &text);
    
    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_downloadReply = nullptr;
    QFile *m_downloadFile = nullptr;
    QString m_currentVersion;
    QString m_latestVersion;
    QString m_releaseNotes;
    QString m_downloadUrl;
    QString m_htmlUrl;  // GitHub release page URL
    QString m_downloadedFilePath;
    QString m_updatedToVersion;
    QString m_statusText;
    bool m_updateAvailable = false;
    bool m_checking = false;
    bool m_downloading = false;
    int m_downloadProgress = 0;
    
    static const QString GITHUB_API_URL;
    static const QString GITHUB_REPO;
};

#endif // UPDATECHECKER_H
