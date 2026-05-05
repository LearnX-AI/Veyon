/*
 * BroadcastPlugin.cpp - implementation of the broadcast overlay feature
 */

#include <QDateTime>
#include <QInputDialog>
#include <QProcess>

#include "BroadcastPlugin.h"
#include "ComputerControlInterface.h"
#include "FeatureMessage.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"


BroadcastPlugin::BroadcastPlugin( QObject* parent ) :
    QObject( parent ),
    m_normalFeature(
        QStringLiteral("BroadcastNormal"),
        Feature::Flag::Action | Feature::Flag::AllComponents,
        Feature::Uid{ QStringLiteral("a1b2c3d4-e5f6-7890-1234-567890abcde1") },
        Feature::Uid{},
        tr("Announcement"),
        {},
        tr("Send a normal announcement to selected students."),
        QStringLiteral(":/broadcast/announcement.png") ),
    m_urgentFeature(
        QStringLiteral("BroadcastUrgent"),
        Feature::Flag::Action | Feature::Flag::AllComponents,
        Feature::Uid{ QStringLiteral("a1b2c3d4-e5f6-7890-1234-567890abcde2") },
        Feature::Uid{},
        tr("Urgent message"),
        {},
        tr("Send an urgent full-screen message that students must acknowledge."),
        QStringLiteral(":/broadcast/urgent.png") ),
    m_emergencyFeature(
        QStringLiteral("BroadcastEmergency"),
        Feature::Flag::Action | Feature::Flag::AllComponents,
        Feature::Uid{ QStringLiteral("a1b2c3d4-e5f6-7890-1234-567890abcde3") },
        Feature::Uid{},
        tr("Emergency broadcast"),
        {},
        tr("Send a non-dismissible emergency overlay (e.g. fire drill, evacuation)."),
        QStringLiteral(":/broadcast/emergency.png") ),
    m_features( { m_normalFeature, m_urgentFeature, m_emergencyFeature } ),
    m_rateLimiter( 2000 )   // max 1 broadcast per 2 seconds
{
}


BroadcastLevel BroadcastPlugin::featureToLevel( const Feature::Uid& uid ) const
{
    if( uid == m_emergencyFeature.uid() ) return BroadcastLevel::Emergency;
    if( uid == m_urgentFeature.uid() )    return BroadcastLevel::Urgent;
    return BroadcastLevel::Normal;
}


bool BroadcastPlugin::promptAndSend( BroadcastLevel level,
                                     VeyonMasterInterface& /*master*/,
                                     const ComputerControlInterfaceList& ccil )
{
    bool ok = false;
    const QString prompt = tr("Enter message (max %1 chars):").arg( BroadcastMessage::MaxBodyLength );
    const QString text = QInputDialog::getMultiLineText( nullptr,
                                                         tr("Broadcast — %1")
                                                             .arg( BroadcastMessage::levelToString( level ) ),
                                                         prompt,
                                                         {},
                                                         &ok );
    if( ok == false || text.trimmed().isEmpty() )
    {
        return false;
    }

    QString safeBody = text;
    if( safeBody.length() > BroadcastMessage::MaxBodyLength )
    {
        safeBody.truncate( BroadcastMessage::MaxBodyLength );
    }

    Feature::Uid featureUid;
    switch( level )
    {
        case BroadcastLevel::Normal:    featureUid = m_normalFeature.uid();    break;
        case BroadcastLevel::Urgent:    featureUid = m_urgentFeature.uid();    break;
        case BroadcastLevel::Emergency: featureUid = m_emergencyFeature.uid(); break;
    }

    FeatureMessage msg{ featureUid };
    msg.addArgument( Level,     static_cast<int>( level ) );
    msg.addArgument( Title,     QString() );
    msg.addArgument( Body,      safeBody );
    msg.addArgument( Timestamp, QDateTime::currentMSecsSinceEpoch() );

    sendFeatureMessage( msg, ccil );
    return true;
}


bool BroadcastPlugin::controlFeature( Feature::Uid /*featureUid*/,
                                      Operation /*operation*/,
                                      const QVariantMap& /*arguments*/,
                                      const ComputerControlInterfaceList& /*ccil*/ )
{
    // Broadcast features are interactive (require dialog input) — not used
    // via controlFeature in this plugin.
    return false;
}


bool BroadcastPlugin::startFeature( VeyonMasterInterface& master,
                                    const Feature& feature,
                                    const ComputerControlInterfaceList& ccil )
{
    return promptAndSend( featureToLevel( feature.uid() ), master, ccil );
}


bool BroadcastPlugin::stopFeature( VeyonMasterInterface& /*master*/,
                                   const Feature& /*feature*/,
                                   const ComputerControlInterfaceList& /*ccil*/ )
{
    // Broadcasts are fire-and-forget; no stop semantics.
    return false;
}


bool BroadcastPlugin::handleFeatureMessage( ComputerControlInterface::Pointer /*ccp*/,
                                            const FeatureMessage& /*message*/ )
{
    return false;   // master side does not receive replies
}


// SERVER-SIDE: receives the broadcast and spawns the worker overlay.
bool BroadcastPlugin::handleFeatureMessage( VeyonServerInterface& /*server*/,
                                            const MessageContext& /*messageContext*/,
                                            const FeatureMessage& message )
{
    // Identify whether this message is for any of our features
    if( message.featureUid() != m_normalFeature.uid() &&
        message.featureUid() != m_urgentFeature.uid() &&
        message.featureUid() != m_emergencyFeature.uid() )
    {
        return false;
    }

    // Rebuild and validate the BroadcastMessage
    BroadcastMessage bm;
    bm.level     = static_cast<BroadcastLevel>( message.argument( Level ).toInt() );
    bm.title     = message.argument( Title ).toString();
    bm.body      = message.argument( Body ).toString();
    bm.timestamp = message.argument( Timestamp ).toLongLong();
    bm.enforceLimits();

    if( bm.isValid() == false )
    {
        vWarning() << "Broadcast: rejected invalid message";
        return true;
    }

    // Rate limit (Emergency bypasses the limiter)
    if( bm.level != BroadcastLevel::Emergency && m_rateLimiter.tryAccept() == false )
    {
        vWarning() << "Broadcast: rate-limited; dropping message";
        return true;
    }

    // Spawn worker process to render the overlay in the user session.
    // For now we just log — the worker process will be added in the next step.
    vInfo() << "Broadcast received:"
            << "level=" << BroadcastMessage::levelToString( bm.level )
            << "body=" << bm.body.left( 60 )
            << ( bm.body.length() > 60 ? QStringLiteral("...") : QString() );

    // Launch the worker process to render the overlay in the user session.
    // Body is base64-encoded to safely pass through command-line arguments.
    const QStringList args = {
        QStringLiteral("--level"), BroadcastMessage::levelToString( bm.level ),
        QStringLiteral("--title"), QString::fromLatin1( bm.title.toUtf8().toBase64() ),
        QStringLiteral("--body"),  QString::fromLatin1( bm.body.toUtf8().toBase64() ),
    };

    if( QProcess::startDetached( QStringLiteral("veyon-broadcast-worker"), args ) == false )
    {
        vWarning() << "Broadcast: failed to launch veyon-broadcast-worker";
    }

    return true;
}
