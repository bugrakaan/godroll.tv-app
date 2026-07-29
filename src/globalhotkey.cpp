#include "globalhotkey.h"
#include <QCoreApplication>
#include <QDebug>
#include <QSettings>

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
    m_shortcutText = "Cmd+G";
    setRegistered(registerHotkey(0x05, cmdKey));
#endif
    
    // Register default hotkey: Alt + G (0x47 is 'G' key)
#ifdef Q_OS_WIN
    QSettings settings("Godroll.tv", "GodrollLauncher");
    QString savedShortcut = settings.value("globalShortcut", "Alt+G").toString();
    QKeySequence sequence(savedShortcut, QKeySequence::PortableText);
    if (sequence.isEmpty()) {
        sequence = QKeySequence("Alt+G", QKeySequence::PortableText);
    }
    m_shortcutText = sequence.toString(QKeySequence::NativeText);
    if (!registerSequence(sequence)) {
        m_lastErrorMessage = QString("%1 is unavailable. Another app or Windows may already be using it. Choose a different shortcut from the tray menu.")
                                 .arg(m_shortcutText);
    }
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
    if (m_registered) {
        UnregisterHotKey(nullptr, m_hotkeyId);
    }

    if (RegisterHotKey(nullptr, m_hotkeyId, modifiers | MOD_NOREPEAT, key)) {
        setRegistered(true);
        return true;
    }

    qWarning() << "Failed to register hotkey. Error:" << GetLastError();
    setRegistered(false);
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
    if (m_registered) {
        UnregisterHotKey(nullptr, m_hotkeyId);
    }
#endif
    setRegistered(false);
}

void GlobalHotkey::setRegistered(bool registered)
{
    if (m_registered == registered)
        return;
    m_registered = registered;
    emit registeredChanged();
}

bool GlobalHotkey::validateShortcut(const QString &shortcut, QString &message) const
{
#ifdef Q_OS_WIN
    QKeySequence sequence(shortcut, QKeySequence::PortableText);
    if (sequence.isEmpty() || sequence.count() != 1) {
        message = "Press a single key combination.";
        return false;
    }

    QKeyCombination combination = sequence[0];
    int qtKey = combination.key();
    Qt::KeyboardModifiers modifiers = combination.keyboardModifiers();
    bool hasModifier = modifiers &
        (Qt::AltModifier | Qt::ControlModifier | Qt::ShiftModifier | Qt::MetaModifier);
    bool isAlphaNumeric = (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) ||
                          (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9);
    bool isFunctionKey = qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24;

    if (qtKey == Qt::Key_Tab || qtKey == Qt::Key_Backtab) {
        message = "Tab is reserved for navigation. Choose another shortcut.";
        return false;
    }
    if (qtKey == Qt::Key_Escape) {
        message = "Escape is reserved for closing dialogs. Choose another shortcut.";
        return false;
    }

    if (isAlphaNumeric && !hasModifier) {
        message = "Letters and numbers must be combined with Alt, Ctrl, Shift, or Windows.";
        return false;
    }
    if (!hasModifier && !isFunctionKey) {
        message = "Use a modifier key. Function keys can be used on their own when available.";
        return false;
    }

    UINT key = 0;
    UINT nativeModifiers = 0;
    if (!sequenceToNative(sequence, key, nativeModifiers)) {
        message = "This key combination is not supported. Choose another shortcut.";
        return false;
    }

    QKeySequence currentSequence(m_shortcutText, QKeySequence::NativeText);
    if (m_registered && !currentSequence.isEmpty() && currentSequence[0] == sequence[0]) {
        message = "Shortcut is available.";
        return true;
    }

    constexpr int validationHotkeyId = 0x4752;
    if (!RegisterHotKey(nullptr, validationHotkeyId,
                        nativeModifiers | MOD_NOREPEAT, key)) {
        message = "This shortcut is already in use. Choose another one.";
        return false;
    }
    UnregisterHotKey(nullptr, validationHotkeyId);

    message = "Shortcut is available.";
    return true;
#else
    Q_UNUSED(shortcut);
    message = "Changing the global shortcut is currently available on Windows only.";
    return false;
#endif
}

QVariantMap GlobalHotkey::validateShortcutForUi(const QString &shortcut) const
{
    QString message;
    const bool valid = validateShortcut(shortcut, message);
    return {
        {"valid", valid},
        {"message", message}
    };
}

QString GlobalHotkey::shortcutFromKey(int key, int modifiers) const
{
    const auto qtKey = static_cast<Qt::Key>(key);
    if (qtKey == Qt::Key_Control || qtKey == Qt::Key_Shift ||
        qtKey == Qt::Key_Alt || qtKey == Qt::Key_Meta ||
        qtKey == Qt::Key_AltGr || qtKey == Qt::Key_unknown) {
        return {};
    }

    const auto keyboardModifiers = static_cast<Qt::KeyboardModifiers>(modifiers) &
        (Qt::ShiftModifier | Qt::ControlModifier |
         Qt::AltModifier | Qt::MetaModifier);
    return QKeySequence(QKeyCombination(keyboardModifiers, qtKey))
        .toString(QKeySequence::PortableText);
}

bool GlobalHotkey::setShortcut(const QString &shortcut)
{
#ifdef Q_OS_WIN
    QString validationMessage;
    if (!validateShortcut(shortcut, validationMessage)) {
        emit registrationFailed(validationMessage);
        return false;
    }

    QKeySequence sequence(shortcut, QKeySequence::PortableText);
    if (sequence.isEmpty() || sequence.count() != 1) {
        emit registrationFailed("Choose a single key combination, such as Alt+G or Ctrl+Shift+G.");
        return false;
    }

    QString previousText = m_shortcutText;
    bool hadWorkingShortcut = m_registered;
    QKeySequence previousSequence(previousText, QKeySequence::NativeText);
    QString requestedText = sequence.toString(QKeySequence::NativeText);

    if (!registerSequence(sequence)) {
        QString message = m_lastErrorMessage;
        if (hadWorkingShortcut && !previousSequence.isEmpty()) {
            registerSequence(previousSequence);
        }
        emit registrationFailed(message);
        return false;
    }

    m_shortcutText = requestedText;
    m_lastErrorMessage.clear();
    QSettings settings("Godroll.tv", "GodrollLauncher");
    settings.setValue("globalShortcut", sequence.toString(QKeySequence::PortableText));
    emit shortcutChanged();
    return true;
#else
    Q_UNUSED(shortcut);
    emit registrationFailed("Changing the global shortcut is currently available on Windows only.");
    return false;
#endif
}

void GlobalHotkey::suspendForShortcutCapture()
{
#ifdef Q_OS_WIN
    if (m_captureSuspended)
        return;

    m_captureSuspended = true;
    m_wasRegisteredBeforeCapture = m_registered;
    if (m_wasRegisteredBeforeCapture) {
        // Keep the public registration state stable while the editor is open,
        // but release the native binding so it cannot consume the keys being captured.
        UnregisterHotKey(nullptr, m_hotkeyId);
    }
#endif
}

void GlobalHotkey::resumeAfterShortcutCapture()
{
#ifdef Q_OS_WIN
    if (!m_captureSuspended)
        return;

    const bool shouldRestore = m_wasRegisteredBeforeCapture;
    m_captureSuspended = false;
    m_wasRegisteredBeforeCapture = false;

    if (!shouldRestore)
        return;

    QKeySequence sequence(m_shortcutText, QKeySequence::NativeText);
    if (!registerSequence(sequence)) {
        emit registrationFailed(m_lastErrorMessage);
    }
#endif
}

#ifdef Q_OS_WIN
bool GlobalHotkey::registerSequence(const QKeySequence &sequence)
{
    UINT key = 0;
    UINT modifiers = 0;
    if (!sequenceToNative(sequence, key, modifiers)) {
        m_lastErrorMessage = "This key combination is not supported. Choose another shortcut.";
        return false;
    }

    if (!registerHotkey(static_cast<int>(key), static_cast<int>(modifiers))) {
        m_lastErrorMessage = QString("%1 is unavailable. Another app or Windows may already be using it. Choose a different shortcut.")
                                 .arg(sequence.toString(QKeySequence::NativeText));
        return false;
    }
    return true;
}

bool GlobalHotkey::sequenceToNative(const QKeySequence &sequence, UINT &key, UINT &modifiers) const
{
    if (sequence.isEmpty() || sequence.count() != 1)
        return false;

    QKeyCombination combination = sequence[0];
    Qt::KeyboardModifiers qtModifiers = combination.keyboardModifiers();
    int qtKey = combination.key();

    bool hasModifier = qtModifiers &
        (Qt::AltModifier | Qt::ControlModifier | Qt::ShiftModifier | Qt::MetaModifier);
    bool isFunctionKey = qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24;
    if (!hasModifier && !isFunctionKey)
        return false;

    modifiers = 0;
    if (qtModifiers & Qt::AltModifier) modifiers |= MOD_ALT;
    if (qtModifiers & Qt::ControlModifier) modifiers |= MOD_CONTROL;
    if (qtModifiers & Qt::ShiftModifier) modifiers |= MOD_SHIFT;
    if (qtModifiers & Qt::MetaModifier) modifiers |= MOD_WIN;

    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) {
        key = static_cast<UINT>('A' + (qtKey - Qt::Key_A));
    } else if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
        key = static_cast<UINT>('0' + (qtKey - Qt::Key_0));
    } else if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24) {
        key = VK_F1 + static_cast<UINT>(qtKey - Qt::Key_F1);
    } else {
        switch (qtKey) {
        case Qt::Key_Space: key = VK_SPACE; break;
        case Qt::Key_Tab: key = VK_TAB; break;
        case Qt::Key_Backspace: key = VK_BACK; break;
        case Qt::Key_Return:
        case Qt::Key_Enter: key = VK_RETURN; break;
        case Qt::Key_Escape: key = VK_ESCAPE; break;
        case Qt::Key_Insert: key = VK_INSERT; break;
        case Qt::Key_Delete: key = VK_DELETE; break;
        case Qt::Key_Home: key = VK_HOME; break;
        case Qt::Key_End: key = VK_END; break;
        case Qt::Key_PageUp: key = VK_PRIOR; break;
        case Qt::Key_PageDown: key = VK_NEXT; break;
        case Qt::Key_Left: key = VK_LEFT; break;
        case Qt::Key_Right: key = VK_RIGHT; break;
        case Qt::Key_Up: key = VK_UP; break;
        case Qt::Key_Down: key = VK_DOWN; break;
        default: {
            if (qtKey <= 0 || qtKey > 0xFFFF)
                return false;
            SHORT mapped = VkKeyScanW(static_cast<WCHAR>(qtKey));
            if (mapped == -1)
                return false;
            key = LOBYTE(mapped);
            break;
        }
        }
    }

    return key != 0;
}
#endif

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
                if (m_captureSuspended)
                    return true;
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
