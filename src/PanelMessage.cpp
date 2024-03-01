/*
 *   File name: PanelMessage.cpp
 *   Summary:	Message in a panel with icon and close button
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */

#include "PanelMessage.h"
#include "SysUtil.h"
#include "Logger.h"
#include "Exception.h"


using namespace QDirStat;


PanelMessage::PanelMessage( QWidget	  * parent,
			    const QString & headingText,
			    const QString & msgText ):
    QWidget ( parent ),
    _ui { new Ui::PanelMessage }
{
    CHECK_NEW( _ui );

    _ui->setupUi( this );

    _ui->headingLabel->setText( headingText );
    _ui->msgLabel->setText( msgText );

    _ui->detailsLinkLabel->hide();
}


PanelMessage::~PanelMessage()
{
    delete _ui;
}


void PanelMessage::setDetails( const QString & urlText )
{
    _ui->detailsLinkLabel->setText( "<a href=\"details\">" + urlText + "</a>" );
}


void PanelMessage::connectDetailsLink( const QObject * receiver,
				       const char    * slotName )
{
    _ui->detailsLinkLabel->show();

    connect( _ui->detailsLinkLabel,	SIGNAL( linkActivated( QString ) ),
	     receiver,			slotName );
}


void PanelMessage::setDetailsUrl( const QString url )
{
    _detailsUrl = url;
    connectDetailsLink( this, SLOT( openDetailsUrl() ) );
}


void PanelMessage::openDetailsUrl() const
{
    SysUtil::openInBrowser( _detailsUrl );
}


void PanelMessage::setIcon( const QPixmap & pixmap )
{
    if ( pixmap.isNull() )
    {
	_ui->iconLabel->hide();
    }
    else
    {
	_ui->iconLabel->setPixmap( pixmap );
	_ui->iconLabel->show();
    }
}
