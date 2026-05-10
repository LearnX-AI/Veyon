/*
 * CentralPolicyPlugin.h - Background policy sync agent (headless plugin)
 *
 * Connects each student PC to the central control server. Polls for
 * policy changes (blocklist updates, focus mode state, etc.) and
 * notifies subscribed plugins (e.g. FocusMode) when they need to react.
 *
 * No toolbar button, no GUI. Pure background service.
 */

#pragma once

#include <QObject>

#include "PluginInterface.h"


class SyncAgent;


class CentralPolicyPlugin : public QObject, PluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.CentralPolicy")
    Q_INTERFACES(PluginInterface)

public:
    explicit CentralPolicyPlugin( QObject* parent = nullptr );
    ~CentralPolicyPlugin() override;

    // ---- PluginInterface ----
    Plugin::Uid uid() const override
    {
        return Plugin::Uid{ QStringLiteral("e9f0a1b2-c3d4-5e6f-7890-1234567890ab") };
    }

    QVersionNumber version() const override     { return QVersionNumber( 1, 0, 0 ); }
    QString name() const override               { return QStringLiteral("CentralPolicy"); }
    QString description() const override        { return tr("Central policy sync agent (background service)"); }
    QString vendor() const override             { return QStringLiteral("PowerX Technologies"); }
    QString copyright() const override          { return QStringLiteral("Copyright 2026 PowerX Technologies"); }

    /// Singleton-style accessor (set by constructor) so other plugins can
    /// reach the sync agent. Returns nullptr if the plugin isn't loaded.
    static CentralPolicyPlugin* instance();

    SyncAgent* syncAgent() const { return m_syncAgent; }

private:
    static CentralPolicyPlugin* s_instance;
    SyncAgent* m_syncAgent;
};
