/*
 *   File name: AdaptiveTimer.h
 *   Summary:	Support classes for QDirStat
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include "AdaptiveTimer.h"
#include "Logger.h"
#include "Exception.h"


#define VERBOSE_DELAY           0


using namespace QDirStat;


AdaptiveTimer::AdaptiveTimer( QObject * parent, QList<float> delays, QList<int> cooldowns ):
    QObject ( parent ),
    _payloadTime { 0 },
    _delayStage { 0 },
    _delays { delays },
    _cooldowns { cooldowns },
    _defaultDelay { 0 }
{
    init();
}

/*
AdaptiveTimer::AdaptiveTimer( QObject * parent, int defaultDelay ):
    QObject ( parent ),
    _payloadTime { 0 },
    _delayStage { 0 },
    _defaultDelay { defaultDelay }
{
    init();
}
*/

void AdaptiveTimer::init()
{
    connect( &_deliveryTimer, &QTimer::timeout,
             this,            &AdaptiveTimer::deliveryTimeout );

    connect( &_cooldownTimer, &QTimer::timeout,
             this,            &AdaptiveTimer::decreaseDelay );

    _deliveryTimer.setSingleShot( true );
    _cooldownTimer.setSingleShot( true );
}


void AdaptiveTimer::request( Payload payload )
{
    //logDebug() << "Received request" << Qt::endl;
    _payload = payload;

    if ( _cooldownTimer.isActive() )
        increaseDelay();

    _deliveryTimer.start( currentDelay() );
}


void AdaptiveTimer::deliveryTimeout()
{
    //logDebug() << "Delivering request" << Qt::endl;

    _payloadStopwatch.start();
    _payload();
    _payloadTime = _payloadStopwatch.elapsed();

    //logDebug() << "deliveryTime=" << _deliveryTime << Qt::endl;

//    deliveryComplete();
//    emit deliverRequest( _payload );

    _cooldownTimer.start( cooldownPeriod() );
}


/*
void AdaptiveTimer::deliveryComplete()
{
    _deliveryTime = _deliveryStopwatch.elapsed();
    // logDebug() << "deliveryTime=" << _deliveryTime << Qt::endl;

    _deliveryTimer.setInterval( _deliveryTime * _delays[ _delayStage ] );

    _cooldownTimer.start();
}
*/

int AdaptiveTimer::currentDelay() const
{
    if ( _delays.isEmpty() )
        return _defaultDelay;

    return _payloadTime * _delays[ _delayStage ];
}


int AdaptiveTimer::cooldownPeriod() const
{
    if ( _cooldowns.isEmpty() )
        return 0;

    // Might be more delays than cooldowns
    const int cooldownStage = qMin( _cooldowns.size() - 1, _delayStage );
    return _cooldowns[ cooldownStage ];
}


void AdaptiveTimer::increaseDelay()
{
    if ( _delayStage >= _delays.size() - 1 )
        return;

    ++_delayStage;

#if VERBOSE_DELAY
    logDebug() << "Increasing delay to stage " << _delayStage
               << ": " << currentDelay() << " millisec"
               << Qt::endl;
#endif
}


void AdaptiveTimer::decreaseDelay()
{
    if ( _delayStage == 0 )
        return;

    --_delayStage;

#if VERBOSE_DELAY
    logDebug() << "Decreasing delay to stage " << _delayStage
               << ": " << currentDelay() << " millisec"
               << Qt::endl;
#endif

    // continue to cool down even if there are no further requests
    _cooldownTimer.start( cooldownPeriod() );
}
