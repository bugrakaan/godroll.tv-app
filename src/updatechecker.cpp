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
#include <QtConcurrent>
#include <QDateTime>
#include <QCryptographicHash>
#include <QHash>
#include <QRegularExpression>
#include <QUuid>

const QString UpdateChecker::GITHUB_API_URL = "https://api.github.com/repos/%1/releases/latest";
const QString UpdateChecker::GITHUB_REPO = "bugrakaan/godroll.tv-app";

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentVersion(QCoreApplication::applicationVersion())
{
    // Load last check time from settings
    QSettings settings("Godroll.tv", "GodrollLauncher");
    m_lastCheckTime = settings.value("lastUpdateCheckTime", 0).toLongLong();
    
    // Check if we just updated from a previous version
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
    
    // Update last check time
    m_lastCheckTime = QDateTime::currentSecsSinceEpoch();
    QSettings settings("Godroll.tv", "GodrollLauncher");
    settings.setValue("lastUpdateCheckTime", m_lastCheckTime);
    
    QString apiUrl = GITHUB_API_URL.arg(GITHUB_REPO);
    QUrl url(apiUrl);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    request.setRawHeader("User-Agent", "GodrollLauncher");
    
    qDebug() << "Checking for updates at:" << apiUrl;
    
    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onNetworkReply(reply);
    });
}

void UpdateChecker::checkForUpdatesIfNeeded()
{
    // Check only if 4+ hours (14400 seconds) have passed since last check
    const qint64 CHECK_INTERVAL = 4 * 60 * 60;  // 4 hours in seconds
    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    qint64 timeSinceLastCheck = currentTime - m_lastCheckTime;
    
    if (timeSinceLastCheck >= CHECK_INTERVAL) {
        qDebug() << "Update check needed, last check was" << (timeSinceLastCheck / 3600) << "hours ago";
        checkForUpdates();
    } else {
        qDebug() << "Skipping update check, only" << (timeSinceLastCheck / 60) << "minutes since last check";
        // If we already know there's an update available, emit the signal
        if (m_updateAvailable) {
            emit updateCheckComplete(true);
        }
    }
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
    m_checksumUrl.clear();
    m_releaseAssetSha256.clear();
    m_downloadSha256.clear();
    QString zipUrl, exeUrl, msiUrl;
    QString zipName, exeName, msiName;
    QString zipDigest, exeDigest, msiDigest;
    QHash<QString, QString> assetUrls;
    QJsonArray assets = release["assets"].toArray();
    for (const QJsonValue &assetVal : assets) {
        QJsonObject asset = assetVal.toObject();
        const QString assetName = asset["name"].toString();
        QString name = assetName.toLower();
        QString url = asset["browser_download_url"].toString();
        assetUrls.insert(name, url);
        QString digest = asset["digest"].toString();
        if (digest.startsWith("sha256:", Qt::CaseInsensitive))
            digest = digest.mid(7).toLower();
        else
            digest.clear();
        // Look for Windows files
        if (name.contains("windows") || name.contains("win")) {
            if (name.endsWith(".zip")) {
                zipUrl = url;
                zipName = assetName;
                zipDigest = digest;
            } else if (name.endsWith(".exe")) {
                exeUrl = url;
                exeName = assetName;
                exeDigest = digest;
            } else if (name.endsWith(".msi")) {
                msiUrl = url;
                msiName = assetName;
                msiDigest = digest;
            }
        } else {
            // Fallback: any zip/exe/msi
            if (name.endsWith(".zip") && zipUrl.isEmpty()) {
                zipUrl = url;
                zipName = assetName;
                zipDigest = digest;
            } else if (name.endsWith(".exe") && exeUrl.isEmpty()) {
                exeUrl = url;
                exeName = assetName;
                exeDigest = digest;
            } else if (name.endsWith(".msi") && msiUrl.isEmpty()) {
                msiUrl = url;
                msiName = assetName;
                msiDigest = digest;
            }
        }
    }
    // Prefer ZIP for auto-update, then exe, then msi
    QString selectedAssetName;
    if (!zipUrl.isEmpty()) {
        m_downloadUrl = zipUrl;
        selectedAssetName = zipName;
        m_releaseAssetSha256 = zipDigest;
    } else if (!exeUrl.isEmpty()) {
        m_downloadUrl = exeUrl;
        selectedAssetName = exeName;
        m_releaseAssetSha256 = exeDigest;
    } else if (!msiUrl.isEmpty()) {
        m_downloadUrl = msiUrl;
        selectedAssetName = msiName;
        m_releaseAssetSha256 = msiDigest;
    }
    if (!selectedAssetName.isEmpty())
        m_checksumUrl = assetUrls.value((selectedAssetName + ".sha256").toLower());
    
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
    if (m_downloading || m_checksumReply || m_downloadUrl.isEmpty()) return;
    
    // If URL is not a direct file download, open browser instead
    if (!m_downloadUrl.endsWith(".exe") && !m_downloadUrl.endsWith(".zip") && !m_downloadUrl.endsWith(".msi")) {
        qDebug() << "Download URL is not a direct file, opening browser:" << m_downloadUrl;
        openDownloadPage();
        return;
    }

    if (m_checksumUrl.isEmpty()) {
        setStatusText("Update verification unavailable");
        emit downloadFailed(
            "This update cannot be verified. Please download it manually from the release page.");
        return;
    }
    
    m_downloading = true;
    m_downloadProgress = 0;
    setStatusText("Verifying update...");
    emit downloadingChanged();
    emit downloadProgressChanged();

    QNetworkRequest checksumRequest{QUrl(m_checksumUrl)};
    checksumRequest.setRawHeader("User-Agent", "GodrollLauncher");
    checksumRequest.setRawHeader("Accept", "text/plain");
    checksumRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                 QNetworkRequest::NoLessSafeRedirectPolicy);
    checksumRequest.setMaximumRedirectsAllowed(10);
    m_checksumReply = m_networkManager->get(checksumRequest);
    connect(m_checksumReply, &QNetworkReply::finished,
            this, &UpdateChecker::onChecksumFinished);
}

void UpdateChecker::onChecksumFinished()
{
    if (!m_checksumReply)
        return;

    QNetworkReply *reply = m_checksumReply;
    m_checksumReply = nullptr;
    const QByteArray checksumData = reply->readAll();
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkErrorText = reply->errorString();
    reply->deleteLater();

    auto failVerification = [this](const QString &message) {
        m_downloading = false;
        emit downloadingChanged();
        setStatusText("Update verification failed");
        emit downloadFailed(message);
    };

    if (networkError != QNetworkReply::NoError) {
        qWarning() << "Checksum download failed:" << networkErrorText;
        failVerification("The update checksum could not be downloaded. Please try again.");
        return;
    }

    const QString checksumText = QString::fromUtf8(checksumData).trimmed();
    const QRegularExpression checksumPattern(
        "^([A-Fa-f0-9]{64})[\\t ]+\\*?([^\\r\\n]+)$");
    const QRegularExpressionMatch match = checksumPattern.match(checksumText);
    const QString expectedFileName = QUrl(m_downloadUrl).fileName();
    if (!match.hasMatch() ||
        QFileInfo(match.captured(2).trimmed()).fileName() != expectedFileName) {
        qWarning() << "Invalid checksum asset for" << expectedFileName;
        failVerification("The update checksum is invalid. Installation was stopped.");
        return;
    }

    const QString checksumSha256 = match.captured(1).toLower();
    if (!m_releaseAssetSha256.isEmpty() &&
        checksumSha256 != m_releaseAssetSha256.toLower()) {
        qWarning() << "Release digest and checksum asset do not match";
        failVerification("The update verification information does not match. Installation was stopped.");
        return;
    }

    m_downloadSha256 = checksumSha256;
    startUpdateDownload();
}

void UpdateChecker::startUpdateDownload()
{
    if (m_downloadSha256.isEmpty()) {
        m_downloading = false;
        emit downloadingChanged();
        setStatusText("Update verification failed");
        emit downloadFailed("The update checksum is missing. Installation was stopped.");
        return;
    }
    
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
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
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

    if (!m_downloadSha256.isEmpty()) {
        QFile downloadedFile(m_downloadedFilePath);
        if (!downloadedFile.open(QIODevice::ReadOnly)) {
            m_downloading = false;
            emit downloadingChanged();
            setStatusText("Download verification failed");
            emit downloadFailed("The downloaded update could not be verified.");
            m_downloadReply->deleteLater();
            m_downloadReply = nullptr;
            delete m_downloadFile;
            m_downloadFile = nullptr;
            return;
        }

        QCryptographicHash hasher(QCryptographicHash::Sha256);
        if (!hasher.addData(&downloadedFile) ||
            QString::fromLatin1(hasher.result().toHex()).toLower() != m_downloadSha256) {
            downloadedFile.close();
            QFile::remove(m_downloadedFilePath);
            m_downloading = false;
            emit downloadingChanged();
            setStatusText("Download verification failed");
            emit downloadFailed("The downloaded update did not pass its integrity check.");
            m_downloadReply->deleteLater();
            m_downloadReply = nullptr;
            delete m_downloadFile;
            m_downloadFile = nullptr;
            return;
        }
    }
    
    qDebug() << "Download complete:" << m_downloadedFilePath << "(" << fileSize << "bytes)";
    
    // Clean up file handle
    if (m_downloadFile) {
        delete m_downloadFile;
        m_downloadFile = nullptr;
    }
    
    emit downloadComplete(m_downloadedFilePath);
    
    // Check if it's a ZIP file
    if (m_downloadedFilePath.endsWith(".zip", Qt::CaseInsensitive)) {
        setStatusText("Installing...");
        m_downloadProgress = 100;
        emit downloadProgressChanged();
        
        // Small delay before starting installation to show "Installing..." message
        QTimer::singleShot(150, this, [this]() {
            setStatusText("Extracting files...");
            // Extract asynchronously to avoid UI freeze
            startAsyncExtraction(m_downloadedFilePath);
        });
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

void UpdateChecker::startAsyncExtraction(const QString &zipPath)
{
    // Store app paths before starting the thread (must be called from main thread)
    QString appDir = QCoreApplication::applicationDirPath();
    QString appExe = QCoreApplication::applicationFilePath();
    qint64 launcherPid = QCoreApplication::applicationPid();
    
    // Create watcher if needed
    if (!m_extractionWatcher) {
        m_extractionWatcher = new QFutureWatcher<bool>(this);
        connect(m_extractionWatcher, &QFutureWatcher<bool>::finished,
                this, &UpdateChecker::onExtractionFinished);
    }
    
    // Run extraction in a separate thread
    QFuture<bool> future = QtConcurrent::run([zipPath, appDir, appExe, launcherPid]() {
        return extractZipAndReplace(zipPath, appDir, appExe, launcherPid);
    });
    
    m_extractionWatcher->setFuture(future);
}

void UpdateChecker::onExtractionFinished()
{
    bool success = m_extractionWatcher->result();
    
    if (success) {
        // The external updater is now the sole owner of replacement and restart.
        // It waits for this exact PID to exit before touching application files.
        QSettings settings("Godroll.tv", "GodrollLauncher");
        settings.setValue("updatedToVersion", m_latestVersion);
        setStatusText("Restarting...");
        QCoreApplication::quit();
    } else {
        m_downloading = false;
        emit downloadingChanged();
        emit downloadFailed("Failed to extract update. Please download manually.");
    }
}

bool UpdateChecker::extractZipAndReplace(const QString &zipPath, const QString &appDir,
                                         const QString &appExe, qint64 launcherPid)
{
    qDebug() << "Extracting ZIP:" << zipPath;

    const QString updateId = QUuid::createUuid().toString(QUuid::Id128);
    const QString tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString tempExtractDir = tempRoot + "/GodrollUpdate-" + updateId;
    
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
    
    extractProcess.start("powershell.exe", QStringList() << "-NoProfile" << "-NonInteractive"
                                                        << "-Command" << psCommand);
    if (!extractProcess.waitForStarted(10000) ||
        !extractProcess.waitForFinished(60000) ||
        extractProcess.exitStatus() != QProcess::NormalExit ||
        extractProcess.exitCode() != 0) {
        qWarning() << "Failed to extract ZIP:" << extractProcess.readAllStandardError();
        return false;
    }
    
    qDebug() << "ZIP extracted to:" << tempExtractDir;
    
    // Locate the deploy root by finding the executable in the extracted tree.
    QString sourceExe;
    QDirIterator exeIterator(tempExtractDir, {QFileInfo(appExe).fileName()},
                             QDir::Files, QDirIterator::Subdirectories);
    if (exeIterator.hasNext())
        sourceExe = exeIterator.next();
    if (sourceExe.isEmpty()) {
        qWarning() << "The update archive does not contain" << QFileInfo(appExe).fileName();
        return false;
    }
    const QString sourceDir = QFileInfo(sourceExe).absolutePath();
    qDebug() << "Source directory for update:" << sourceDir;

    auto psQuote = [](QString value) {
        return value.replace("'", "''");
    };

    const QString updaterPath = tempRoot + "/godroll-update-" + updateId + ".ps1";
    const QString failureLog = tempRoot + "/godroll-update-" + updateId + ".log";
    const QString backupDir = tempRoot + "/GodrollBackup-" + updateId;
    QFile updaterFile(updaterPath);
    if (!updaterFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to create update helper:" << updaterPath;
        return false;
    }

    QTextStream stream(&updaterFile);
    stream << "$ErrorActionPreference = 'Stop'\r\n";
    stream << QString("$launcherPid = %1\r\n").arg(launcherPid);
    stream << QString("$sourceDir = '%1'\r\n").arg(psQuote(QDir::toNativeSeparators(sourceDir)));
    stream << QString("$sourceExe = '%1'\r\n").arg(psQuote(QDir::toNativeSeparators(sourceExe)));
    stream << QString("$appDir = '%1'\r\n").arg(psQuote(QDir::toNativeSeparators(appDir)));
    stream << QString("$appExe = '%1'\r\n").arg(psQuote(QDir::toNativeSeparators(appExe)));
    stream << QString("$stagingDir = '%1'\r\n").arg(psQuote(QDir::toNativeSeparators(tempExtractDir)));
    stream << QString("$zipPath = '%1'\r\n").arg(psQuote(QDir::toNativeSeparators(zipPath)));
    stream << QString("$failureLog = '%1'\r\n").arg(psQuote(QDir::toNativeSeparators(failureLog)));
    stream << QString("$backupDir = '%1'\r\n").arg(psQuote(QDir::toNativeSeparators(backupDir)));
    stream << QString("$updaterPath = '%1'\r\n").arg(psQuote(QDir::toNativeSeparators(updaterPath)));
    stream << "$newFiles = [System.Collections.Generic.List[string]]::new()\r\n";
    stream << "try {\r\n";
    stream << "  $launcher = Get-Process -Id $launcherPid -ErrorAction SilentlyContinue\r\n";
    stream << "  if ($null -ne $launcher) { Wait-Process -Id $launcherPid -Timeout 60 -ErrorAction Stop }\r\n";
    stream << "  New-Item -ItemType Directory -Path $backupDir -Force | Out-Null\r\n";
    stream << "  Get-ChildItem -LiteralPath $sourceDir -Recurse -File | ForEach-Object {\r\n";
    stream << "    $relativePath = $_.FullName.Substring($sourceDir.Length).TrimStart('\\')\r\n";
    stream << "    $targetPath = Join-Path $appDir $relativePath\r\n";
    stream << "    if (Test-Path -LiteralPath $targetPath) {\r\n";
    stream << "      $backupPath = Join-Path $backupDir $relativePath\r\n";
    stream << "      New-Item -ItemType Directory -Path (Split-Path $backupPath) -Force | Out-Null\r\n";
    stream << "      Copy-Item -LiteralPath $targetPath -Destination $backupPath -Force -ErrorAction Stop\r\n";
    stream << "    } else { [void]$newFiles.Add($targetPath) }\r\n";
    stream << "  }\r\n";
    stream << "  Copy-Item -Path (Join-Path $sourceDir '*') -Destination $appDir -Recurse -Force -ErrorAction Stop\r\n";
    stream << "  if (-not (Test-Path -LiteralPath $appExe)) { throw 'Updated executable is missing.' }\r\n";
    stream << "  $sourceHash = (Get-FileHash -LiteralPath $sourceExe -Algorithm SHA256).Hash\r\n";
    stream << "  $installedHash = (Get-FileHash -LiteralPath $appExe -Algorithm SHA256).Hash\r\n";
    stream << "  if ($sourceHash -ne $installedHash) { throw 'Updated executable verification failed.' }\r\n";
    stream << "  Remove-Item -LiteralPath $backupDir -Recurse -Force -ErrorAction SilentlyContinue\r\n";
    stream << "  Remove-Item -LiteralPath $stagingDir -Recurse -Force -ErrorAction SilentlyContinue\r\n";
    stream << "  Remove-Item -LiteralPath $zipPath -Force -ErrorAction SilentlyContinue\r\n";
    stream << "  Start-Process -FilePath $appExe -WorkingDirectory $appDir\r\n";
    stream << "} catch {\r\n";
    stream << "  Set-Content -LiteralPath $failureLog -Value $_.Exception.ToString()\r\n";
    stream << "  foreach ($newFile in $newFiles) { Remove-Item -LiteralPath $newFile -Force -ErrorAction SilentlyContinue }\r\n";
    stream << "  if (Test-Path -LiteralPath $backupDir) { Copy-Item -Path (Join-Path $backupDir '*') -Destination $appDir -Recurse -Force -ErrorAction SilentlyContinue }\r\n";
    stream << "  Remove-Item -LiteralPath $backupDir -Recurse -Force -ErrorAction SilentlyContinue\r\n";
    stream << "  if (Test-Path -LiteralPath $appExe) { Start-Process -FilePath $appExe -WorkingDirectory $appDir -ArgumentList '--update-failed' }\r\n";
    stream << "}\r\n";
    stream << "Start-Sleep -Milliseconds 500\r\n";
    stream << "Remove-Item -LiteralPath $updaterPath -Force -ErrorAction SilentlyContinue\r\n";
    updaterFile.close();

    qDebug() << "Created update helper:" << updaterPath;
    const bool started = QProcess::startDetached(
        "powershell.exe",
        {"-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass",
         "-WindowStyle", "Hidden", "-File", QDir::toNativeSeparators(updaterPath)});
    if (!started)
        qWarning() << "Failed to start update helper";
    return started;
}
