/*
 * FocusModePlugin.cpp - server- and master-side logic for Smart Focus Mode
 */

#include "BlocklistLoader.h"
#include "ComputerControlInterface.h"
#include "FeatureMessage.h"
#include "FocusModePlugin.h"
#include "HostsFileManager.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"


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
    m_features( { m_focusModeFeature } )
{
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


// Master-side incoming message handler (we don't expect any from server here)
bool FocusModePlugin::handleFeatureMessage( ComputerControlInterface::Pointer /*ccp*/,
                                            const FeatureMessage& /*message*/ )
{
    return false;
}


// SERVER-SIDE handler: this is what runs on the student machine.
bool FocusModePlugin::handleFeatureMessage( VeyonServerInterface& /*server*/,
                                            const MessageContext& /*messageContext*/,
                                            const FeatureMessage& message )
{
    if( message.featureUid() != m_focusModeFeature.uid() )
    {
        return false;
    }

    HostsFileManager hostsMgr;

    if( message.command() == static_cast<FeatureMessage::Command>(EnableFocusMode) )
    {
        const QStringList domains = BlocklistLoader::load();
        if( domains.isEmpty() )
        {
            vWarning() << "FocusMode: blocklist is empty or unreadable - nothing to block";
            return true;
        }

        if( hostsMgr.applyBlocklist( domains ) == false )
        {
            vCritical() << "FocusMode: failed to apply blocklist:" << hostsMgr.lastError();
            return false;
        }

        vInfo() << "FocusMode: ENABLED. Blocked" << domains.size() << "domains.";
        return true;
    }

    if( message.command() == static_cast<FeatureMessage::Command>(DisableFocusMode) )
    {
        if( hostsMgr.clearBlocklist() == false )
        {
            vCritical() << "FocusMode: failed to clear blocklist:" << hostsMgr.lastError();
            return false;
        }

        vInfo() << "FocusMode: DISABLED.";
        return true;
    }

    return false;
}
