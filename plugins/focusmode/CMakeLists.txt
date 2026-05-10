include(BuildVeyonPlugin)

build_veyon_plugin(focusmode
    NAME FocusMode
    SOURCES
    FocusModePlugin.cpp
    FocusModePlugin.h
    BlocklistLoader.cpp
    BlocklistLoader.h
    HostsFileManager.cpp
    HostsFileManager.h
    focusmode.qrc
)

set_source_files_properties(FocusModePlugin.cpp HostsFileManager.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE SKIP_PRECOMPILE_HEADERS TRUE)


