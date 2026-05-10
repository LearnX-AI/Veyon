/*
 * CentralPolicyConfiguration.h - configuration properties for the sync agent.
 *
 * Uses the X-macro pattern: define a property once, get the typed
 * getter/setter/default for free via VeyonCore's config framework.
 */

#pragma once

#include "Configuration/Proxy.h"


#define FOREACH_CENTRAL_POLICY_CONFIG_PROPERTY(OP)                              \
    OP( CentralPolicyConfiguration, m_configuration,                            \
        STRING, serverUrl, setServerUrl,                                        \
        QStringLiteral("http://localhost:8000"),                                \
        QStringLiteral("CentralPolicy"), Configuration::Property::Flag::Standard ) \
    OP( CentralPolicyConfiguration, m_configuration,                            \
        STRING, adminToken, setAdminToken,                                      \
        QString(),                                                              \
        QStringLiteral("CentralPolicy"), Configuration::Property::Flag::Standard ) \
    OP( CentralPolicyConfiguration, m_configuration,                            \
        STRING, machineHostname, setMachineHostname,                            \
        QString(),                                                              \
        QStringLiteral("CentralPolicy"), Configuration::Property::Flag::Standard ) \
    OP( CentralPolicyConfiguration, m_configuration,                            \
        INT, heartbeatIntervalSeconds, setHeartbeatIntervalSeconds,             \
        30,                                                                     \
        QStringLiteral("CentralPolicy"), Configuration::Property::Flag::Standard )


DECLARE_CONFIG_PROXY( CentralPolicyConfiguration, FOREACH_CENTRAL_POLICY_CONFIG_PROPERTY )
