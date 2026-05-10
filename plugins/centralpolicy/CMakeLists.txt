# CentralPolicy plugin - background sync agent.

build_veyon_plugin(centralpolicy
    CentralPolicyPlugin.cpp
    CentralPolicyPlugin.h
    CentralPolicyConfiguration.h
    HttpClient.cpp
    HttpClient.h
    SyncAgent.cpp
    SyncAgent.h
    PolicySubscriber.h
    centralpolicy.qrc
)

# This plugin uses Qt Network for HTTP. Other plugins do not need it,
# so we only link it here.
target_link_libraries(centralpolicy PRIVATE Qt${QT_MAJOR_VERSION}::Network)

# SyncAgent.cpp uses Q_SIGNALS / Q_SLOTS - same Unity-build quirk
# we hit with AttentionTracking. Exclude from unity to be safe.
set_source_files_properties(SyncAgent.cpp PROPERTIES
    SKIP_UNITY_BUILD_INCLUSION TRUE
    SKIP_PRECOMPILE_HEADERS TRUE
)
