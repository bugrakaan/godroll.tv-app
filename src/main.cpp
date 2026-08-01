#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QFontDatabase>
#include <QSharedMemory>
#include <QMessageBox>
#include <QTimer>
#include <QEventLoop>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QSplashScreen>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QPushButton>
#include <QQuickWindow>
#include <QRandomGenerator>
#include <QWindow>
#include <QUuid>
#include <functional>
#include <memory>
#include "weaponsearchmodel.h"
#include "globalhotkey.h"
#include "weaponloader.h"
#include "trayicon.h"
#include "updatechecker.h"
#include "manifestchecker.h"

// Version from CMake
#ifndef APP_VERSION
#define APP_VERSION "1.0.0"
#endif

class BootSplashScreen final : public QSplashScreen
{
public:
    BootSplashScreen(const QPixmap &pixmap, Qt::WindowFlags flags)
        : QSplashScreen(pixmap, flags)
        , m_retryButton(new QPushButton("Retry", this))
        , m_exitButton(new QPushButton("Exit", this))
    {
        constexpr int buttonWidth = 112;
        constexpr int buttonGap = 12;
        const int buttonGroupX = (pixmap.width() - (buttonWidth * 2 + buttonGap)) / 2;
        m_exitButton->setGeometry(buttonGroupX, 136, buttonWidth, 32);
        m_retryButton->setGeometry(buttonGroupX + buttonWidth + buttonGap,
                                   136, buttonWidth, 32);
        m_retryButton->setCursor(Qt::PointingHandCursor);
        m_retryButton->setStyleSheet(R"(
            QPushButton {
                color: #1a1a1a;
                background-color: #09d7d0;
                border: 1px solid #09d7d0;
                border-radius: 8px;
                font-family: "Space Grotesk";
                font-size: 12px;
                font-weight: 700;
            }
            QPushButton:hover { background-color: #0bc5bf; }
            QPushButton:pressed { background-color: #08aaa5; }
        )");
        m_exitButton->setCursor(Qt::PointingHandCursor);
        m_exitButton->setStyleSheet(R"(
            QPushButton {
                color: #ffffff;
                background-color: #252525;
                border: 1px solid #555555;
                border-radius: 8px;
                font-family: "Space Grotesk";
                font-size: 12px;
                font-weight: 600;
            }
            QPushButton:hover { background-color: #333333; }
            QPushButton:pressed { background-color: #202020; }
        )");
        m_retryButton->hide();
        m_exitButton->hide();
        connect(m_retryButton, &QPushButton::clicked, this, [this]() {
            if (m_retryHandler)
                m_retryHandler();
        });
        connect(m_exitButton, &QPushButton::clicked,
                QCoreApplication::instance(), &QCoreApplication::quit);
    }

    void setStage(const QString &stage)
    {
        m_stage = stage;
        update();
    }

    void setRetryHandler(std::function<void()> handler)
    {
        m_retryHandler = std::move(handler);
    }

    void showRetryButton(bool visible, const QString &message = {})
    {
        m_retryVisible = visible;
        if (!message.isEmpty())
            m_retryMessage = message;
        m_retryButton->setVisible(visible);
        m_exitButton->setVisible(visible);
        if (visible) {
            show();
            raise();
        }
        update();
    }

    void allowClose()
    {
        m_canClose = true;
    }

protected:
    void drawContents(QPainter *painter) override
    {
        painter->setRenderHint(QPainter::TextAntialiasing);
        painter->setPen(QColor("#a8a8a8"));
        painter->setFont(QFont("Space Grotesk", 10));
        painter->drawText(QRect(24, m_retryVisible ? 96 : 126,
                                width() - 48, m_retryVisible ? 28 : 34),
                          Qt::AlignHCenter | Qt::AlignVCenter |
                              Qt::TextWordWrap,
                          m_retryVisible ? m_retryMessage : m_stage);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        // QSplashScreen normally hides itself when clicked. During boot that
        // makes a healthy process look as though it disappeared.
        event->accept();
    }

    void closeEvent(QCloseEvent *event) override
    {
        if (!m_canClose) {
            // A user-initiated close (notably Alt+F4) must still terminate the
            // application even while boot is deliberately held on the splash.
            event->accept();
            QCoreApplication::quit();
            return;
        }
        QSplashScreen::closeEvent(event);
    }

private:
    bool m_canClose = false;
    bool m_retryVisible = false;
    QString m_stage;
    QString m_retryMessage;
    QPushButton *m_retryButton;
    QPushButton *m_exitButton;
    std::function<void()> m_retryHandler;
};

int main(int argc, char *argv[])
{
    qDebug() << "Starting Godroll Launcher v" << APP_VERSION;
    
    QApplication app(argc, argv);
    app.setApplicationName("Godroll.tv Launcher");
    app.setApplicationDisplayName("Godroll.tv Launcher");
    app.setApplicationVersion(APP_VERSION);
    app.setOrganizationName("Godroll.tv");
    app.setQuitOnLastWindowClosed(false); // Keep running in background

    // Single instance check using shared memory
    QSharedMemory sharedMemory("GodrollLauncherSingleInstance");
    if (!sharedMemory.create(1)) {
        // Another instance is already running
        qDebug() << "Another instance is already running. Exiting.";
        return 0;
    }

    // Check if started with --hidden flag (for auto-start)
    bool startHidden = false;
    bool holdSplash = false;
    bool failBoot = false;
    bool updateInstallFailed = false;
    int bootAttempt = 1;
    QString bootHandoffServer;
    for (int i = 1; i < argc; ++i) {
        const QString argument = QString::fromLocal8Bit(argv[i]);
        if (argument == "--hidden" || argument == "-h") {
            startHidden = true;
        } else if (argument == "--hold-splash") {
            holdSplash = true;
        } else if (argument == "--fail-boot") {
            failBoot = true;
        } else if (argument == "--update-failed") {
            updateInstallFailed = true;
        } else if (argument.startsWith("--boot-attempt=")) {
            bool ok = false;
            const int parsedAttempt = argument.mid(15).toInt(&ok);
            if (ok)
                bootAttempt = qBound(1, parsedAttempt, 3);
        } else if (argument.startsWith("--boot-handoff=")) {
            bootHandoffServer = argument.mid(15);
        }
    }
    qDebug() << "Start hidden:" << startHidden;

    const QStringList simulatedBootStages = {
        "Getting things ready...",
        "Preparing your weapon library...",
        "Setting up your keyboard shortcut...",
        "Adding Godroll TV to the system tray...",
        "Checking app services...",
        "Preparing the launcher...",
        "Loading your weapons...",
        "Connecting to Godroll TV...",
        "Organizing your weapons..."
    };
    const QString simulatedFailureStage = failBoot
        ? simulatedBootStages.at(QRandomGenerator::global()->bounded(
              simulatedBootStages.size()))
        : QString();
    const int simulatedFailureStageIndex = failBoot
        ? simulatedBootStages.indexOf(simulatedFailureStage)
        : -1;
    if (failBoot)
        qWarning() << "Simulated boot failure at:" << simulatedFailureStage;

    // Load the same bundled font before painting the native splash so its logo
    // row uses the exact typography as SearchWindow.qml.
    int fontId = QFontDatabase::addApplicationFont(
        ":/qt/qml/GodrollLauncher/resources/fonts/SpaceGrotesk.ttf");
    if (fontId != -1) {
        qDebug() << "Loaded font families:"
                 << QFontDatabase::applicationFontFamilies(fontId);
    } else {
        qDebug() << "Failed to load Space Grotesk font";
    }

    std::unique_ptr<BootSplashScreen> bootSplash;
    {
        QPixmap splashPixmap(460, 220);
        splashPixmap.fill(Qt::transparent);

        QPainter painter(&splashPixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        // Match the launcher's 14/700 corner-radius ratio and translucent
        // #1a1a1a surface instead of using an opaque native window rectangle.
        painter.setPen(QPen(QColor(45, 45, 45, 220), 1));
        painter.setBrush(QColor(26, 26, 26, 204));
        painter.drawRoundedRect(QRectF(0.5, 0.5,
                                      splashPixmap.width() - 1.0,
                                      splashPixmap.height() - 1.0),
                                9.2, 9.2);

        QFont godrollFont("Space Grotesk");
        godrollFont.setPixelSize(36);
        godrollFont.setWeight(QFont::Bold);
        godrollFont.setLetterSpacing(QFont::AbsoluteSpacing, 2);
        const QFontMetricsF godrollMetrics(godrollFont);

        QFont tvFont("Space Grotesk");
        tvFont.setPixelSize(16);
        tvFont.setWeight(QFont::Bold);
        const QFontMetricsF tvMetrics(tvFont);

        constexpr qreal iconSize = 40;
        constexpr qreal iconSpacing = 12;
        const qreal godrollWidth = godrollMetrics.horizontalAdvance("GODROLL");
        const qreal tvWidth = tvMetrics.horizontalAdvance("TV");
        const qreal rowWidth = iconSize + iconSpacing + godrollWidth + tvWidth;
        const qreal rowHeight = qMax(iconSize, godrollMetrics.height());
        const qreal rowX = (splashPixmap.width() - rowWidth) / 2.0;
        const qreal rowY = 52;

        QPixmap logo = QIcon(":/qt/qml/GodrollLauncher/resources/logo.svg")
                           .pixmap(iconSize, iconSize);
        painter.drawPixmap(QPointF(rowX, rowY + (rowHeight - iconSize) / 2.0),
                           logo);

        const qreal godrollX = rowX + iconSize + iconSpacing;
        const qreal godrollTop = rowY + (rowHeight - godrollMetrics.height()) / 2.0;
        painter.setFont(godrollFont);
        painter.setPen(QColor("#ffffff"));
        painter.drawText(QPointF(godrollX,
                                 godrollTop + godrollMetrics.ascent()),
                         "GODROLL");

        painter.setFont(tvFont);
        painter.setPen(QColor("#09d7d0"));
        painter.drawText(QPointF(godrollX + godrollWidth,
                                 godrollTop + 6 + tvMetrics.ascent()),
                         "TV");
        painter.end();

        bootSplash = std::make_unique<BootSplashScreen>(splashPixmap,
            Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
        bootSplash->setAttribute(Qt::WA_TranslucentBackground);
        bootSplash->setFont(QFont("Space Grotesk", 10));
        if (!startHidden)
            bootSplash->show();
    }

    constexpr int bootStageTimeoutMs = 10000;
    constexpr int maxBootAttempts = 3;
    QTimer bootWatchdog;
    bootWatchdog.setSingleShot(true);
    QString currentBootStage;
    bool bootFinished = false;
    bool retryScheduled = false;

    auto restartApplication = [&app, &sharedMemory, &bootWatchdog]
                              (int nextAttempt) {
        QStringList arguments;
        const QStringList currentArguments = QCoreApplication::arguments().mid(1);
        for (const QString &argument : currentArguments) {
            if (argument.startsWith("--boot-attempt=") ||
                argument.startsWith("--boot-handoff=") ||
                argument == "--retry-offered") {
                continue;
            }
            arguments.append(argument);
        }
        arguments.append(QString("--boot-attempt=%1").arg(nextAttempt));
        const QString handoffName = QString("GodrollLauncherBootHandoff_%1_%2")
            .arg(QCoreApplication::applicationPid())
            .arg(QUuid::createUuid().toString(QUuid::Id128));
        auto *handoffServer = new QLocalServer(&app);
        QLocalServer::removeServer(handoffName);
        if (!handoffServer->listen(handoffName)) {
            handoffServer->deleteLater();
            return false;
        }
        arguments.append(QString("--boot-handoff=%1").arg(handoffName));

        QObject::connect(handoffServer, &QLocalServer::newConnection,
                         &app, [&app, handoffServer]() {
            // The replacement process connects only after its splash is drawn.
            // Keep this splash visible until that handoff is complete.
            while (handoffServer->hasPendingConnections()) {
                if (QLocalSocket *socket = handoffServer->nextPendingConnection())
                    socket->deleteLater();
            }
            handoffServer->close();
            app.quit();
        });

        bootWatchdog.stop();
        sharedMemory.detach();
        const bool started = QProcess::startDetached(
            QCoreApplication::applicationFilePath(), arguments,
            QCoreApplication::applicationDirPath());
        if (started) {
            return true;
        }

        // Restore the single-instance guard if Windows could not spawn the retry.
        handoffServer->close();
        handoffServer->deleteLater();
        sharedMemory.create(1);
        return false;
    };

    auto setBootStage = [&app, &bootSplash, &bootWatchdog,
                         &currentBootStage, &bootFinished,
                         &retryScheduled, failBoot,
                         simulatedFailureStageIndex,
                         simulatedBootStages](const QString &stage) {
        const int stageIndex = simulatedBootStages.indexOf(stage);
        if (failBoot && stageIndex >= 0 &&
            stageIndex > simulatedFailureStageIndex) {
            return;
        }

        qDebug() << "Boot stage:" << stage;
        currentBootStage = stage;
        if (bootSplash) {
            bootSplash->setStage(stage);
            app.processEvents(QEventLoop::ExcludeUserInputEvents);
        }
        if (!bootFinished && !retryScheduled)
            bootWatchdog.start(bootStageTimeoutMs);
    };

    std::function<void(const QString &)> handleBootFailure;
    handleBootFailure = [&app, &bootSplash, &bootWatchdog, &setBootStage,
                         &restartApplication, &bootFinished, &retryScheduled,
                         bootAttempt](const QString &reason) {
        if (bootFinished || retryScheduled)
            return;

        if (bootAttempt < maxBootAttempts) {
            retryScheduled = true;
            qWarning() << "Boot attempt failed:" << reason;
            setBootStage(QString("This is taking longer than expected. Trying again (%1 of %2)...")
                             .arg(bootAttempt + 1)
                             .arg(maxBootAttempts));
            bootWatchdog.stop();
            QTimer::singleShot(1000, &app,
                [&restartApplication, &retryScheduled, bootAttempt]() {
                    if (!restartApplication(bootAttempt + 1))
                        retryScheduled = false;
                });
            return;
        }

        qWarning() << "Boot retries exhausted:" << reason;
        if (bootSplash) {
            bootSplash->showRetryButton(true,
                "Godroll TV couldn't start. Check your connection, then try again.");
        }
        setBootStage("Godroll TV couldn't start.");
        bootWatchdog.stop();
    };

    QObject::connect(&bootWatchdog, &QTimer::timeout, &app,
                     [&handleBootFailure, &currentBootStage]() {
        handleBootFailure(QString("Timed out at %1.").arg(currentBootStage));
    });

    if (bootSplash) {
        bootSplash->setRetryHandler([&restartApplication]() {
            restartApplication(1);
        });
        bootSplash->showRetryButton(false);
    }

    if (!bootHandoffServer.isEmpty()) {
        // Notify the previous attempt only after this process has painted its
        // splash (including the retry state), preventing a visible gap.
        app.processEvents(QEventLoop::ExcludeUserInputEvents);
        QLocalSocket handoffSocket;
        handoffSocket.connectToServer(bootHandoffServer, QIODevice::WriteOnly);
        handoffSocket.waitForConnected(2000);
        handoffSocket.disconnectFromServer();
    }

    setBootStage("Getting things ready...");

    qDebug() << "App initialized";

    // Initialize components
    setBootStage("Preparing your weapon library...");
    WeaponLoader weaponLoader;
    WeaponSearchModel searchModel;

    setBootStage("Setting up your keyboard shortcut...");
    GlobalHotkey hotkey;

    setBootStage("Adding Godroll TV to the system tray...");
    TrayIcon trayIcon;

    setBootStage("Checking app services...");
    UpdateChecker updateChecker;
    ManifestChecker manifestChecker;

    // Keep the tray shortcut editor and status in sync with the native hotkey.
    QObject::connect(&trayIcon, &TrayIcon::hotkeyChangeRequested,
                     &hotkey, &GlobalHotkey::setShortcut);
    QObject::connect(&trayIcon, &TrayIcon::hotkeyEditingStarted,
                     &hotkey, &GlobalHotkey::suspendForShortcutCapture);
    QObject::connect(&trayIcon, &TrayIcon::hotkeyEditingFinished,
                     &hotkey, &GlobalHotkey::resumeAfterShortcutCapture);
    QObject::connect(&hotkey, &GlobalHotkey::shortcutChanged,
                     &trayIcon, [&trayIcon, &hotkey]() {
        trayIcon.setHotkeyStatus(hotkey.shortcutText(), hotkey.isRegistered());
    });
    QObject::connect(&hotkey, &GlobalHotkey::registeredChanged,
                     &trayIcon, [&trayIcon, &hotkey]() {
        trayIcon.setHotkeyStatus(hotkey.shortcutText(), hotkey.isRegistered());
    });
    QObject::connect(&hotkey, &GlobalHotkey::registrationFailed,
                     &trayIcon, [&trayIcon](const QString &message) {
        trayIcon.showHotkeyError(message);
    });
    trayIcon.setHotkeyValidator([&hotkey](const QString &shortcut, QString &message) {
        return hotkey.validateShortcut(shortcut, message);
    });
    trayIcon.setHotkeyStatus(hotkey.shortcutText(), hotkey.isRegistered());
    trayIcon.setBootComplete(false);

    // Before the QML interface is ready, Show is a recovery action for the
    // native splash rather than a request to expose a half-initialized window.
    QObject::connect(&trayIcon, &TrayIcon::forceShowRequested,
                     &app, [&trayIcon, &bootSplash]() {
        if (trayIcon.isBootComplete() || !bootSplash)
            return;
        bootSplash->show();
        bootSplash->raise();
        bootSplash->activateWindow();
    });
    
    // Show tray icon
    trayIcon.show();

    if (updateInstallFailed) {
        QTimer::singleShot(1000, &trayIcon, [&trayIcon]() {
            trayIcon.showUpdateError(
                "Godroll TV kept your previous version. Please try the update again.");
        });
    }

    // Connect tray icon exit signal
    QObject::connect(&trayIcon, &TrayIcon::exitRequested, &app, &QApplication::quit);
    
    qDebug() << "Components created";

    // Connect reload signal to update search model
    QObject::connect(&weaponLoader, &WeaponLoader::weaponsLoaded, 
                     &searchModel, &WeaponSearchModel::setWeapons);

    // Manifest checker: reload weapons when manifest version changes
    QObject::connect(&manifestChecker, &ManifestChecker::manifestChanged,
                     &weaponLoader, &WeaponLoader::reload);

    // Tray icon auto-refresh toggle syncs with manifest checker
    QObject::connect(&trayIcon, &TrayIcon::autoRefreshToggled,
                     &manifestChecker, &ManifestChecker::setAutoRefresh);

    // Initial manifest check after weapons are first loaded
    QObject::connect(&searchModel, &WeaponSearchModel::weaponsLoaded,
                     &manifestChecker, [&manifestChecker]() {
        static bool firstLoad = true;
        if (firstLoad) {
            firstLoad = false;
            manifestChecker.checkNow();
        }
    });

    setBootStage("Preparing the launcher...");
    QQmlApplicationEngine engine;
    
    // Expose C++ objects to QML
    engine.rootContext()->setContextProperty("searchModel", &searchModel);
    engine.rootContext()->setContextProperty("hotkey", &hotkey);
    engine.rootContext()->setContextProperty("trayIcon", &trayIcon);
    engine.rootContext()->setContextProperty("weaponLoader", &weaponLoader);
    engine.rootContext()->setContextProperty("updateChecker", &updateChecker);
    engine.rootContext()->setContextProperty("manifestChecker", &manifestChecker);
    engine.rootContext()->setContextProperty("startHidden", startHidden);
    engine.rootContext()->setContextProperty("appVersion", APP_VERSION);

    const QUrl url(QStringLiteral("qrc:/qt/qml/GodrollLauncher/qml/main.qml"));
    
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
            qDebug() << "Failed to load QML!";
            QCoreApplication::exit(-1);
        } else {
            qDebug() << "QML loaded successfully!";
        }
    }, Qt::QueuedConnection);

    qDebug() << "Loading QML from:" << url;
    engine.load(url);

    if (!engine.rootObjects().isEmpty()) {
        auto *mainWindow = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        trayIcon.setMainWindow(mainWindow);
        if (mainWindow) {
            QObject::connect(mainWindow, &QQuickWindow::closing,
                             &app, [&app]() { app.quit(); });
        }
    }

    auto bootCompletionScheduled = std::make_shared<bool>(false);
    auto completeBoot = [&app, &engine, &bootSplash, &setBootStage,
                         &bootWatchdog, &bootFinished, &retryScheduled, &trayIcon,
                         bootCompletionScheduled, holdSplash](const QString &stage, int delayMs) {
        if (*bootCompletionScheduled)
            return;
        *bootCompletionScheduled = true;
        bootFinished = true;
        retryScheduled = false;
        bootWatchdog.stop();
        if (bootSplash)
            bootSplash->showRetryButton(false);
        setBootStage(stage);

        // Diagnostic launch mode: keep the event loop, tray and shortcut editor
        // alive while deliberately stopping before the QML window is completed.
        if (holdSplash)
            return;

        QTimer::singleShot(delayMs, &app,
                           [&app, &engine, &bootSplash, &trayIcon]() {
            if (bootSplash) {
                bootSplash->allowClose();
                bootSplash->close();
                app.processEvents(QEventLoop::ExcludeUserInputEvents);
            }

            if (!engine.rootObjects().isEmpty()) {
                QMetaObject::invokeMethod(engine.rootObjects().constFirst(),
                                          "completeBoot", Qt::DirectConnection);
                trayIcon.setBootComplete(true);
            }
        });
    };

    QObject::connect(&weaponLoader, &WeaponLoader::weaponsLoaded,
                     &app, [&completeBoot](const QJsonArray &) {
        completeBoot("Ready", 250);
    });
    QObject::connect(&weaponLoader, &WeaponLoader::loadStatusChanged,
                     &app, [&setBootStage](const QString &message) {
        setBootStage(message);
    });
    QObject::connect(&weaponLoader, &WeaponLoader::loadFailed,
                     &app, [&handleBootFailure](const QString &message) {
        handleBootFailure(message);
    });

    // Start network work only after the native splash and QML scene exist.
    QTimer::singleShot(0, &weaponLoader,
                       [&weaponLoader, &setBootStage, failBoot,
                        simulatedFailureStageIndex]() {
        if (failBoot) {
            // Continue through the asynchronous stages in their real order,
            // then stop precisely at the randomly selected point. Later stages
            // are filtered by setBootStage and can never appear out of order.
            if (simulatedFailureStageIndex < 6)
                return;

            setBootStage("Loading your weapons...");
            if (simulatedFailureStageIndex < 7)
                return;

            QTimer::singleShot(100, &weaponLoader,
                [&weaponLoader, &setBootStage, simulatedFailureStageIndex]() {
                    setBootStage("Connecting to Godroll TV...");
                    if (simulatedFailureStageIndex < 8)
                        return;

                    QTimer::singleShot(100, &weaponLoader, [&setBootStage]() {
                        setBootStage("Organizing your weapons...");
                    });
                });
            return;
        }
        setBootStage("Loading your weapons...");
        // Process-level boot retries own the retry budget during startup.
        weaponLoader.loadWeapons({}, 0);
    });

    return app.exec();
}
