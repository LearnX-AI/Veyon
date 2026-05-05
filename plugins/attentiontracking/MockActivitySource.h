
/*

 * MockActivitySource.h - test/mock implementation of ActivitySource

 *

 * Lets us simulate user activity manually. Used for development and

 * testing of the state engine before the real ClientActivityTracker

 

 *

 * Usage:

 *   MockActivitySource mock;

 *   mock.simulateActivityNow();        // marks "user just did something"

 *   mock.setLastActivityTime( ts );    // set to specific timestamp

 */

#pragma once

#include <QDateTime>

#include <QtGlobal>

#include "ActivitySource.h"

class MockActivitySource : public ActivitySource

{

public:

    MockActivitySource() :

        m_lastActivityTime( QDateTime::currentMSecsSinceEpoch() )

    {

    }

    qint64 lastActivityTime() const override

    {

        return m_lastActivityTime;

    }

    /// Marks "user just did something" using current system time.

    void simulateActivityNow()

    {

        m_lastActivityTime = QDateTime::currentMSecsSinceEpoch();

    }

    /// Set last activity to a specific timestamp (for testing transitions).

    void setLastActivityTime( qint64 ts )

    {

        m_lastActivityTime = ts;

    }

private:

    qint64 m_lastActivityTime;

};

