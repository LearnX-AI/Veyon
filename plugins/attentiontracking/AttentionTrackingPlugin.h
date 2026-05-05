/*
 * AttentionTrackingPlugin.h - plugin entry point for attention tracking
 *
 * This plugin provides real-time attention state tracking for student
 * machines, using the StudentAttentionStatus data model.
 *
 * Task 1: defines the data model (StudentAttentionStatus.h)
 * Task 2: implements the state transition engine (this file + .cpp)
 * Task 3: adds optimization and synchronization
 */

#pragma once

#include <QObject>

#include "PluginInterface.h"
#include "StudentAttentionStatus.h"


class AttentionTrackingPlugin : public QObject, PluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.AttentionTracking")
    Q_INTERFACES(PluginInterface)

public:
    explicit AttentionTrackingPlugin( QObject* parent = nullptr );
    ~AttentionTrackingPlugin() override = default;

    // ---- PluginInterface ----
    Plugin::Uid uid() const override
    {
        return Plugin::Uid{ QStringLiteral("b7e4f2a1-9c3d-4e5f-a1b2-c3d4e5f67890") };
    }

    QVersionNumber version() const override
    {
        return QVersionNumber( 1, 0, 0 );
    }

    QString name() const override
    {
        return QStringLiteral("AttentionTracking");
    }

    QString description() const override
    {
        return tr( "Real-time attention state tracking for student machines" );
    }

    QString vendor() const override
    {
        return QStringLiteral("PowerX Technologies");
    }

    QString copyright() const override
    {
        return QStringLiteral("Copyright 2026 PowerX Technologies");
    }

    // ---- Task 2 will add ----
    //  - startStateEngine()
    //  - stopStateEngine()
    //  - evaluateAttentionState() slot
    //  - StudentAttentionStatus currentStatus() getter
};
