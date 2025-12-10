#ifndef GLOBALHOTKEY_H
#define GLOBALHOTKEY_H

#include <QObject>
#include <QAbstractNativeEventFilter>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

#ifdef Q_OS_MAC
#include <Carbon/Carbon.h>
#endif

class GlobalHotkey : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit GlobalHotkey(QObject *parent = nullptr);
    ~GlobalHotkey();

    bool registerHotkey(int key, int modifiers);
    void unregisterHotkey();

signals:
    void activated();

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
#else
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;
#endif

private:
#ifdef Q_OS_WIN
    int m_hotkeyId;
    HWND m_hwnd;
#endif

#ifdef Q_OS_MAC
    EventHotKeyRef m_hotkeyRef;
    static OSStatus hotkeyHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData);
#endif
};

#endif // GLOBALHOTKEY_H
