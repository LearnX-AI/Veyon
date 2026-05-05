/*
 * BroadcastMessage.h - data model for teacher broadcast messages
 *
 * This is the wire format sent from teacher (Master) to student (Server)
 * via Veyon's authenticated FeatureMessage channel.
 *
 * Security notes:
 *   - All text is rendered as plain text only (never HTML)
 *   - Title and body are length-limited at the server side
 *   - Level controls UI behavior (popup vs. fullscreen vs. locked)
 */

#pragma once

#include <QString>


enum class BroadcastLevel
{
    Normal    = 0,   ///< Small popup, top-right corner. Auto-dismiss after 10s.
    Urgent    = 1,   ///< Full-screen overlay. Student can acknowledge.
    Emergency = 2    ///< Full-screen red overlay. Cannot be dismissed by student.
};


struct BroadcastMessage
{
    static constexpr int MaxTitleLength = 100;
    static constexpr int MaxBodyLength  = 500;

    BroadcastLevel level = BroadcastLevel::Normal;
    QString        title;
    QString        body;
    qint64         timestamp = 0;

    // ---- Level <-> string conversion ----

    static QString levelToString( BroadcastLevel l )
    {
        switch( l )
        {
            case BroadcastLevel::Normal:    return QStringLiteral("Normal");
            case BroadcastLevel::Urgent:    return QStringLiteral("Urgent");
            case BroadcastLevel::Emergency: return QStringLiteral("Emergency");
        }
        return QStringLiteral("Normal");
    }

    static BroadcastLevel levelFromString( const QString& s )
    {
        if( s == QStringLiteral("Urgent") )    return BroadcastLevel::Urgent;
        if( s == QStringLiteral("Emergency") ) return BroadcastLevel::Emergency;
        return BroadcastLevel::Normal;
    }

    // ---- Validation (called server-side before rendering) ----

    bool isValid() const
    {
        if( body.isEmpty() )                     return false;
        if( body.length()  > MaxBodyLength )     return false;
        if( title.length() > MaxTitleLength )    return false;
        if( timestamp <= 0 )                     return false;
        return true;
    }

    /// Truncate fields to safe lengths (defensive — for messages from the wire).
    void enforceLimits()
    {
        if( title.length() > MaxTitleLength ) title.truncate( MaxTitleLength );
        if( body.length()  > MaxBodyLength )  body.truncate( MaxBodyLength );
    }
};
