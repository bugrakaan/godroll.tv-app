#include "trayicon.h"
#include <QApplication>
#include <QIcon>
#include <QPainter>
#include <QPolygon>
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QEvent>
#include <QKeyEvent>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWindow>

#ifdef Q_OS_MAC
#include <QProcess>
#endif

namespace {
class ShortcutCaptureEdit final : public QKeySequenceEdit
{
public:
    explicit ShortcutCaptureEdit(QWidget *parent = nullptr)
        : QKeySequenceEdit(parent)
    {
        setMaximumSequenceLength(1);
        setFocusPolicy(Qt::StrongFocus);
    }

protected:
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::ShortcutOverride) {
            event->accept();
            return true;
        }

        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            const int key = keyEvent->key();

            // Modifier-only presses are kept pending until a base key arrives.
            if (key == Qt::Key_Control || key == Qt::Key_Shift ||
                key == Qt::Key_Alt || key == Qt::Key_Meta ||
                key == Qt::Key_AltGr) {
                keyEvent->accept();
                return true;
            }

            Qt::KeyboardModifiers modifiers = keyEvent->modifiers() &
                (Qt::ShiftModifier | Qt::ControlModifier |
                 Qt::AltModifier | Qt::MetaModifier);
            setKeySequence(QKeySequence(QKeyCombination(modifiers,
                static_cast<Qt::Key>(key))));
            keyEvent->accept();
            return true;
        }

        if (event->type() == QEvent::KeyRelease) {
            event->accept();
            return true;
        }

        return QKeySequenceEdit::event(event);
    }
};
}

TrayIcon::TrayIcon(QObject *parent)
    : QObject(parent)
    , m_trayIcon(new QSystemTrayIcon(this))
    , m_menu(new QMenu())
{
    // Set icon - use the app logo
    QIcon appIcon(":/qt/qml/GodrollLauncher/resources/logo.svg");
    if (appIcon.isNull()) {
        // Fallback to a diamond shape if icon not found
        QPixmap pixmap(32, 32);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QColor("#09d7d0"));
        painter.setPen(Qt::NoPen);
        QPolygon diamond;
        diamond << QPoint(16, 0) << QPoint(32, 16) << QPoint(16, 32) << QPoint(0, 16);
        painter.drawPolygon(diamond);
        m_trayIcon->setIcon(QIcon(pixmap));
    } else {
        m_trayIcon->setIcon(appIcon);
    }
    
    // Set tooltip with version
    QString version = QCoreApplication::applicationVersion();
    m_trayIcon->setToolTip(QString("Godroll.tv Launcher v%1").arg(version));

    // Auto-register startup on first run, or update path if already registered
    initializeStartup();

    // Create title action with version (non-clickable header)
    QAction *titleAction = new QAction(QString("Godroll.tv v%1").arg(version), this);
    titleAction->setEnabled(false);  // Make it non-clickable
    QFont titleFont;
    titleFont.setBold(true);
    titleAction->setFont(titleFont);

    // Create menu actions
    m_showAction = new QAction("Show", this);
    connect(m_showAction, &QAction::triggered, this, &TrayIcon::forceShowRequested);

    m_startupAction = new QAction("Start at Login", this);
    m_startupAction->setCheckable(true);
    m_startupAction->setChecked(isStartupEnabled());
    connect(m_startupAction, &QAction::toggled, this, &TrayIcon::onStartupToggled);

    m_autoRefreshAction = new QAction("Auto Refresh Weapon List", this);
    m_autoRefreshAction->setCheckable(true);
    QSettings autoRefreshSettings("Godroll.tv", "GodrollLauncher");
    m_autoRefreshAction->setChecked(autoRefreshSettings.value("autoRefreshWeapons", true).toBool());
    connect(m_autoRefreshAction, &QAction::toggled, this, &TrayIcon::autoRefreshToggled);

    m_hotkeyAction = new QAction("Change Global Shortcut", this);
    connect(m_hotkeyAction, &QAction::triggered, this, &TrayIcon::hotkeyEditorRequested);

    m_checkUpdatesAction = new QAction("Check for Updates", this);
    connect(m_checkUpdatesAction, &QAction::triggered, this, &TrayIcon::checkForUpdatesRequested);

    m_exitAction = new QAction("Exit", this);
    connect(m_exitAction, &QAction::triggered, this, &TrayIcon::exitRequested);

    // Build menu
    m_menu->addAction(titleAction);
    m_menu->addAction(m_showAction);
    m_menu->addSeparator();
    m_menu->addAction(m_startupAction);
    m_menu->addAction(m_autoRefreshAction);
    m_menu->addAction(m_hotkeyAction);
    m_menu->addAction(m_checkUpdatesAction);
    m_menu->addSeparator();
    m_menu->addAction(m_exitAction);

    m_trayIcon->setContextMenu(m_menu);

    // Connect tray icon activation
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &TrayIcon::onActivated);
}

TrayIcon::~TrayIcon()
{
    delete m_menu;
}

void TrayIcon::show()
{
    m_trayIcon->show();
}

void TrayIcon::hide()
{
    m_trayIcon->hide();
}

void TrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        emit forceShowRequested();
    } else if (reason == QSystemTrayIcon::Trigger) {
        emit showHideRequested();
    }
}

void TrayIcon::setHotkeyStatus(const QString &shortcut, bool registered)
{
    m_currentShortcut = shortcut;
    if (registered) {
        m_hotkeyAction->setText("Change Global Shortcut");
    } else {
        m_hotkeyAction->setText("Change Global Shortcut (Unavailable)");
    }
}

void TrayIcon::showHotkeyError(const QString &message)
{
    m_trayIcon->showMessage("Global Shortcut Unavailable", message,
                            QSystemTrayIcon::Warning, 7000);
}

void TrayIcon::showUpdateError(const QString &message)
{
    m_trayIcon->showMessage("Update Couldn't Be Installed", message,
                            QSystemTrayIcon::Warning, 10000);
}

void TrayIcon::setHotkeyValidator(
    const std::function<bool(const QString &, QString &)> &validator)
{
    m_hotkeyValidator = validator;
}

void TrayIcon::setMainWindow(QWindow *window)
{
    m_mainWindow = window;
}

void TrayIcon::setBootComplete(bool complete)
{
    m_bootComplete = complete;

    // Show and Exit are recovery controls and must remain usable throughout boot.
    m_showAction->setEnabled(true);
    m_exitAction->setEnabled(true);
    m_startupAction->setEnabled(complete);
    m_autoRefreshAction->setEnabled(complete);
    m_hotkeyAction->setEnabled(complete);
    m_checkUpdatesAction->setEnabled(complete);
}

void TrayIcon::onStartupToggled(bool checked)
{
    setStartupEnabled(checked);
}

void TrayIcon::onChangeHotkeyRequested()
{
    QDialog dialog;
    dialog.setObjectName("shortcutDialog");
    dialog.setWindowTitle("Change Global Shortcut");
    dialog.setWindowIcon(m_trayIcon->icon());
    dialog.setModal(true);
    dialog.setMinimumWidth(440);
    dialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint |
                          Qt::WindowStaysOnTopHint);
    dialog.setAttribute(Qt::WA_TranslucentBackground);
    dialog.setStyleSheet(R"(
        QDialog#shortcutDialog {
            background-color: transparent;
            color: #ffffff;
        }
        QFrame#dialogSurface {
            background-color: rgba(26, 26, 26, 248);
            border: 1px solid #2d2d2d;
            border-radius: 14px;
        }
        QLabel {
            color: #d5d5d5;
            font-family: "Space Grotesk", "Segoe UI";
        }
        QKeySequenceEdit {
            min-height: 42px;
            padding: 0 12px;
            color: #ffffff;
            background-color: rgba(35, 35, 35, 210);
            border: 2px solid #2d2d2d;
            border-radius: 10px;
            font-family: "Space Grotesk", "Segoe UI";
            font-size: 14px;
        }
        QKeySequenceEdit:focus {
            border-color: #09d7d0;
        }
        QPushButton {
            min-width: 82px;
            min-height: 34px;
            padding: 0 14px;
            color: #e8e8e8;
            background-color: #2b2b2b;
            border: 1px solid #3b3b3b;
            border-radius: 8px;
            font-family: "Space Grotesk", "Segoe UI";
            font-weight: 600;
        }
        QPushButton:hover {
            border-color: #09d7d0;
        }
        QPushButton:default {
            color: #151515;
            background-color: #09d7d0;
            border-color: #09d7d0;
        }
        QPushButton:default:hover {
            background-color: #0bc5bf;
        }
        QPushButton:disabled {
            color: #686868;
            background-color: #242424;
            border-color: #303030;
        }
    )");

    auto *outerLayout = new QVBoxLayout(&dialog);
    outerLayout->setContentsMargins(8, 8, 8, 8);

    auto *surface = new QFrame(&dialog);
    surface->setObjectName("dialogSurface");
    outerLayout->addWidget(surface);

    auto *layout = new QVBoxLayout(surface);
    layout->setContentsMargins(24, 22, 24, 20);
    layout->setSpacing(12);

    auto *title = new QLabel("Global Shortcut");
    title->setStyleSheet("color: #ffffff; font-size: 18px; font-weight: 700;");
    layout->addWidget(title);

    auto *description = new QLabel("Press a key combination to toggle Godroll Launcher.");
    description->setWordWrap(true);
    layout->addWidget(description);

    auto *shortcutEdit = new ShortcutCaptureEdit(&dialog);
    if (!m_currentShortcut.isEmpty()) {
        shortcutEdit->setKeySequence(QKeySequence(m_currentShortcut, QKeySequence::NativeText));
    }
    layout->addWidget(shortcutEdit);

    auto *hint = new QLabel("Letters and numbers require a modifier. Available F1-F24 keys may be used alone.");
    hint->setStyleSheet("color: #8b8b8b; font-size: 12px;");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *statusLabel = new QLabel();
    statusLabel->setWordWrap(true);
    statusLabel->setMinimumHeight(20);
    layout->addWidget(statusLabel);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel,
                                         Qt::Horizontal, &dialog);
    layout->addWidget(buttons);
    QPushButton *saveButton = buttons->button(QDialogButtonBox::Save);
    saveButton->setDefault(true);

    auto validateSelection = [&]() -> bool {
        QKeySequence sequence = shortcutEdit->keySequence();
        QString message;
        bool valid = false;

        if (sequence.isEmpty()) {
            message = "Press a shortcut to continue.";
        } else if (m_hotkeyValidator) {
            valid = m_hotkeyValidator(sequence.toString(QKeySequence::PortableText), message);
        } else {
            message = "Shortcut validation is unavailable.";
        }

        saveButton->setEnabled(valid);
        statusLabel->setText(message);
        statusLabel->setStyleSheet(valid
            ? "color: #09d7d0; font-size: 12px;"
            : "color: #ef6464; font-size: 12px;");
        return valid;
    };

    connect(shortcutEdit, &QKeySequenceEdit::keySequenceChanged,
            &dialog, [&](const QKeySequence &) { validateSelection(); });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (!validateSelection()) {
            return;
        }
        QKeySequence sequence = shortcutEdit->keySequence();
        emit hotkeyChangeRequested(sequence.toString(QKeySequence::PortableText));
        dialog.accept();
    });

    validateSelection();
    emit hotkeyEditingStarted();

    if (m_mainWindow) {
        m_mainWindow->show();
        m_mainWindow->raise();
        m_mainWindow->requestActivate();
        dialog.winId();
        dialog.windowHandle()->setTransientParent(m_mainWindow);
    }

    shortcutEdit->setFocus();
    dialog.exec();
    emit hotkeyEditingFinished();
}

bool TrayIcon::isStartupEnabled() const
{
#ifdef Q_OS_MAC
    QString launchAgentPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) 
                              + "/Library/LaunchAgents/tv.godroll.launcher.plist";
    return QFile::exists(launchAgentPath);
#else
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    return settings.contains("GodrollLauncher");
#endif
}

void TrayIcon::setStartupEnabled(bool enabled)
{
#ifdef Q_OS_MAC
    QString launchAgentDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) 
                             + "/Library/LaunchAgents";
    QString launchAgentPath = launchAgentDir + "/tv.godroll.launcher.plist";
    
    if (enabled) {
        // Create LaunchAgents directory if it doesn't exist
        QDir().mkpath(launchAgentDir);
        
        // Get the app bundle path
        QString appPath = QApplication::applicationDirPath();
        // Go up from Contents/MacOS to get .app bundle
        appPath = QDir(appPath).absolutePath();
        appPath = appPath.replace("/Contents/MacOS", "");
        
        QString plistContent = QString(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
            "<plist version=\"1.0\">\n"
            "<dict>\n"
            "    <key>Label</key>\n"
            "    <string>tv.godroll.launcher</string>\n"
            "    <key>ProgramArguments</key>\n"
            "    <array>\n"
            "        <string>%1/Contents/MacOS/GodrollLauncher</string>\n"
            "        <string>--hidden</string>\n"
            "    </array>\n"
            "    <key>RunAtLoad</key>\n"
            "    <true/>\n"
            "    <key>KeepAlive</key>\n"
            "    <false/>\n"
            "</dict>\n"
            "</plist>\n"
        ).arg(appPath);
        
        QFile file(launchAgentPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(plistContent.toUtf8());
            file.close();
            qDebug() << "Created launch agent at:" << launchAgentPath;
        }
    } else {
        QFile::remove(launchAgentPath);
        qDebug() << "Removed launch agent";
    }
#else
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    
    if (enabled) {
        QString exePath = getExecutablePath();
        // Add --hidden flag so app starts minimized to tray
        settings.setValue("GodrollLauncher", QString("\"%1\" --hidden").arg(exePath));
    } else {
        settings.remove("GodrollLauncher");
    }
#endif
}

QString TrayIcon::getExecutablePath() const
{
    return QDir::toNativeSeparators(QApplication::applicationFilePath());
}

void TrayIcon::initializeStartup()
{
    QSettings appSettings("Godroll.tv", "GodrollLauncher");
    bool isFirstRun = !appSettings.contains("startupInitialized");
    
    if (isFirstRun) {
        // First run: enable startup by default
        appSettings.setValue("startupInitialized", true);
        setStartupEnabled(true);
    } else if (isStartupEnabled()) {
        // Already registered: update path in case user moved the folder
        setStartupEnabled(true);
    }
}
