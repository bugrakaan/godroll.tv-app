#ifndef TRAYICON_H
#define TRAYICON_H

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>

class TrayIcon : public QObject
{
    Q_OBJECT

public:
    explicit TrayIcon(QObject *parent = nullptr);
    ~TrayIcon();

    void show();
    void hide();

signals:
    void showHideRequested();
    void exitRequested();

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);
    void onStartupToggled(bool checked);

private:
    bool isStartupEnabled() const;
    void setStartupEnabled(bool enabled);
    QString getExecutablePath() const;
    void initializeStartup();

    QSystemTrayIcon *m_trayIcon;
    QMenu *m_menu;
    QAction *m_showHideAction;
    QAction *m_startupAction;
    QAction *m_exitAction;
};

#endif // TRAYICON_H
