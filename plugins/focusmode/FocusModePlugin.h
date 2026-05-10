/*
 * FocusModePlugin.h - "Smart Focus Mode" feature plugin for Veyon
 *
 * v1.1: subscribes to CentralPolicyHooks (in veyon-core) for live
 *       blocklist and focus-state updates from the central server.
 *       Falls back to the local blocklist file if no central server
 *       is configured.
 */

#pragma once

#include <QObject>
#include <QStringList>

#include "Feature.h"
#include "FeatureProviderInterface.h"
#include "PluginInterface.h"


class FocusModePlugin : public QObject,
                        PluginInterface,
                        FeatureProviderInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.FocusMode")
    Q_INTERFACES(PluginInterface FeatureProviderInterface)

public:
    explicit FocusModePlugin( QObject* parent = nullptr );
    ~FocusModePlugin() override = default;

    // ---- PluginInterface ----
    Plugin::Uid uid() const override
    {
        return Plugin::Uid{ QStringLiteral("c8e2f3a4-9d1b-4e5c-b6a7-d8e9f0123456") };
    }

    QVersionNumber version() const override            { return QVersionNumber( 1, 1, 0 ); }
    QString name() const override                      { return QStringLiteral("FocusMode"); }
    QString description() const override               { return tr( "Smart Focus Mode — application and website restriction" ); }
    QString vendor() const override                    { return QStringLiteral("PowerX Technologies"); }
    QString copyright() const override                 { return QStringLiteral("Copyright 2026 PowerX Technologies"); }

    // ---- FeatureProviderInterface ----
    const FeatureList& featureList() const override    { return m_features; }

    bool controlFeature( Feature::Uid featureUid,
                         Operation operation,
                         const QVariantMap& arguments,
                         const ComputerControlInterfaceList& computerControlInterfaces ) override;

    bool startFeature( VeyonMasterInterface& master,
                       const Feature& feature,
                       const ComputerControlInterfaceList& computerControlInterfaces ) override;

    bool stopFeature( VeyonMasterInterface& master,
                      const Feature& feature,
                      const ComputerControlInterfaceList& computerControlInterfaces ) override;

    bool handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
                               const FeatureMessage& message ) override;

    bool handleFeatureMessage( VeyonServerInterface& server,
                               const MessageContext& messageContext,
                               const FeatureMessage& message ) override;

private:
    enum Commands
    {
        EnableFocusMode,
        DisableFocusMode,
    };

    /// Apply or clear the blocklist on /etc/hosts.
    bool applyOrClear( bool enable, const QStringList& domains );

    /// Subscribe to CentralPolicy callbacks via the core registry.
    void subscribeToCentralPolicy();

    Feature m_focusModeFeature;
    FeatureList m_features;

    bool m_focusActive;
};
