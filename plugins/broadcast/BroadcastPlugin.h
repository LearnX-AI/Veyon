/*
 * BroadcastPlugin.h - Real-Time Teacher Broadcast Overlay plugin
 *
 * Registers three features with Veyon: BroadcastNormal, BroadcastUrgent,
 * BroadcastEmergency. Each appears as a separate toolbar button in Master.
 *
 * Architecture:
 *   - Master  side: shows input dialog, sends FeatureMessage
 *   - Server  side: validates, rate-limits, spawns BroadcastWorker
 *   - Worker  side: renders the overlay window in user session
 *
 * Security:
 *   - Uses Veyon's authenticated channel (no new sockets)
 *   - Plain text only (no HTML rendering)
 *   - Server-side rate limit + length validation
 *   - Emergency overlay is non-dismissable by student
 */

#pragma once

#include <QObject>

#include "BroadcastMessage.h"
#include "Feature.h"
#include "FeatureProviderInterface.h"
#include "PluginInterface.h"
#include "RateLimiter.h"


class BroadcastPlugin : public QObject,
                        PluginInterface,
                        FeatureProviderInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.Broadcast")
    Q_INTERFACES(PluginInterface FeatureProviderInterface)

public:
    explicit BroadcastPlugin( QObject* parent = nullptr );
    ~BroadcastPlugin() override = default;

    // ---- PluginInterface ----
    Plugin::Uid uid() const override
    {
        return Plugin::Uid{ QStringLiteral("d4f5a6b7-c8d9-4e0f-a1b2-c3d4e5f67891") };
    }

    QVersionNumber version() const override     { return QVersionNumber( 1, 0, 0 ); }
    QString name() const override               { return QStringLiteral("Broadcast"); }
    QString description() const override        { return tr("Real-time teacher broadcast overlay"); }
    QString vendor() const override             { return QStringLiteral("PowerX Technologies"); }
    QString copyright() const override          { return QStringLiteral("Copyright 2026 PowerX Technologies"); }

    // ---- FeatureProviderInterface ----
    const FeatureList& featureList() const override   { return m_features; }

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
    enum Argument
    {
        Level,
        Title,
        Body,
        Timestamp,
    };

    BroadcastLevel featureToLevel( const Feature::Uid& uid ) const;
    bool promptAndSend( BroadcastLevel level,
                        VeyonMasterInterface& master,
                        const ComputerControlInterfaceList& ccil );

    Feature m_normalFeature;
    Feature m_urgentFeature;
    Feature m_emergencyFeature;
    FeatureList m_features;

    RateLimiter m_rateLimiter;
};
