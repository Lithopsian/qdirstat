/*
 *   File name:	ExcludeRules.cpp
 *   Summary:	Support classes for QDirStat
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include "ExcludeRules.h"
#include "DirInfo.h"
#include "DotEntry.h"
#include "FileInfoIterator.h"
#include "Settings.h"
#include "SettingsHelpers.h"
//#include "DebugHelpers.h"
#include "Logger.h"
#include "Exception.h"


#define VERBOSE_EXCLUDE_MATCHES	0

using namespace QDirStat;


ExcludeRule::ExcludeRule( PatternSyntax   patternSyntax,
			  const QString & pattern,
			  bool            caseSensitive,
			  bool            useFullPath,
			  bool            checkAnyFileChild ):
    QRegularExpression { formatPattern( patternSyntax, pattern ), makePatternOptions( caseSensitive ) },
    _patternSyntax { patternSyntax },
    _pattern { pattern },
    _useFullPath { useFullPath },
    _checkAnyFileChild { checkAnyFileChild }
{
#if VERBOSE_EXCLUDE_MATCHES
    logDebug() << _patternSyntax << " " << _pattern << ( caseSensitive ? ", case-sensitive" : ", case-insensitive" << Qt::endl;
#endif
    // NOP
}


ExcludeRule::~ExcludeRule()
{
    //logDebug() << "ExcludeRule " << _pattern << " destructor" << Qt::endl;
    // NOP
}


void ExcludeRule::setCaseSensitive( bool caseSensitive )
{
    setPatternOptions( makePatternOptions( caseSensitive ) );
}


QString ExcludeRule::formatPattern( PatternSyntax patternSyntax,
				    const QString &    pattern )
{
    // Anchor and escape all special characters so a regexp match behaves like a simple comparison
    if ( patternSyntax == FixedString )
	return Wildcard::anchoredPattern( Wildcard::escape( pattern ) );

    // Convert the *, ?, and [] wildcards to regexp equivalents and anchor the pattern
    if ( patternSyntax == Wildcard )
	return Wildcard::wildcardToRegularExpression( pattern );

    // Note: unanchored for regexp!
    return pattern;
}


void ExcludeRule::setPatternSyntax( PatternSyntax patternSyntax )
{
    _patternSyntax = patternSyntax;
    setPattern( formatPattern( patternSyntax, _pattern ) );
}


void ExcludeRule::setPattern( const QString & pattern )
{
    _pattern = pattern;
    QRegularExpression::setPattern( formatPattern( _patternSyntax, pattern ) );
}


bool ExcludeRule::match( const QString & fullPath, const QString & fileName )
{
    if ( _checkAnyFileChild )  // use matchDirectChildren() for those rules
        return false;

    const QString matchText = _useFullPath ? fullPath : fileName;

    if ( matchText.isEmpty() )
	return false;

    if ( _pattern.isEmpty() )
	return false;

    return isMatch( matchText );
}


bool ExcludeRule::matchDirectChildren( DirInfo * dir )
{
    if ( ! _checkAnyFileChild || ! dir )
        return false;

    if ( _pattern.isEmpty() )
        return false;

    FileInfoIterator it( dir->dotEntry() ? dir->dotEntry() : dir );

    while ( *it )
    {
        if ( ! (*it)->isDir() && isMatch( (*it)->name() ) )
                return true;

        ++it;
    }

    return false;
}


SettingsEnumMapping ExcludeRule::patternSyntaxMapping()
{
    static SettingsEnumMapping mapping;

    if ( mapping.isEmpty() )
    {
	mapping[ RegExp      ] = "RegExp";
	mapping[ Wildcard    ] = "Wildcard";
	mapping[ FixedString ] = "FixedString";
    }

    return mapping;
}


//
//---------------------------------------------------------------------------
//

ExcludeRules::ExcludeRules()
{
    readSettings();
#if VERBOSE_EXCLUDE_MATCHES
    Debug::dumpExcludeRules();
#endif
}


ExcludeRules::ExcludeRules( const QStringList & paths,
			    ExcludeRule::PatternSyntax patternSyntax,
			    bool caseSensitive,
			    bool useFullPath,
			    bool checkAnyFileChild )
{
    foreach ( const QString & path, paths )
        add( patternSyntax, path, caseSensitive, useFullPath, checkAnyFileChild );
}


ExcludeRules::~ExcludeRules()
{
    clear();
}


ExcludeRules * ExcludeRules::instance()
{
    static ExcludeRules _instance;

    return &_instance;
}


void ExcludeRules::clear()
{
    qDeleteAll( _rules );
    _rules.clear();
}

/*
void ExcludeRules::add( ExcludeRule * rule )
{
    CHECK_NEW( rule );
    _rules << rule;
    logInfo() << "Added " << rule << Qt::endl;
}
*/

void ExcludeRules::add( ExcludeRule::PatternSyntax   patternSyntax,
			const QString              & pattern,
			bool                         caseSensitive,
                        bool                         useFullPath,
                        bool                         checkAnyFileChild )
{
    ExcludeRule * rule = new ExcludeRule( patternSyntax, pattern, caseSensitive, useFullPath, checkAnyFileChild );
    CHECK_NEW( rule );

    _rules << rule;

    logInfo() << "Added " << rule << Qt::endl;
}

/*
void ExcludeRules::remove( ExcludeRule * rule )
{
    CHECK_PTR( rule );

    _rules.removeAll( rule );
    delete rule;
}
*/

bool ExcludeRules::match( const QString & fullPath, const QString & fileName )
{
    if ( fullPath.isEmpty() || fileName.isEmpty() )
	return false;

    foreach ( ExcludeRule * rule, _rules )
    {
	if ( rule->match( fullPath, fileName ) )
	{
#if VERBOSE_EXCLUDE_MATCHES
	    logDebug() << fullPath << " matches " << rule << Qt::endl;
#endif
	    return true;
	}
    }

    return false;
}


bool ExcludeRules::matchDirectChildren( DirInfo * dir )
{
    if ( ! dir )
	return false;

    foreach ( ExcludeRule * rule, _rules )
    {
	if ( rule->matchDirectChildren( dir ) )
	{
#if VERBOSE_EXCLUDE_MATCHES
	    logDebug() << dir << " matches " << rule << Qt::endl;
#endif
	    return true;
	}
    }

    return false;
}

/*
const ExcludeRule * ExcludeRules::matchingRule( const QString & fullPath,
						const QString & fileName )
{
    if ( fullPath.isEmpty() || fileName.isEmpty() )
	return 0;

    foreach ( ExcludeRule * rule, _rules )
    {
	if ( rule->match( fullPath, fileName ) )
	    return rule;
    }

    return 0;
}
*/
/*
void ExcludeRules::moveUp( ExcludeRule * rule )
{
    _listMover.moveUp( rule );
}


void ExcludeRules::moveDown( ExcludeRule * rule )
{
    _listMover.moveDown( rule );
}


void ExcludeRules::moveToTop( ExcludeRule * rule )
{
    _listMover.moveToTop( rule );
}


void ExcludeRules::moveToBottom( ExcludeRule * rule )
{
    _listMover.moveToBottom( rule );
}
*/

void ExcludeRules::addDefaultRules()
{
    //logInfo() << "Adding default exclude rules" << Qt::endl;

    add( ExcludeRule::FixedString, "/timeshift", true, true, false );

    ExcludeRuleSettings settings;
    settings.setValue( "DefaultExcludeRulesAdded", true );

    writeSettings( _rules );
}


void ExcludeRules::readSettings()
{
    clear();

    ExcludeRuleSettings settings;
    const QStringList excludeRuleGroups = settings.findGroups( settings.groupPrefix() );

    if ( ! excludeRuleGroups.isEmpty() )
    {
	// Read all settings groups [ExcludeRule_xx] that were found

	foreach ( const QString & groupName, excludeRuleGroups )
	{
	    settings.beginGroup( groupName );

	    // Read one exclude rule

	    const QString pattern        = settings.value( "Pattern"	              ).toString();
	    const bool caseSensitive     = settings.value( "CaseSensitive",     true  ).toBool();
	    const bool useFullPath       = settings.value( "UseFullPath",	false ).toBool();
            const bool checkAnyFileChild = settings.value( "CheckAnyFileChild", false ).toBool();
	    const int syntax = readEnumEntry( settings,
					      "Syntax",
					      ExcludeRule::RegExp,
					      ExcludeRule::patternSyntaxMapping() );

	    ExcludeRule * rule = new ExcludeRule( ( ExcludeRule::PatternSyntax )syntax,
						  pattern,
						  caseSensitive,
						  useFullPath,
						  checkAnyFileChild );
	    CHECK_NEW( rule );

	    if ( ! pattern.isEmpty() && rule->isValid() )
		_rules << rule;
	    else
	    {
		logError() << "Invalid regexp: \"" << rule->pattern()
			   << "\": " << rule->errorString()
			   << Qt::endl;
		delete rule;
	    }

	    settings.endGroup(); // [ExcludeRule_01], [ExcludeRule_02], ...
	}
    }

    if ( isEmpty() && !settings.value( "DefaultExcludeRulesAdded", false ).toBool() )
	addDefaultRules();
}


void ExcludeRules::writeSettings( const ExcludeRuleList & newRules )
{
    ExcludeRuleSettings settings;

    // Remove all leftover exclude rule descriptions
    settings.removeGroups( settings.groupPrefix() );

    // Similar to CleanupCollection::writeSettings(), using a separate group
    // for each exclude rule for better readability in the settings file.

    for ( int i=0; i < newRules.size(); ++i )
    {
	const ExcludeRule * rule = newRules.at(i);

	if ( rule && ! rule->pattern().isEmpty() )
	{
	    settings.beginGroup( "ExcludeRule", i+1 );

	    settings.setValue( "Pattern",	    rule->pattern()           );
	    settings.setValue( "CaseSensitive",	    rule->caseSensitive()     );
	    settings.setValue( "UseFullPath",	    rule->useFullPath()       );
	    settings.setValue( "CheckAnyFileChild", rule->checkAnyFileChild() );

	    writeEnumEntry( settings, "Syntax",
			    rule->patternSyntax(),
			    ExcludeRule::patternSyntaxMapping() );

	    settings.endGroup(); // [ExcludeRule_01], [ExcludeRule_02], ...
	}
    }

    // Now replace the previous settings with the new ones
    readSettings();
}
