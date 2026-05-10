/*
 * CentralPolicyPlugin.cpp - sync agent plugin entry point.
 */

#include "CentralPolicyPlugin.h"
#include "SyncAgent.h"


CentralPolicyPlugin* CentralPolicyPlugin::s_instance = nullptr;


CentralPolicyPlugin::CentralPolicyPlugin( QObject* parent ) :
    QObject( parent ),
    m_syncAgent( new SyncAgent( this ) )
{
    s_instance = this;

    // SyncAgent::start() is non-blocking. It will register with the server
    // and begin the heartbeat loop on its own.
    m_syncAgent->start();
}


CentralPolicyPlugin::~CentralPolicyPlugin()
{
    if( m_syncAgent != nullptr )
    {
        m_syncAgent->stop();
    }
    s_instance = nullptr;
}


CentralPolicyPlugin* CentralPolicyPlugin::instance()
{
    return s_instance;
}
