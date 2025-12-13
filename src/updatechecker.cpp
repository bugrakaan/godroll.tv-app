#include "updatechecker.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDesktopServices>
#include <QSettings>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QProcess>
#include <QTimer>
#include <QFileInfo>
#include <QDirIterator>
#include <QTextStream>

const QString UpdateChecker::GITHUB_API_URL = "https://api.github.com/repos/%1/releases/latest";
const QString UpdateChecker::GITHUB_REPO = "bugrakaan/godroll.tv-app";

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentVersion(QCoreApplication::applicationVersion())
{
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &UpdateChecker::onNetworkReply);
    
    // Check if we just updated from a previous version
    QSettings settings("Godroll.tv", "GodrollLauncher");
    m_updatedToVersion = settings.value("updatedToVersion", "").toString();
    if (!m_updatedToVersion.isEmpty() && m_updatedToVersion == m_currentVersion) {
        // We just updated! Emit signal after a short delay (let QML load first)
        QTimer::singleShot(500, this, [this]() {
            emit justUpdated(m_updatedToVersion);
        });
        // Clear the notification
        settings.remove("updatedToVersion");
    } else {
        // Clear any stale version info
        settings.remove("updatedToVersion");
        m_updatedToVersion.clear();
    }
}

void UpdateChecker::checkForUpdates()
{
    if (m_checking) return;
    
    m_checking = true;
    emit checkingChanged();
    
    QString apiUrl = GITHUB_API_URL.arg(GITHUB_REPO);
    QUrl url(apiUrl);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    request.setRawHeader("User-Agent", "GodrollLauncher");
    
    qDebug() << "Checking for updates at:" << apiUrl;
    
    m_networkManager->get(request);
}

void UpdateChecker::onNetworkReply(QNetworkReply *reply)
{
    m_checking = false;
    emit checkingChanged();
    
    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Update check failed:" << reply->errorString();
        emit updateCheckFailed(reply->errorString());
        reply->deleteLater();
        return;
    }
    
    QByteArray responseData = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    
    if (!doc.isObject()) {
        qWarning() << "Invalid JSON response from GitHub API";
        emit updateCheckFailed("Invalid response from server");
        reply->deleteLater();
        return;
    }
    
    QJsonObject release = doc.object();
    
    // Get version (tag_name, usually "v1.2.3" format)
    QString tagName = release["tag_name"].toString();
    m_latestVersion = tagName.startsWith("v") ? tagName.mid(1) : tagName;
    
    // Get release notes (body)
    m_releaseNotes = release["body"].toString();
    
    // Get HTML URL (release page)
    m_htmlUrl = release["html_url"].toString();
    
    // Find Windows asset download URL (prefer .zip for auto-update)
    m_downloadUrl = m_htmlUrl; // Default to release page
    QString zipUrl, exeUrl, msiUrl;
    QJsonArray assets = release["assets"].toArray();
    for (const QJsonValue &assetVal : assets) {
        QJsonObject asset = assetVal.toObject();
        QString name = asset["name"].toString().toLower();
        QString url = asset["browser_download_url"].toString();
        // Look for Windows files
        if (name.contains("windows") || name.contains("win")) {
            if (name.endsWith(".zip")) {
                zipUrl = url;
            } else if (name.endsWith(".exe")) {
                exeUrl = url;
            } else if (name.endsWith(".msi")) {
                msiUrl = url;
            }
        } else {
            // Fallback: any zip/exe/msi
            if (name.endsWith(".zip") && zipUrl.isEmpty()) {
                zipUrl = url;
            } else if (name.endsWith(".exe") && exeUrl.isEmpty()) {
                exeUrl = url;
            } else if (name.endsWith(".msi") && msiUrl.isEmpty()) {
                msiUrl = url;
            }
        }
    }
    // Prefer ZIP for auto-update, then exe, then msi
    if (!zipUrl.isEmpty()) {
        m_downloadUrl = zipUrl;
    } else if (!exeUrl.isEmpty()) {
        m_downloadUrl = exeUrl;
    } else if (!msiUrl.isEmpty()) {
        m_downloadUrl = msiUrl;
    }
    
    qDebug() << "Download URL:" << m_downloadUrl;
    
    // Check if update is available
    m_updateAvailable = isNewerVersion(m_latestVersion, m_currentVersion);
    
    // Check if this version was skipped
    if (m_updateAvailable && isVersionSkipped(m_latestVersion)) {
        qDebug() << "Version" << m_latestVersion << "was skipped by user";
        m_updateAvailable = false;
    }
    
    qDebug() << "Current version:" << m_currentVersion;
    qDebug() << "Latest version:" << m_latestVersion;
    qDebug() << "Update available:" << m_updateAvailable;
    
    emit updateInfoChanged();
    emit updateCheckComplete(m_updateAvailable);
    
    reply->deleteLater();
}

bool UpdateChecker::isNewerVersion(const QString &latest, const QString &current) const
{
    // Parse version strings (e.g., "1.2.3" -> [1, 2, 3])
    QStringList latestParts = latest.split('.');
    QStringList currentParts = current.split('.');
    
    // Pad with zeros if needed
    while (latestParts.size() < 3) latestParts.append("0");
    while (currentParts.size() < 3) currentParts.append("0");
    
    // Compare each part
    for (int i = 0; i < 3; ++i) {
        int latestNum = latestParts[i].toInt();
        int currentNum = currentParts[i].toInt();
        
        if (latestNum > currentNum) return true;
        if (latestNum < currentNum) return false;
    }
    
    return false; // Versions are equal
}

void UpdateChecker::openDownloadPage()
{
    if (!m_downloadUrl.isEmpty()) {
        QDesktopServices::openUrl(QUrl(m_downloadUrl));
    } else if (!m_htmlUrl.isEmpty()) {
        QDesktopServices::openUrl(QUrl(m_htmlUrl));
    }
}

void UpdateChecker::skipVersion(const QString &version)
{
    QSettings settings("Godroll.tv", "GodrollLauncher");
    settings.setValue("skippedVersion", version);
    qDebug() << "Skipped version:" << version;
}

bool UpdateChecker::isVersionSkipped(const QString &version)
{
    QSettings settings("Godroll.tv", "GodrollLauncher");
    QString skipped = settings.value("skippedVersion", "").toString();
    return skipped == version;
}

void UpdateChecker::downloadAndInstall()
{
    if (m_downloading || m_downloadUrl.isEmpty()) return;
    
    // If URL is not a direct file download, open browser instead
    if (!m_downloadUrl.endsWith(".exe") && !m_downloadUrl.endsWith(".zip") && !m_downloadUrl.endsWith(".msi")) {
        qDebug() << "Download URL is not a direct file, opening browser:" << m_downloadUrl;
        openDownloadPage();
        return;
    }
    
    m_downloading = true;
    m_downloadProgress = 0;
    emit downloadingChanged();
    emit downloadProgressChanged();
    
    // Prepare download location
    QString downloadDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir dir(downloadDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // Extract filename from URL
    QUrl url(m_downloadUrl);
    QString fileName = url.fileName();
    if (fileName.isEmpty()) {
        fileName = "GodrollLauncher_update.exe";
    }
    
    m_downloadedFilePath = downloadDir + "/" + fileName;
    
    qDebug() << "Downloading update from:" << m_downloadUrl;
    qDebug() << "Saving to:" << m_downloadedFilePath;
    
    // Delete existing file if present
    QFile::remove(m_downloadedFilePath);
    
    // Start download
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "GodrollLauncher");
    request.setRawHeader("Accept", "application/octet-stream");
    // GitHub uses redirects for asset downloads - must follow them
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setMaximumRedirectsAllowed(10);
    
    m_downloadReply = m_networkManager->get(request);
    
    // Open file for writing chunks as they arrive
    m_downloadFile = new QFile(m_downloadedFilePath);
    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open file for writing:" << m_downloadedFilePath;
        m_downloading = false;
        emit downloadingChanged();
        emit downloadFailed("Failed to create download file");
        delete m_downloadFile;
        m_downloadFile = nullptr;
        return;
    }
    
    connect(m_downloadReply, &QNetworkReply::readyRead,
            this, &UpdateChecker::onDownloadReadyRead);
    connect(m_downloadReply, &QNetworkReply::downloadProgress, 
            this, &UpdateChecker::onDownloadProgress);
    connect(m_downloadReply, &QNetworkReply::finished,
            this, &UpdateChecker::onDownloadFinished);
}

void UpdateChecker::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        int progress = static_cast<int>((bytesReceived * 100) / bytesTotal);
        if (progress != m_downloadProgress) {
            m_downloadProgress = progress;
            setStatusText(QString("Downloading %1%").arg(progress));
            emit downloadProgressChanged();
        }
    }
}

void UpdateChecker::onDownloadReadyRead()
{
    if (m_downloadReply && m_downloadFile) {
        m_downloadFile->write(m_downloadReply->readAll());
    }
}

void UpdateChecker::onDownloadFinished()
{
    if (!m_downloadReply) return;
    
    qDebug() << "Download finished, error:" << m_downloadReply->error() 
             << "HTTP status:" << m_downloadReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    
    // Write any remaining data
    if (m_downloadFile) {
        m_downloadFile->write(m_downloadReply->readAll());
        m_downloadFile->close();
        qDebug() << "File size:" << QFileInfo(m_downloadedFilePath).size() << "bytes";
    }
    
    if (m_downloadReply->error() != QNetworkReply::NoError) {
        m_downloading = false;
        emit downloadingChanged();
        qWarning() << "Download failed:" << m_downloadReply->errorString();
        setStatusText("Download failed: " + m_downloadReply->errorString());
        emit downloadFailed(m_downloadReply->errorString());
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
        if (m_downloadFile) {
            delete m_downloadFile;
            m_downloadFile = nullptr;
        }
        return;
    }
    
    // Check file size
    qint64 fileSize = QFileInfo(m_downloadedFilePath).size();
    if (fileSize == 0) {
        m_downloading = false;
        emit downloadingChanged();
        qWarning() << "Downloaded file is empty";
        setStatusText("Download failed: Empty file");
        emit downloadFailed("Downloaded file is empty");
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
        if (m_downloadFile) {
            delete m_downloadFile;
            m_downloadFile = nullptr;
        }
        return;
    }
    
    qDebug() << "Download complete:" << m_downloadedFilePath << "(" << fileSize << "bytes)";
    
    // Clean up file handle
    if (m_downloadFile) {
        delete m_downloadFile;
        m_downloadFile = nullptr;
    }
    
    // Save the new version to settings so we can show it after restart
    QSettings settings("Godroll.tv", "GodrollLauncher");
    settings.setValue("updatedToVersion", m_latestVersion);
    
    emit downloadComplete(m_downloadedFilePath);
    
    // Check if it's a ZIP file
    if (m_downloadedFilePath.endsWith(".zip", Qt::CaseInsensitive)) {
        setStatusText("Extracting...");
        // Extract and replace files
        if (extractZipAndReplace(m_downloadedFilePath)) {
            setStatusText("Restarting...");
            // Restart the application
            QString appPath = QCoreApplication::applicationFilePath();
            qDebug() << "Restarting application:" << appPath;
            QProcess::startDetached(appPath, QStringList());
            QCoreApplication::quit();
        } else {
            m_downloading = false;
            emit downloadingChanged();
            emit downloadFailed("Failed to extract update. Please download manually.");
        }
    } else {
        m_downloading = false;
        emit downloadingChanged();
        // Launch the installer and quit
        launchInstallerAndQuit(m_downloadedFilePath);
    }
    
    m_downloadReply->deleteLater();
    m_downloadReply = nullptr;
}

void UpdateChecker::launchInstallerAndQuit(const QString &installerPath)
{
    qDebug() << "Launching installer:" << installerPath;
    
    // Start the installer as a detached process
    bool started = QProcess::startDetached(installerPath, QStringList());
    
    if (started) {
        qDebug() << "Installer started, quitting application...";
        // Quit the application to allow the installer to run
        QCoreApplication::quit();
    } else {
        qWarning() << "Failed to start installer";
        // Open the folder containing the downloaded file
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(installerPath).absolutePath()));
        emit downloadFailed("Failed to start installer. File saved to: " + installerPath);
    }
}

void UpdateChecker::clearUpdateNotification()
{
    QSettings settings("Godroll.tv", "GodrollLauncher");
    settings.remove("updatedToVersion");
    m_updatedToVersion.clear();
    emit updatedToVersionChanged();
}

void UpdateChecker::setStatusText(const QString &text)
{
    if (m_statusText != text) {
        m_statusText = text;
        emit statusTextChanged();
    }
}

bool UpdateChecker::extractZipAndReplace(const QString &zipPath)
{
    qDebug() << "Extracting ZIP:" << zipPath;
    
    // Get the application directory
    QString appDir = QCoreApplication::applicationDirPath();
    QString appExe = QCoreApplication::applicationFilePath();
    QString tempExtractDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/GodrollUpdate";
    
    // Clean up any previous temp extraction
    QDir tempDir(tempExtractDir);
    if (tempDir.exists()) {
        tempDir.removeRecursively();
    }
    tempDir.mkpath(".");
    
    // Use PowerShell to extract ZIP (Windows built-in)
    QProcess extractProcess;
    QString zipPathEscaped = QString(zipPath).replace("'", "''");
    QString tempExtractDirEscaped = QString(tempExtractDir).replace("'", "''");
    QString psCommand = QString(
        "Expand-Archive -Path '%1' -DestinationPath '%2' -Force"
    ).arg(zipPathEscaped, tempExtractDirEscaped);
    
    qDebug() << "Running PowerShell:" << psCommand;
    
    extractProcess.start("powershell", QStringList() << "-NoProfile" << "-Command" << psCommand);
    extractProcess.waitForFinished(60000); // 60 second timeout
    
    if (extractProcess.exitCode() != 0) {
        qWarning() << "Failed to extract ZIP:" << extractProcess.readAllStandardError();
        return false;
    }
    
    qDebug() << "ZIP extracted to:" << tempExtractDir;
    
    // Find the extracted content (might be in a subfolder)
    QDir extractedDir(tempExtractDir);
    QStringList entries = extractedDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    QString sourceDir = tempExtractDir;
    if (entries.size() == 1) {
        // Content is in a subfolder (common with GitHub releases)
        sourceDir = tempExtractDir + "/" + entries.first();
    }
    
    qDebug() << "Source directory for update:" << sourceDir;
    
    // Create a batch script to replace files after app exits
    QString batchPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/godroll_update.bat";
    QFile batchFile(batchPath);
    
    if (batchFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&batchFile);
        stream << "@echo off\r\n";
        stream << "echo Updating Godroll Launcher...\r\n";
        stream << "timeout /t 2 /nobreak >nul\r\n"; // Wait for app to close
        
        // Copy all files from extracted directory to app directory
        QString sourceDirWin = QString(sourceDir).replace("/", "\\");
        QString appDirWin = QString(appDir).replace("/", "\\");
        stream << QString("xcopy \"%1\\*\" \"%2\\\" /E /Y /I /Q\r\n").arg(sourceDirWin, appDirWin);
        
        // Clean up
        QString tempExtractDirWin = QString(tempExtractDir).replace("/", "\\");
        QString zipPathWin = QString(zipPath).replace("/", "\\");
        stream << QString("rmdir /S /Q \"%1\"\r\n").arg(tempExtractDirWin);
        stream << QString("del \"%1\"\r\n").arg(zipPathWin);
        
        // Restart the application
        QString appExeWin = QString(appExe).replace("/", "\\");
        stream << QString("start \"\" \"%1\"\r\n").arg(appExeWin);
        
        // Delete this batch file
        QString batchPathWin = QString(batchPath).replace("/", "\\");
        stream << QString("del \"%1\"\r\n").arg(batchPathWin);
        
        batchFile.close();
        
        qDebug() << "Created update batch script:" << batchPath;
        
        // Run the batch script
        bool started = QProcess::startDetached("cmd", QStringList() << "/c" << batchPath);
        
        if (!started) {
            qWarning() << "Failed to start update script";
            return false;
        }
        
        return true;
    }
    
    qWarning() << "Failed to create update batch script";
    return false;
}
