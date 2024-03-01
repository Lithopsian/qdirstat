/*
 *   File name: MimeCategorizer.cpp
 *   Summary:	Support classes for QDirStat
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */

#include <QElapsedTimer>

#include "MimeCategorizer.h"
#include "FileInfo.h"
#include "Settings.h"
#include "SettingsHelpers.h"
#include "Logger.h"
#include "Exception.h"

using namespace QDirStat;

MimeCategorizer * MimeCategorizer::instance()
{
    static MimeCategorizer _instance;
    return &_instance;
}


MimeCategorizer::MimeCategorizer():
    QObject()
{
    //logDebug() << "Creating MimeCategorizer" << Qt::endl;
    readSettings();
}


MimeCategorizer::~MimeCategorizer()
{
    clear();
}


void MimeCategorizer::clear()
{
    qDeleteAll( _categories );
    _categories.clear();
}


const QString & MimeCategorizer::name( const FileInfo * item )
{
    const QReadLocker locker( &_lock );

    return category( item )->name();
}


const QColor & MimeCategorizer::color( const FileInfo * item )
{
    const QReadLocker locker( &_lock );

    return category( item )->color();
}


const MimeCategory * MimeCategorizer::category( const FileInfo * item, QString * suffix_ret )
{
    if ( !item )
	return nullptr;

    CHECK_MAGIC( item );

    const QReadLocker locker( &_lock );

    return category( item->name(), suffix_ret );
}

/*
const MimeCategory * MimeCategorizer::category( const QString & filename )
{
    const QReadLocker locker( &_lock );

    return category( filename, 0 );
}
*/

const MimeCategory * MimeCategorizer::category( const FileInfo * item ) const
{
    if ( item->isSymLink() )
	return _symlinkCategory;

    if ( item->isFile() )
    {
	const MimeCategory *matchedCategory = category( item->name(), 0 );
	if ( matchedCategory )
	    return matchedCategory;

	if ( ( item->mode() & S_IXUSR ) == S_IXUSR )
	    return _executableCategory;
	else
	    return &_emptyCategory;
    }

    return &_emptyCategory;
}


const MimeCategory * MimeCategorizer::category( const QString & filename,
						QString	* suffix_ret ) const
{
    if ( suffix_ret )
	*suffix_ret = "";

    if ( filename.isEmpty() )
	return nullptr;

    const MimeCategory *category = nullptr;

    // Whole filename exact matching will be relatively rare, so quickly check if it is
    // possible to produce any matches before doing the actual case-sensitive and insensitive
    // tests.  There are a finite set of pattern lengths and only filenames of these lengths
    // can match.
    const int length = filename.size();

    if ( length < _caseSensitiveLengths.size() && _caseSensitiveLengths.testBit( length ) )
    {
	category = _caseSensitiveExact.value( filename, nullptr );
	if ( category )
	    return category;
    }

    if ( length < _caseInsensitiveLengths.size() && _caseInsensitiveLengths.testBit( length ) && !filename.isLower() )
    {
	// A lowercased filename will have been detected already because there is a pattern
	// in the case-sensitive map, so only filenames which are not lowercase are of
	// interest here.
	category = _caseInsensitiveExact.value( filename.toLower(), nullptr );
	if ( category )
	    return category;
    }

    // Find the filename suffix, ignoring any leading '.' separator
    // Ignore an initial dot and treat repeated dots as a single separator
    QString suffix = filename.section( '.', 1, -1, QString::SectionSkipEmpty );

    while ( !suffix.isEmpty() )
    {
        // logVerbose() << "Checking " << suffix << Qt::endl;

	// Try case sensitive first (also ncludes upper- and lower-cased suffixes
	// from the case-insensitive lists)
	category = matchWildcardSuffix( _caseSensitiveSuffixes, filename, suffix );

	if ( !category && suffix.size() > 1 && !suffix.isLower() && !suffix.isUpper() )
	    category = matchWildcardSuffix( _caseInsensitiveSuffixes, filename, suffix.toLower() );

	if ( category ) // success
        {
            if ( suffix_ret )
                *suffix_ret = suffix;

	    return category;
        }

	// No match so far? Try the next suffix. Some files might have more
	// than one, e.g., "tar.bz2" - if there is no match for "tar.bz2",
	// there might be one for just "bz2".

	suffix = suffix.section( '.', 1, -1, QString::SectionSkipEmpty );
    }

    // Go through all the plain regular expressions one by one
    category = matchWildcard( filename );

#if 0
    if ( category )
	logVerbose() << "Found " << category << " for " << filename << Qt::endl;
#endif

    return category;
}


const MimeCategory * MimeCategorizer::matchWildcardSuffix( const QMultiHash<QString, WildcardPair> & map,
							   const QString & filename,
							   const QString & suffix ) const
{
    const auto rangeIts = map.equal_range( suffix );
    for ( auto it = rangeIts.first; it != rangeIts.second && it.key() == suffix; ++it )
    {
	WildcardPair pair = it.value();
	if ( pair.first.isEmpty() || pair.first.isMatch( filename ) )
	    return pair.second;
    }

    return nullptr;
}


const MimeCategory * MimeCategorizer::matchWildcard( const QString & filename ) const
{
    for ( const WildcardPair & pair : _wildcards )
    {
	if ( pair.first.isMatch( filename ) )
	    return pair.second;
    }

    return nullptr; // No match
}


const MimeCategory * MimeCategorizer::findCategoryByName( const QString & categoryName ) const
{
    for ( const MimeCategory * category : _categories )
    {
	if ( category && category->name() == categoryName )
	    return category;
    }

    return nullptr; // No match
}


MimeCategory * MimeCategorizer::create( const QString & name, const QColor & color )
{
    MimeCategory * category = new MimeCategory( name, color );
    CHECK_NEW( category );

    _categories << category;

    return category;
}


void MimeCategorizer::buildMaps()
{
    QElapsedTimer stopwatch;
    stopwatch.start();

    _caseInsensitiveExact.clear();
    _caseSensitiveExact.clear();
    _caseInsensitiveSuffixes.clear();
    _caseSensitiveSuffixes.clear();
    _wildcards.clear();
    _caseInsensitiveLengths.clear();
    _caseSensitiveLengths.clear();

    for ( MimeCategory * category : _categories )
    {
	CHECK_PTR( category );

	addExactKeys( category );
	addSuffixKeys( category );
	addWildcardKeys( category );
	buildWildcardLists( category );
    }

    logDebug() << "maps built in " << stopwatch.restart() << "ms - " << _wildcards.size() << " regular expressions" << Qt::endl;
}


void MimeCategorizer::addExactKeys( MimeCategory * category )
{
    //logDebug() << "adding " << keyList << " to " << category << Qt::endl;
    for ( const QString & key : category->caseSensitiveExactList() )
	// Add key to the case-sensitive map and record this length
	addExactKey( _caseSensitiveExact, _caseSensitiveLengths, key, category );

    for ( const QString & key : category->caseInsensitiveExactList() )
    {
	// Add key to the case-insensitive map and record this length.  Also add
	// the lowercased name to the case-sensitive map as a common case that
	// will get picked up earlier and prevent the filename string having to
	// be copied when it is converted to lowercase.
	const QString lower = key.toLower();
	addExactKey( _caseInsensitiveExact, _caseInsensitiveLengths, lower, category );
	addExactKey( _caseSensitiveExact, _caseSensitiveLengths, lower, category );
    }
}


void MimeCategorizer::addExactKey( QHash<QString, MimeCategory *> & keys,
				   QBitArray & lengths,
				   const QString & key,
				   MimeCategory * category )
{
    //logDebug() << "adding " << keyList << " to " << category << Qt::endl;
    if ( keys.contains( key ) )
    {
	logError() << "Duplicate key: " << key << " for "
		   << keys.value( key ) << " and " << category
		   << Qt::endl;
    }
    else
    {
	// Add this pattern with no wildcards into a hash map
	keys.insert( key, category );

	// Mark the length pf this pattern so we only try to match filenames wih the right length
	const int length = key.size();
	if ( length >= lengths.size() )
	    lengths.resize( length + 1 );
	lengths.setBit( length );
    }
}


void MimeCategorizer::addWildcardKeys( MimeCategory * category )
{
    //logDebug() << "adding " << patternList << " to " << category << Qt::endl;

    // Add any case-insensitive regular expression, plus a case-sensitive lowercased version
    for ( const QString & pattern : category->caseInsensitiveWildcardSuffixList() )
    {
	const QString suffix = pattern.section( "*.", -1 ).toLower();
	const auto pair = WildcardPair( CaseInsensitiveWildcard( pattern ), category );
	_caseInsensitiveSuffixes.insert( suffix, pair );
	_caseSensitiveSuffixes.insert( suffix, pair );
    }

    // Add any case-sensitive regular expressions last so they are retrieved first
    for ( const QString & pattern : category->caseSensitiveWildcardSuffixList() )
    {
	const QString suffix = pattern.section( "*.", -1 );
	_caseSensitiveSuffixes.insert( suffix, { CaseSensitiveWildcard( pattern ), category } );
    }
}


void MimeCategorizer::addSuffixKeys( MimeCategory * category )
{
    //logDebug() << "adding " << keyList << " to " << category << Qt::endl;

    // Add simple suffix matches into case-sensitive and case-insensitive hash maps
    for ( const QString & suffix : category->caseInsensitiveSuffixList() )
    {
	const QString lowercaseSuffix = suffix.toLower();
	const QString uppercaseSuffix = suffix.toUpper();
	addSuffixKey( _caseInsensitiveSuffixes, suffix, category );

	// Add a lowercased and an uppercased version of the suffix into tyhe case-sensitive map
	addSuffixKey( _caseSensitiveSuffixes, lowercaseSuffix, category );
	if ( lowercaseSuffix != uppercaseSuffix)
	    addSuffixKey( _caseSensitiveSuffixes, uppercaseSuffix, category );
    }

    // Add any case-sensitive regular expressions last so they are retrieved first
    for ( const QString & suffix : category->caseSensitiveSuffixList() )
	addSuffixKey( _caseSensitiveSuffixes, suffix, category );
}


void MimeCategorizer::addSuffixKey( QMultiHash<QString, WildcardPair> & suffixes,
					const QString & suffix,
					MimeCategory * category )
{
    if ( suffixes.contains( suffix ) )
    {
	logError() << "Duplicate suffix: " << suffix << " for "
		   << suffixes.value( suffix ).first.pattern() << " and " << category
		   << Qt::endl;
    }
    else
    {
	suffixes.insert( suffix, { Wildcard(), category } );
    }
}


void MimeCategorizer::buildWildcardLists( MimeCategory * category )
{
    //logDebug() << "adding " << keyList << " to " << category << Qt::endl;
    for ( const QString & pattern : category->caseSensitiveWildcardList() )
	_wildcards << WildcardPair( { CaseSensitiveWildcard( pattern ), category } );

    for ( const QString & pattern : category->caseInsensitiveWildcardList() )
	_wildcards << WildcardPair( { CaseInsensitiveWildcard( pattern ), category } );
}


void MimeCategorizer::readSettings()
{
    //logDebug() << Qt::endl;

    Settings generalSettings;
    generalSettings.beginGroup( "MimeCategorizer" );
    generalSettings.endGroup();

    MimeCategorySettings settings;
    const QStringList mimeCategoryGroups = settings.findGroups( settings.groupPrefix() );

    clear();

    // Read all settings groups [MimeCategory_xx] that were found

    for ( const QString & groupName : mimeCategoryGroups )
    {
	settings.beginGroup( groupName );

	QString name  = settings.value( "Name", groupName ).toString();
	QColor	color = readColorEntry( settings, "Color", QColor( "#b0b0b0" ) );
	QStringList patternsCaseInsensitive = settings.value( "PatternsCaseInsensitive" ).toStringList();
	QStringList patternsCaseSensitive   = settings.value( "PatternsCaseSensitive"	).toStringList();

	//logDebug() << name << Qt::endl;
	MimeCategory * category = create( name, color );
	category->addPatterns( patternsCaseInsensitive, Qt::CaseInsensitive );
	category->addPatterns( patternsCaseSensitive,	Qt::CaseSensitive   );

	settings.endGroup(); // [MimeCategory_01], [MimeCategory_02], ...
    }

    if ( _categories.isEmpty() )
	addDefaultCategories();

    ensureMandatoryCategories();

    buildMaps();
}


void MimeCategorizer::replaceCategories( const MimeCategoryList & categories )
{
//    const QWriteLocker locker( &_lock );

   _lock.lockForWrite();
    writeSettings( categories );
    readSettings();
    _lock.unlock();

    // Unlock before the signal to avoid deadlocks
    emit categoriesChanged();
}


void MimeCategorizer::writeSettings( const MimeCategoryList & categoryList )
{
    //logDebug() << Qt::endl;

    MimeCategorySettings settings;

    // Remove all leftover cleanup descriptions
    settings.removeGroups( settings.groupPrefix() );

    for ( int i=0; i < categoryList.size(); ++i )
    {
	settings.beginGroup( "MimeCategory", i+1 );

	MimeCategory * category = categoryList.at(i);

	settings.setValue( "Name", category->name() );
	//logDebug() << "Adding " << category->name() << Qt::endl;
	writeColorEntry( settings, "Color", category->color() );

	QStringList patterns = category->humanReadablePatternList( Qt::CaseInsensitive );

	if ( patterns.isEmpty() )
	    patterns << "";

	settings.setValue( "PatternsCaseInsensitive", patterns );

	patterns = category->humanReadablePatternList( Qt::CaseSensitive );

	if ( patterns.isEmpty() )
	    patterns << "";

	settings.setValue( "PatternsCaseSensitive", patterns );

	settings.endGroup(); // [MimeCategory_01], [MimeCategory_02], ...
    }
}


void MimeCategorizer::ensureMandatoryCategories()
{
    // Remember this category so we don't have to search for it every time
    _executableCategory = findCategoryByName( CATEGORY_EXECUTABLE );
    if ( !_executableCategory )
    {
	// Special catchall category for files that don't match anything else, cannot be deleted
	_executableCategory = create( tr( CATEGORY_EXECUTABLE ), Qt::magenta );
	writeSettings( _categories );
    }

    // Remember this category so we don't have to search for it every time
    _symlinkCategory = findCategoryByName( CATEGORY_SYMLINK );
    if ( !_symlinkCategory )
    {
	// Special category for symlinks regardless of the filename, cannot be deleted
	_symlinkCategory = create( tr( CATEGORY_SYMLINK ), Qt::blue );
	writeSettings( _categories );
    }
}


void MimeCategorizer::addDefaultCategories()
{
    MimeCategory * archives = create( tr( "archive (compressed)" ), "#00ff00" );
    archives->addSuffixes( { "7z", "arj", "bz2", "cab", "cpio.gz", "gz", "jmod", "jsonlz4", "lz",
	                     "lzo", "rar", "tar.bz2", "tar.gz", "tar.lz", "tar.lzo", "tar.xz",
			     "tar.zst", "tbz2", "tgz", "txz", "tz2", "tzst", "xz", "zip", "zst" },
			   Qt::CaseInsensitive );
    archives->addPatterns( { "pack-*.pack" },
                           Qt::CaseSensitive );      // Git archive

    MimeCategory * uncompressedArchives = create( tr( "archive (uncompressed)" ), "#88ff88" );
    uncompressedArchives->addSuffixes( { "cpio", "tar" },
				       Qt::CaseInsensitive );

    MimeCategory * compressed = create( tr( "configuration file" ), "#aabbff" );
    compressed->addSuffixes( { "alias", "cfg", "conf", "conffiles", "config", "dep", "desktop",
	                       "ini", "kmap", "lang", "my", "page", "properties", "rc", "service",
			       "shlibs", "symbols", "templates", "theme", "triggers", "xcd", "xsl" },
			     Qt::CaseSensitive );
    compressed->addPatterns( { ".config", ".gitignore", "Kconfig", "control", "gtkrc" },
			     Qt::CaseSensitive );

    MimeCategory * database = create( tr( "database" ), "#22aaff" );
    database->addSuffixes( { "alias.bin", "builtin.bin", "dat", "db", "dep.bin", "enc", "hwdb",
	                       "idx", "lm", "md5sums", "odb", "order", "sbstore", "sqlite",
			       "sqlite-wal", "symbols.bin", "tablet", "vlpset", "yaml" },
			   Qt::CaseSensitive );
    database->addPatterns( { "magic.mgc" },
			   Qt::CaseSensitive );

    MimeCategory * diskImage = create( tr( "disk image" ), "#aaaaaa" );
    diskImage->addSuffixes( { "fsa", "iso" },
			    Qt::CaseSensitive );
    diskImage->addSuffixes( { "BIN", "img" },
			    Qt::CaseInsensitive );

    MimeCategory * doc = create( tr( "document" ), "#66ccff" );
    doc->addSuffixes( { "list", "log.0", "log.1", "odc", "odg", "odp", "ods", "odt", "otc",
	                "otp", "ots", "ott" },
		      Qt::CaseSensitive );
    doc->addSuffixes( { "css", "csv", "doc", "docbook", "docx", "dotx", "dvi", "dvi.bz2", "epub", "htm",
	                "html", "json", "latex", "log", "md", "pdf", "pod", "potx", "ppsx", "ppt",
			"pptx", "ps", "readme", "rst", "sav", "sdc", "sdc.gz", "sdd", "sdp", "sdw",
			"sla", "sla.gz", "slaz", "sxi", "tex", "txt", "xls", "xlsx", "xlt", "xml" },
		      Qt::CaseInsensitive );
    database->addPatterns( { "copyright", "readme.*" },
			   Qt::CaseInsensitive );

    MimeCategory * executable = create( tr( "executable" ), "#ff00ff" );
    executable->addPatterns( { "lft.db", "traceproto.db", "traceroute.db" },
			     Qt::CaseSensitive );
    executable->addSuffixes( { "jsa", "ucode" },
			     Qt::CaseSensitive );

    MimeCategory * font = create( tr( "font" ), "#44ddff" );
    font->addSuffixes( { "afm", "bdf", "cache-7", "cache-8", "otf", "pcf", "pcf.gz", "pf1",
	                 "pf2", "pfa", "pfb", "t1", "ttf" },
		       Qt::CaseSensitive );

    MimeCategory * game = create( tr( "game file" ), "#ff88dd" );
    game->addSuffixes( { "NHK", "bsp", "mdl", "pak", "wad" },
		       Qt::CaseSensitive );

    MimeCategory * image = create( tr( "image" ), "#00ffff" );
    image->addSuffixes( { "gif", "jpeg", "jpg", "jxl", "mng", "png", "tga", "tif", "tiff",
	                  "webp", "xcf.bz2", "xcf.gz" },
			Qt::CaseInsensitive );

    MimeCategory * uncompressedImage = create( tr( "image (uncompressed)" ), "#88ffff" );
    uncompressedImage->addSuffixes( { "bmp", "pbm", "pgm", "pnm", "ppm", "spr", "svg", "xcf" },
				    Qt::CaseInsensitive );

    MimeCategory * junk = create( tr( "junk" ), "#ff0000" );
    junk->addSuffixes( { "~", "bak", "keep", "old" },
		       Qt::CaseInsensitive );
    junk->addPatterns( { "core" },
		       Qt::CaseSensitive );

    MimeCategory * music = create( tr( "music" ), "#ffff00" );
    music->addSuffixes( { "aac", "ape", "f4a", "f4b", "flac", "m4a", "m4b", "mid", "mka", "mp3",
			  "oga", "ogg", "opus", "ra", "rax", "wav", "wma" },
			Qt::CaseInsensitive );

    MimeCategory * obj = create( tr( "object file" ), "#ee8822" );
    obj->addSuffixes( { "Po", "a.cmd", "al", "elc", "go", "gresource", "ko", "ko.cmd", "ko.xz",
	                "ko.zst", "la", "lo", "mo", "moc", "o", "o.cmd", "pyc", "qrc", "typelib" },
                      Qt::CaseSensitive );
    obj->addPatterns( { "built-in.a", "vmlinux.a" },
                      Qt::CaseSensitive );
    obj->addPatterns( { "lib*.a" },
                      Qt::CaseInsensitive );

    MimeCategory * program = create( tr( "packaged program" ), "#88aa66" );
    program->addSuffixes( { "deb", "ja", "jar", "sfi", "tm" },
		       Qt::CaseSensitive );
    program->addSuffixes( { "rpm", "xpi" },
		       Qt::CaseInsensitive );

    MimeCategory * script = create( tr( "script" ), "#ff8888" );
    script->addSuffixes( { "BAT", "bash", "bashrc", "csh", "js", "ksh", "m4", "pl", "pm", "postinst",
	                   "postrm", "preinst", "prerm", "sh", "tcl", "tmac", "xba", "zsh" },
		         Qt::CaseSensitive );

    MimeCategory * source = create( tr( "source file" ), "#ffbb44" );
    source->addSuffixes( { "S", "S_shipped", "asm", "c", "cc", "cmake", "cpp", "cxx", "dts",
	                   "dtsi", "el", "f", "fuc3", "fuc3.h", "fuc5", "fuc5.h", "gir", "h",
			   "h_shipped", "hpp", "java", "msg", "ph", "php", "po", "pot", "pro",
			   "pxd", "py", "pyi", "pyx", "rb", "scm" },
		         Qt::CaseSensitive );
    source->addPatterns( { "Kbuild", "Makefile" },
		         Qt::CaseSensitive );

    MimeCategory * generated = create( tr( "source file (generated)" ), "#ffaa22" );
    generated->addSuffixes( { "f90", "mod.c", "qm", "qml", "ui" },
			    Qt::CaseSensitive );
    generated->addPatterns( { "moc_*.cpp", "qrc_*.cpp", "ui_*.h" },
			    Qt::CaseSensitive );

    create( tr( "symlink" ), "#0000ff" );

    MimeCategory * shared = create( tr( "shared object" ), "#ff7722" );
    shared->addSuffixes( { "so.,0", "so.1" },
		         Qt::CaseSensitive );
    shared->addSuffixes( { "dll", "so" },
                         Qt::CaseInsensitive );
    shared->addPatterns( { "*.so.*" },
		         Qt::CaseSensitive );

    MimeCategory * videos = create( tr( "video" ), "#aa44ff" );
    videos->addSuffixes( { "asf", "avi", "divx", "flc", "fli", "flv", "m2ts", "m4v", "mk3d",
			   "mkv", "mov", "mp2", "mp4", "mpeg", "mpg", "ogm", "ogv",
			   "rm", "vdr", "vob", "webm", "wmp", "wmv" },
			 Qt::CaseInsensitive );

    writeSettings( _categories );
}
