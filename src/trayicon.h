#ifndef TRAYICON_H
#define TRAYICON_H

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QPointer>
#include <functional>

class QWindow;

class TrayIcon : public QObject
{
    Q_OBJECT

public:
    explicit TrayIcon(QObject *parent = nullptr);
    ~TrayIcon();

    void show();
    void hide();
    void setHotkeyStatus(const QString &shortcut, bool registered);
    void showHotkeyError(const QString &message);
    void setHotkeyValidator(const std::function<bool(const QString &, QString &)> &validator);
    void setMainWindow(QWindow *window);
    void setBootComplete(bool complete);
    bool isBootComplete() const { return m_bootComplete; }

signals:
    void showHideRequested();
    void forceShowRequested();
    void exitRequested();
    void checkForUpdatesRequested();
    void autoRefreshToggled(bool checked);
    void hotkeyChangeRequested(const QString &shortcut);
    void hotkeyEditorRequested();
    void hotkeyEditingStarted();
    void hotkeyEditingFinished();

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);
    void onStartupToggled(bool checked);
    void onChangeHotkeyRequested();

private:
    bool isStartupEnabled() const;
    void setStartupEnabled(bool enabled);
    QString getExecutablePath() const;
    void initializeStartup();

    QSystemTrayIcon *m_trayIcon;
    QMenu *m_menu;
    QAction *m_showAction;
    QAction *m_startupAction;
    QAction *m_autoRefreshAction;
    QAction *m_hotkeyAction;
    QAction *m_checkUpdatesAction;
    QAction *m_exitAction;
    QString m_currentShortcut;
    bool m_bootComplete = false;
    QPointer<QWindow> m_mainWindow;
    std::function<bool(const QString &, QString &)> m_hotkeyValidator;
};

#endif // TRAYICON_H
