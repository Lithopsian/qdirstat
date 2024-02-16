/*
 *   File name: DelayedRebuilder.cpp
 *   Summary:	Utility class to handle delayed rebuilding of widgets
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include <QTimer>

#include "DelayedRebuilder.h"
#include "Logger.h"


using namespace QDirStat;



void DelayedRebuilder::scheduleRebuild()
{
    ++_pendingRebuildCount;
    QTimer::singleShot( _delayMillisec, this, SLOT( rebuildDelayed() ) );
}


void DelayedRebuilder::rebuildDelayed()
{
    if ( --_pendingRebuildCount > 0 ) // Yet another rebuild scheduled (by timer)?
        return;                       // -> do nothing, it will be in vain anyway

    _pendingRebuildCount = 0;
    _firstRebuild       = false;
    emit rebuild();
}
