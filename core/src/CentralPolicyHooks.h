/*
 * CentralPolicyHooks.h
 *
 * Thin "rendezvous point" in veyon-core that lets plugins communicate
 * about central-server policy state WITHOUT depending on each other
 * directly.
 *
 * Veyon plugins are MODULE libraries (loaded via dlopen), and CMake
 * disallows linking against MODULE targets. So plugin-to-plugin
 * communication must happen through code that lives in veyon-core
 * (which all plugins link against).
 *
 * Usage:
 *   - The CentralPolicy plugin calls notify*() when it detects state
 *     changes from the central server.
 *   - Other plugins (FocusMode, future Time Limiter, etc.) call
 *     register*Handler() at startup to subscribe.
 *
 * Thread safety:
 *   - Registration and notification must happen on the same thread.
 *     For Veyon plugins this is the main GUI thread - no extra
 *     synchronization needed.
 */

#pragma once

#include <QStringList>

#include <functional>

#include "VeyonCore.h"


namespace CentralPolicyHooks
{

using BlocklistHandler  = std::function<void(const QStringList& domains, int version)>;
using FocusStateHandler = std::function<void(bool enabled)>;


/// Register a callback to be invoked when the central server reports
/// a new blocklist version. Multiple handlers may be registered.
VEYON_CORE_EXPORT void registerBlocklistHandler( BlocklistHandler handler );

/// Register a callback to be invoked when the central server reports
/// a focus-mode state change.
VEYON_CORE_EXPORT void registerFocusStateHandler( FocusStateHandler handler );

/// Invoked by the CentralPolicy plugin when it pulls a new blocklist.
VEYON_CORE_EXPORT void notifyBlocklistChanged( const QStringList& domains, int version );

/// Invoked by the CentralPolicy plugin when the focus state flips.
VEYON_CORE_EXPORT void notifyFocusStateChanged( bool enabled );

}  // namespace CentralPolicyHooks
