#ifndef GLOBALHOTKEY_H
#define GLOBALHOTKEY_H

#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QKeySequence>
#include <QVariantMap>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

#ifdef Q_OS_MAC
#include <Carbon/Carbon.h>
#endif

class GlobalHotkey : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
    Q_PROPERTY(QString shortcutText READ shortcutText NOTIFY shortcutChanged)
    Q_PROPERTY(bool registered READ isRegistered NOTIFY registeredChanged)

public:
    explicit GlobalHotkey(QObject *parent = nullptr);
    ~GlobalHotkey();

    bool registerHotkey(int key, int modifiers);
    void unregisterHotkey();
    QString shortcutText() const { return m_shortcutText; }
    bool isRegistered() const { return m_registered; }
    QString lastErrorMessage() const { return m_lastErrorMessage; }
    bool validateShortcut(const QString &shortcut, QString &message) const;
    Q_INVOKABLE QVariantMap validateShortcutForUi(const QString &shortcut) const;
    Q_INVOKABLE QString shortcutFromKey(int key, int modifiers) const;

public slots:
    bool setShortcut(const QString &shortcut);
    void suspendForShortcutCapture();
    void resumeAfterShortcutCapture();

signals:
    void activated();
    void shortcutChanged();
    void registeredChanged();
    void registrationFailed(const QString &message);

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
#else
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;
#endif

private:
    void setRegistered(bool registered);

#ifdef Q_OS_WIN
    bool registerSequence(const QKeySequence &sequence);
    bool sequenceToNative(const QKeySequence &sequence, UINT &key, UINT &modifiers) const;
#endif

    QString m_shortcutText;
    QString m_lastErrorMessage;
    bool m_registered = false;
    bool m_captureSuspended = false;
    bool m_wasRegisteredBeforeCapture = false;

#ifdef Q_OS_WIN
    int m_hotkeyId;
#endif

#ifdef Q_OS_MAC
    EventHotKeyRef m_hotkeyRef;
    static OSStatus hotkeyHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData);
#endif
};

#endif // GLOBALHOTKEY_H
