/*
 * AttentionStateEngine.h - state transition engine for student attention
 *
 * Runs a periodic timer, reads activity timestamps from an ActivitySource,
 * and emits state changes based on configurable thresholds.
 *
 * State transitions:
 *   Active -> Idle    after IdleThresholdSeconds   of no activity
 *   Idle   -> Away    after AwayThresholdSeconds   of no activity
 *   *      -> Active  on any new activity
 *
 * Usage:
 *   MockActivitySource src;
 *   AttentionStateEngine engine( &src );
 *   QObject::connect( &engine, &AttentionStateEngine::stateChanged,
 *                     [](AttentionState s){ ... } );
 *   engine.start();
 */

#pragma once

#include <QObject>
#include <QTimer>

#include "ActivitySource.h"
#include "StudentAttentionStatus.h"


class AttentionStateEngine : public QObject
{
    Q_OBJECT

public:
    explicit AttentionStateEngine( ActivitySource* source, QObject* parent = nullptr );

    /// Start the periodic evaluation timer.
    void start();

    /// Stop the periodic evaluation timer.
    void stop();

    /// Returns the current attention status snapshot.
    StudentAttentionStatus currentStatus() const;

    // ---- Configuration (defaults match the design doc) ----

    void setEvaluationIntervalMs( int ms )         { m_intervalMs = ms; }
    void setIdleThresholdSeconds( int seconds )    { m_idleThreshold = seconds; }
    void setAwayThresholdSeconds( int seconds )    { m_awayThreshold = seconds; }
    void setComputerId( const QUuid& id )          { m_computerId = id; }

    int  evaluationIntervalMs()  const  { return m_intervalMs; }
    int  idleThresholdSeconds()  const  { return m_idleThreshold; }
    int  awayThresholdSeconds()  const  { return m_awayThreshold; }

Q_SIGNALS:
    /// Emitted whenever the attention state changes.
    void stateChanged( AttentionState newState );

    /// Emitted on every evaluation tick (useful for UI updates).
    void statusUpdated( const StudentAttentionStatus& status );

private Q_SLOTS:
    void evaluate();

private:
    ActivitySource* m_source;
    QTimer m_timer;

    QUuid m_computerId;
    AttentionState m_currentState = AttentionState::Active;
    qint32 m_currentIdleSeconds = 0;

    // Defaults from design doc Section 6
    int m_intervalMs    = 2000;   // evaluate every 2 seconds
    int m_idleThreshold = 30;

    // Optimization metrics (Task 3)
    qint64 m_tickCount = 0;
    qint64 m_emitCount = 0;

public:
    qint64 tickCount() const { return m_tickCount; }
    qint64 emitCount() const { return m_emitCount; }
    double suppressionRatio() const { return m_tickCount ? 1.0 - (double)m_emitCount / m_tickCount : 0.0; }     // 30s -> Idle
    int m_awayThreshold = 180;    // 180s -> Away
};
