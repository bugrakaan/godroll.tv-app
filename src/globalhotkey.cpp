#include "globalhotkey.h"
#include <QCoreApplication>
#include <QDebug>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

#ifdef Q_OS_MAC
// Static instance pointer for callback
static GlobalHotkey* s_instance = nullptr;
#endif

GlobalHotkey::GlobalHotkey(QObject *parent)
    : QObject(parent)
#ifdef Q_OS_WIN
    , m_hotkeyId(1)
    , m_hwnd(nullptr)
#endif
#ifdef Q_OS_MAC
    , m_hotkeyRef(nullptr)
#endif
{
    QCoreApplication::instance()->installNativeEventFilter(this);
    
#ifdef Q_OS_MAC
    s_instance = this;
    // Register default hotkey: Cmd + G
    // kVK_ANSI_G = 0x05, cmdKey = 0x0100
    registerHotkey(0x05, cmdKey);
#endif
    
    // Register default hotkey: Alt + G (0x47 is 'G' key)
#ifdef Q_OS_WIN
    registerHotkey(0x47, MOD_ALT);
#endif
}

GlobalHotkey::~GlobalHotkey()
{
    unregisterHotkey();
    QCoreApplication::instance()->removeNativeEventFilter(this);
#ifdef Q_OS_MAC
    s_instance = nullptr;
#endif
}

#ifdef Q_OS_MAC
OSStatus GlobalHotkey::hotkeyHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData)
{
    Q_UNUSED(nextHandler);
    Q_UNUSED(event);
    Q_UNUSED(userData);
    
    if (s_instance) {
        qDebug() << "Hotkey pressed! (macOS)";
        QMetaObject::invokeMethod(s_instance, "activated", Qt::QueuedConnection);
    }
    return noErr;
}
#endif

bool GlobalHotkey::registerHotkey(int key, int modifiers)
{
#ifdef Q_OS_MAC
    // Unregister existing hotkey first
    unregisterHotkey();
    
    // Install event handler
    EventTypeSpec eventType;
    eventType.eventClass = kEventClassKeyboard;
    eventType.eventKind = kEventHotKeyPressed;
    
    InstallApplicationEventHandler(&hotkeyHandler, 1, &eventType, nullptr, nullptr);
    
    // Register the hotkey
    EventHotKeyID hotkeyID;
    hotkeyID.signature = 'GDRL';
    hotkeyID.id = 1;
    
    OSStatus status = RegisterEventHotKey(key, modifiers, hotkeyID, 
                                          GetApplicationEventTarget(), 0, &m_hotkeyRef);
    
    if (status == noErr) {
        qDebug() << "Hotkey registered successfully: Cmd+G (macOS)";
        return true;
    } else {
        qWarning() << "Failed to register hotkey. Error:" << status;
        return false;
    }
#endif

#ifdef Q_OS_WIN
    // Create a message-only window for receiving hotkey events
    if (!m_hwnd) {
        m_hwnd = CreateWindowEx(
            0, L"STATIC", L"GlobalHotkeyWindow",
            0, 0, 0, 0, 0,
            HWND_MESSAGE, nullptr, nullptr, nullptr
        );
        
        if (!m_hwnd) {
            qWarning() << "Failed to create message window. Error:" << GetLastError();
            return false;
        }
    }

    // Unregister existing hotkey
    unregisterHotkey();

    // Register new hotkey
    if (RegisterHotKey(m_hwnd, m_hotkeyId, modifiers | MOD_NOREPEAT, key)) {
        qDebug() << "Hotkey registered successfully: Alt+G";
        return true;
    } else {
        qWarning() << "Failed to register hotkey. Error:" << GetLastError();
    }
#endif
    return false;
}

void GlobalHotkey::unregisterHotkey()
{
#ifdef Q_OS_MAC
    if (m_hotkeyRef) {
        UnregisterEventHotKey(m_hotkeyRef);
        m_hotkeyRef = nullptr;
    }
#endif

#ifdef Q_OS_WIN
    if (m_hwnd) {
        UnregisterHotKey(m_hwnd, m_hotkeyId);
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
#endif
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool GlobalHotkey::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
#else
bool GlobalHotkey::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
#endif
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        
        if (msg->message == WM_HOTKEY) {
            if (msg->wParam == m_hotkeyId) {
                qDebug() << "Hotkey pressed!";
                emit activated();
                return true;
            }
        }
    }
#endif
    
    Q_UNUSED(result);
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    return false;
}
