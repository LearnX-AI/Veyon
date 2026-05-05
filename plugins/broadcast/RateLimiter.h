/*
 * RateLimiter.h - simple rate limiting for broadcast messages
 *
 * Server-side defense: prevents a compromised or buggy Master from
 * flooding student screens with rapid-fire messages.
 *
 * Default: max 1 broadcast per 2000ms (2 seconds).
 * Emergency-level messages bypass the limit (if it's a real emergency,
 * the teacher should be able to send immediately).
 */

#pragma once

#include <QDateTime>
#include <QtGlobal>


class RateLimiter
{
public:
    explicit RateLimiter( qint64 minIntervalMs = 2000 ) :
        m_minIntervalMs( minIntervalMs ),
        m_lastAcceptedMs( 0 )
    {
    }

    /// @return true if the message is allowed; false if it should be rejected.
    /// On accept, internal clock is updated.
    bool tryAccept()
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if( now - m_lastAcceptedMs < m_minIntervalMs )
        {
            return false;
        }
        m_lastAcceptedMs = now;
        return true;
    }

    /// Reset the limiter (e.g., for emergency-bypass scenarios).
    void reset()
    {
        m_lastAcceptedMs = 0;
    }

private:
    qint64 m_minIntervalMs;
    qint64 m_lastAcceptedMs;
};
