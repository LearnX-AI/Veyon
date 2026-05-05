include(BuildVeyonPlugin)

build_veyon_plugin(attentiontracking
    NAME AttentionTracking
    SOURCES
    AttentionTrackingPlugin.cpp
    AttentionTrackingPlugin.h
    StudentAttentionStatus.h
    ActivitySource.h
    MockActivitySource.h
    AttentionStateEngine.cpp
    AttentionStateEngine.h
)

set_source_files_properties(AttentionStateEngine.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE SKIP_PRECOMPILE_HEADERS TRUE)
