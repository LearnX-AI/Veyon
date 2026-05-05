/*
 * AttentionStateEngine.cpp - state transition engine implementation
 */

#include <QDateTime>

#include "AttentionStateEngine.h"


AttentionStateEngine::AttentionStateEngine( ActivitySource* source, QObject* parent ) :
    QObject( parent ),
    m_source( source )
{
    connect( &m_timer, &QTimer::timeout, this, &AttentionStateEngine::evaluate );
}


void AttentionStateEngine::start()
{
    m_timer.start( m_intervalMs );
    evaluate();   // immediate first evaluation
}


void AttentionStateEngine::stop()
{
    m_timer.stop();
}


StudentAttentionStatus AttentionStateEngine::currentStatus() const
{
    StudentAttentionStatus s;
    s.computerId            = m_computerId;
    s.state                 = m_currentState;
    s.lastActivityTimestamp = m_source ? m_source->lastActivityTime() : 0;
    s.idleDurationSeconds   = m_currentIdleSeconds;
    return s;
}


void AttentionStateEngine::evaluate()
{
    ++m_tickCount;

    if( m_source == nullptr )
    {
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 last = m_source->lastActivityTime();

    // Defensive: if timestamp is in the future or invalid, treat as just-now.
    const qint64 idleMs = ( last > 0 && last <= now ) ? ( now - last ) : 0;
    const qint32 idleSeconds = static_cast<qint32>( idleMs / 1000 );

    // ---- State transition rules (from design doc Section 4) ----
    AttentionState newState;
    if( idleSeconds >= m_awayThreshold )
    {
        newState = AttentionState::Away;
    }
    else if( idleSeconds >= m_idleThreshold )
    {
        newState = AttentionState::Idle;
    }
    else
    {
        newState = AttentionState::Active;
    }

    m_currentIdleSeconds = idleSeconds;

    if( newState != m_currentState )
    {
        m_currentState = newState;
        Q_EMIT stateChanged( newState );
        Q_EMIT statusUpdated( currentStatus() );
        ++m_emitCount;
    }

}
