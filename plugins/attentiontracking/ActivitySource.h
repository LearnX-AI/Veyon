/*
 * ActivitySource.h - abstract interface for activity sources
 *
 * The state engine consumes timestamps via this interface. Concrete
 * implementations include:
 *   - MockActivitySource (for testing)
 *   - ClientActivityTracker (Sihas's real tracker, Task 2 dependency)
 *
 * This decoupling allows the state engine to be developed and tested
 * independently of the input-tracking implementation.
 */

#pragma once

#include <QtGlobal>


class ActivitySource
{
public:
    virtual ~ActivitySource() = default;

    /**
     * @brief Returns the timestamp of the most recent user activity.
     * @return Milliseconds since Unix epoch (UTC).
     *
     * Contract:
     *  - Must return a positive value once at least one activity has occurred.
     *  - Must never return a value in the future.
     *  - Must be safe to call from any thread.
     */
    virtual qint64 lastActivityTime() const = 0;
};
