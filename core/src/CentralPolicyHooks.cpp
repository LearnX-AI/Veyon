/*
 * CentralPolicyHooks.cpp
 */

#include "CentralPolicyHooks.h"

#include <QList>


namespace CentralPolicyHooks
{

namespace
{
    QList<BlocklistHandler>&  blocklistHandlers()
    {
        static QList<BlocklistHandler> handlers;
        return handlers;
    }

    QList<FocusStateHandler>& focusHandlers()
    {
        static QList<FocusStateHandler> handlers;
        return handlers;
    }
}


void registerBlocklistHandler( BlocklistHandler handler )
{
    blocklistHandlers().append( std::move( handler ) );
}


void registerFocusStateHandler( FocusStateHandler handler )
{
    focusHandlers().append( std::move( handler ) );
}


void notifyBlocklistChanged( const QStringList& domains, int version )
{
    for( const auto& h : blocklistHandlers() )
    {
        h( domains, version );
    }
}


void notifyFocusStateChanged( bool enabled )
{
    for( const auto& h : focusHandlers() )
    {
        h( enabled );
    }
}

}  // namespace CentralPolicyHooks
