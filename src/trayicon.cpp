#include "trayicon.h"
#include <QApplication>
#include <QIcon>
#include <QPainter>
#include <QPolygon>
#include <QSettings>
#include <QDir>

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
    m_trayIcon->setToolTip("Godroll.tv Launcher");

    // Auto-register startup on first run, or update path if already registered
    initializeStartup();

    // Create title action (non-clickable header)
    QAction *titleAction = new QAction("Godroll.tv", this);
    titleAction->setEnabled(false);  // Make it non-clickable
    QFont titleFont;
    titleFont.setBold(true);
    titleAction->setFont(titleFont);

    // Create menu actions
    m_showHideAction = new QAction("Show/Hide", this);
    connect(m_showHideAction, &QAction::triggered, this, &TrayIcon::showHideRequested);

    m_startupAction = new QAction("Start with Windows", this);
    m_startupAction->setCheckable(true);
    m_startupAction->setChecked(isStartupEnabled());
    connect(m_startupAction, &QAction::toggled, this, &TrayIcon::onStartupToggled);

    m_exitAction = new QAction("Exit", this);
    connect(m_exitAction, &QAction::triggered, this, &TrayIcon::exitRequested);

    // Build menu
    m_menu->addAction(titleAction);
    m_menu->addSeparator();
    m_menu->addAction(m_showHideAction);
    m_menu->addAction(m_startupAction);
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
    if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) {
        emit showHideRequested();
    }
}

void TrayIcon::onStartupToggled(bool checked)
{
    setStartupEnabled(checked);
}

bool TrayIcon::isStartupEnabled() const
{
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    return settings.contains("GodrollLauncher");
}

void TrayIcon::setStartupEnabled(bool enabled)
{
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    
    if (enabled) {
        QString exePath = getExecutablePath();
        // Add --hidden flag so app starts minimized to tray
        settings.setValue("GodrollLauncher", QString("\"%1\" --hidden").arg(exePath));
    } else {
        settings.remove("GodrollLauncher");
    }
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
