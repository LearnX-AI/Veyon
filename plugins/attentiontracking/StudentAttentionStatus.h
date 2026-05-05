
/*

 * StudentAttentionStatus.h - data model for student attention tracking

 *

 * This file defines the shared data structure used to represent the

 * real-time attention status of each student computer in the system.

 *

 * See: StudentAttentionStatus_Design.docx for the full specification.

 */

#pragma once

#include <QJsonObject>

#include <QString>

#include <QUuid>

#include <QDateTime>

/**

 * @brief Enumeration of possible attention states for a student machine.

 *

 * These values map 1:1 to the states defined in the design document.

 * The string representations (used on the wire) are case-sensitive.

 */

enum class AttentionState

{

    Active,  ///< User is actively interacting (within IdleThresholdSeconds)

    Idle,    ///< No input for IdleThresholdSeconds but less than AwayThresholdSeconds

    Away     ///< No input for AwayThresholdSeconds or more

};

/**

 * @brief Data model representing a student computer's attention status.

 *

 * This is the single source of truth for student activity across the system.

 * It is produced by the server (state engine) and consumed by the master UI.

 *

 * Wire format: JSON (see toJson() / fromJson()).

 */

struct StudentAttentionStatus

{

    /// Unique identifier of the student computer.

    /// Reuses Veyon's NetworkObject UUID from the computer directory.

    QUuid computerId;

    /// Current attention state of the student machine.

    AttentionState state = AttentionState::Active;

    /// Timestamp of the most recent detected user activity,

    /// in milliseconds since Unix epoch (UTC).

    /// Populated by: client (via QDateTime::currentMSecsSinceEpoch()).

    qint64 lastActivityTimestamp = 0;

    /// Number of seconds elapsed since lastActivityTimestamp.

    /// Populated by: server (derived at emit time).

    qint32 idleDurationSeconds = 0;

    // ---- State <-> string conversion (for JSON wire format) ----

    static QString stateToString( AttentionState s )

    {

        switch( s )

        {

            case AttentionState::Active: return QStringLiteral("Active");

            case AttentionState::Idle:   return QStringLiteral("Idle");

            case AttentionState::Away:   return QStringLiteral("Away");

        }

        return QStringLiteral("Active");

    }

    static AttentionState stateFromString( const QString& s )

    {

        if( s == QStringLiteral("Idle") ) return AttentionState::Idle;

        if( s == QStringLiteral("Away") ) return AttentionState::Away;

        return AttentionState::Active;

    }

    // ---- Serialization ----

    QJsonObject toJson() const

    {

        QJsonObject obj;

        obj[QStringLiteral("computerId")] = computerId.toString( QUuid::WithoutBraces );

        obj[QStringLiteral("state")] = stateToString( state );

        obj[QStringLiteral("lastActivityTimestamp")] = lastActivityTimestamp;

        obj[QStringLiteral("idleDurationSeconds")] = idleDurationSeconds;

        return obj;

    }

    static StudentAttentionStatus fromJson( const QJsonObject& obj )

    {

        StudentAttentionStatus s;

        s.computerId            = QUuid::fromString( obj[QStringLiteral("computerId")].toString() );

        s.state                 = stateFromString( obj[QStringLiteral("state")].toString() );

        s.lastActivityTimestamp = obj[QStringLiteral("lastActivityTimestamp")].toVariant().toLongLong();

        s.idleDurationSeconds   = obj[QStringLiteral("idleDurationSeconds")].toInt();

        return s;

    }

    // ---- Validation (per Section 7 of the design doc) ----

    bool isValid() const

    {

        if( computerId.isNull() )                                           return false;

        if( lastActivityTimestamp <= 0 )                                    return false;

        if( lastActivityTimestamp > QDateTime::currentMSecsSinceEpoch() )   return false;

        if( idleDurationSeconds < 0 )                                       return false;

        return true;

    }

};

