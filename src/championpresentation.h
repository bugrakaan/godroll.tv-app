#ifndef CHAMPIONPRESENTATION_H
#define CHAMPIONPRESENTATION_H

#include <QString>
#include <QVariantMap>

namespace ChampionPresentation {

struct Definition
{
    QString type;
    QString label;
    QString displayName;
    QString icon;
    QString description;
    QString slug;
    int order = 3;

    bool isValid() const { return !type.isEmpty(); }
};

inline Definition forType(const QString &type)
{
    if (type == QStringLiteral("barrier")) {
        return {
            QStringLiteral("barrier"),
            QStringLiteral("Barrier"),
            QStringLiteral("Anti-Barrier Champion"),
            QStringLiteral("qrc:/qt/qml/GodrollLauncher/resources/champion-barrier.svg"),
            QStringLiteral("Dealing damage with this weapon pierces the shields of Barrier Champions."),
            QStringLiteral("anti-barrier-champion"),
            0
        };
    }
    if (type == QStringLiteral("overload")) {
        return {
            QStringLiteral("overload"),
            QStringLiteral("Overload"),
            QStringLiteral("Anti-Overload Champion"),
            QStringLiteral("qrc:/qt/qml/GodrollLauncher/resources/champion-overload.svg"),
            QStringLiteral("Dealing damage with this weapon disrupts and stuns Overload Champions."),
            QStringLiteral("anti-overload-champion"),
            1
        };
    }
    if (type == QStringLiteral("unstoppable")) {
        return {
            QStringLiteral("unstoppable"),
            QStringLiteral("Unstoppable"),
            QStringLiteral("Anti-Unstoppable Champion"),
            QStringLiteral("qrc:/qt/qml/GodrollLauncher/resources/champion-unstoppable.svg"),
            QStringLiteral("Dealing damage with this weapon staggers and stuns Unstoppable Champions."),
            QStringLiteral("anti-unstoppable-champion"),
            2
        };
    }

    return {};
}

inline Definition forFilterPrefix(const QString &prefix)
{
    const QString normalized = prefix.trimmed().toLower();
    if (normalized.isEmpty())
        return {};

    // The supported values have distinct first letters, so resolve as soon as
    // the complete token after -b is captured instead of waiting for the full
    // type name to be typed.
    for (const QString &type : {QStringLiteral("barrier"),
                                QStringLiteral("overload"),
                                QStringLiteral("unstoppable")}) {
        if (type.startsWith(normalized))
            return forType(type);
    }

    return {};
}

inline QVariantMap toVariantMap(const Definition &definition)
{
    if (!definition.isValid())
        return {};

    return {
        {QStringLiteral("type"), definition.type},
        {QStringLiteral("label"), definition.label},
        {QStringLiteral("displayName"), definition.displayName},
        {QStringLiteral("icon"), definition.icon},
        {QStringLiteral("description"), definition.description},
        {QStringLiteral("slug"), definition.slug},
        {QStringLiteral("order"), definition.order}
    };
}

inline int orderForType(const QString &type)
{
    return forType(type).order;
}

} // namespace ChampionPresentation

#endif // CHAMPIONPRESENTATION_H
