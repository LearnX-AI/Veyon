include(BuildVeyonPlugin)

build_veyon_plugin(broadcast
    NAME Broadcast
    SOURCES
    BroadcastPlugin.cpp
    BroadcastPlugin.h
    BroadcastMessage.h
    RateLimiter.h
    broadcast.qrc
)

add_subdirectory(worker)
