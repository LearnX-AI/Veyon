/*
 * FocusModePlugin.cpp - server- and master-side logic for Smart Focus Mode.
 *
 * v1.1: subscribes via CentralPolicyHooks (in veyon-core) for live
 *       blocklist and focus-state updates. Falls back to the local
 *       blocklist file when no central server is configured.
 */

#include "BlocklistLoader.h"
#include "CentralPolicyHooks.h"
#include "ComputerControlInterface.h"
#include "FeatureMessage.h"
#include "FocusModePlugin.h"
#include "HostsFileManager.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"

#include <QPointer>


FocusModePlugin::FocusModePlugin( QObject* parent ) :
    QObject( parent ),
    m_focusModeFeature(
        QStringLiteral("FocusMode"),
        Feature::Flag::Mode | Feature::Flag::AllComponents,
        Feature::Uid{ QStringLiteral("e1f2a3b4-c5d6-7890-1234-56789abcdef0") },
        Feature::Uid{},
        tr("Focus Mode"),
        tr("Disable Focus Mode"),
        tr("Activates Smart Focus Mode on selected student computers, "
           "blocking distracting websites such as social media."),
        QStringLiteral(":/focusmode/focus.png") ),
    m_features( { m_focusModeFeature } ),
    m_focusActive( false )
{
    subscribeToCentralPolicy();
}


bool FocusModePlugin::controlFeature( Feature::Uid featureUid,
                                      Operation operation,
                                      const QVariantMap& /*arguments*/,
                                      const ComputerControlInterfaceList& computerControlInterfaces )
{
    if( featureUid != m_focusModeFeature.uid() )
    {
        return false;
    }

    if( operation == Operation::Start )
    {
        FeatureMessage msg{ m_focusModeFeature.uid(), EnableFocusMode };
        sendFeatureMessage( msg, computerControlInterfaces );
    }
    else if( operation == Operation::Stop )
    {
        FeatureMessage msg{ m_focusModeFeature.uid(), DisableFocusMode };
        sendFeatureMessage( msg, computerControlInterfaces );
    }
    return true;
}


bool FocusModePlugin::startFeature( VeyonMasterInterface& /*master*/,
                                    const Feature& feature,
                                    const ComputerControlInterfaceList& ccil )
{
    if( feature.uid() != m_focusModeFeature.uid() )
    {
        return false;
    }
    return controlFeature( feature.uid(), Operation::Start, {}, ccil );
}


bool FocusModePlugin::stopFeature( VeyonMasterInterface& /*master*/,
                                   const Feature& feature,
                                   const ComputerControlInterfaceList& ccil )
{
    if( feature.uid() != m_focusModeFeature.uid() )
    {
        return false;
    }
    return controlFeature( feature.uid(), Operation::Stop, {}, ccil );
}


bool FocusModePlugin::handleFeatureMessage( ComputerControlInterface::Pointer /*ccp*/,
                                            const FeatureMessage& /*message*/ )
{
    return false;
}


bool FocusModePlugin::handleFeatureMessage( VeyonServerInterface& /*server*/,
                                            const MessageContext& /*messageContext*/,
                                            const FeatureMessage& message )
{
    if( message.featureUid() != m_focusModeFeature.uid() )
    {
        return false;
    }

    if( message.command() == static_cast<FeatureMessage::Command>(EnableFocusMode) )
    {
        const QStringList domains = BlocklistLoader::load();
        if( domains.isEmpty() )
        {
            vWarning() << "FocusMode: blocklist is empty or unreadable - nothing to block";
            return true;
        }
        return applyOrClear( true, domains );
    }

    if( message.command() == static_cast<FeatureMessage::Command>(DisableFocusMode) )
    {
        return applyOrClear( false, {} );
    }

    return false;
}


// =============================================================
// CentralPolicy integration via core hooks
// =============================================================

bool FocusModePlugin::applyOrClear( bool enable, const QStringList& domains )
{
    HostsFileManager hostsMgr;

    if( enable )
    {
        if( hostsMgr.applyBlocklist( domains ) == false )
        {
            vCritical() << "FocusMode: failed to apply blocklist:" << hostsMgr.lastError();
            return false;
        }
        m_focusActive = true;
        vInfo() << "FocusMode: ENABLED. Blocked" << domains.size() << "domains.";
        return true;
    }

    if( hostsMgr.clearBlocklist() == false )
    {
        vCritical() << "FocusMode: failed to clear blocklist:" << hostsMgr.lastError();
        return false;
    }
    m_focusActive = false;
    vInfo() << "FocusMode: DISABLED.";
    return true;
}


void FocusModePlugin::subscribeToCentralPolicy()
{
    // QPointer makes the lambda safe even if 'this' is destroyed before
    // CentralPolicy notifies. The lambda no-ops in that case.
    QPointer<FocusModePlugin> self( this );

    CentralPolicyHooks::registerBlocklistHandler(
        [self]( const QStringList& domains, int version ) {
            if( self.isNull() )
            {
                return;
            }
            vInfo() << "FocusMode: received" << domains.size()
                    << "domains from CentralPolicy (version" << version << ")";

            // Persist locally for offline / reboot survival.
            BlocklistLoader::save( domains );

            // If focus is active, re-apply with the new list.
            if( self->m_focusActive )
            {
                self->applyOrClear( true, domains );
            }
        });

    CentralPolicyHooks::registerFocusStateHandler(
        [self]( bool enabled ) {
            if( self.isNull() )
            {
                return;
            }
            vInfo() << "FocusMode: CentralPolicy requests focus mode:" << enabled;

            if( enabled == self->m_focusActive )
            {
                return;
            }

            if( enabled )
            {
                const QStringList domains = BlocklistLoader::load();
                self->applyOrClear( true, domains );
            }
            else
            {
                self->applyOrClear( false, {} );
            }
        });

    vInfo() << "FocusMode: subscribed to CentralPolicy hooks.";
}
