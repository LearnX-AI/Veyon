/*
 * PolicySubscriber.h - interface for plugins that want policy updates.
 *
 * Plugins implement this interface and connect to SyncAgent's signals
 * to react when the central server pushes new state.
 *
 * Example usage (in another plugin's constructor):
 *
 *     auto* cp = CentralPolicyPlugin::instance();
 *     if( cp != nullptr )
 *     {
 *         connect( cp->syncAgent(), &SyncAgent::blocklistChanged,
 *                  this, &MyPlugin::onBlocklistChanged );
 *     }
 *
 * This file is currently a documentation placeholder. Future versions
 * may turn it into a Q_INTERFACES contract if we need to enforce it.
 */

#pragma once

class PolicySubscriber
{
public:
    virtual ~PolicySubscriber() = default;

    /// Called when the server reports a new blocklist version.
    virtual void onBlocklistVersionChanged( int newVersion ) = 0;

    /// Called when the server reports a Focus Mode state change.
    virtual void onFocusModeStateChanged( bool enabled ) = 0;
};
