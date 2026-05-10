/*
 * main.cpp - Veyon Policy Agent entry point.
 *
 * Standalone systemd service. Polls the central control server for
 * blocklist changes and applies them to /etc/hosts. Independent of
 * Veyon Service; runs on any modern Linux (Wayland or X11).
 */

#include <QCoreApplication>
#include <QtDebug>

#include <csignal>

#include "PolicyAgent.h"


namespace
{
    QCoreApplication* g_app = nullptr;

    void shutdown( int signum )
    {
        qInfo().noquote() << "Received signal" << signum << "- shutting down.";
        if( g_app != nullptr )
        {
            g_app->quit();
        }
    }
}


int main( int argc, char** argv )
{
    QCoreApplication app( argc, argv );
    g_app = &app;

    // Graceful shutdown on SIGTERM (systemd stop) and SIGINT (Ctrl-C)
    std::signal( SIGTERM, shutdown );
    std::signal( SIGINT,  shutdown );

    qInfo().noquote() << "Veyon Policy Agent v0.1 starting.";

    PolicyAgent agent;
    if( !agent.start() )
    {
        return 1;
    }

    const int rc = app.exec();
    agent.stop();
    return rc;
}
