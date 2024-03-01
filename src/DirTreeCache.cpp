/*
 *   File name: DirTreeCache.cpp
 *   Summary:	QDirStat cache reader / writer
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include <ctype.h>
#include <QUrl>

#include "DirTreeCache.h"
#include "DirInfo.h"
#include "DirTree.h"
#include "DotEntry.h"
#include "ExcludeRules.h"
#include "FormatUtil.h"
#include "MountPoints.h"
#include "Logger.h"
#include "Exception.h"

#define KB 1024LL
#define MB (1024LL*1024)
#define GB (1024LL*1024*1024)
#define TB (1024LL*1024*1024*1024)

#define MAX_ERROR_COUNT			1000

#define VERBOSE_READ			0
#define VERBOSE_CACHE_DIRS		0
#define VERBOSE_CACHE_FILE_INFOS	0
#define DEBUG_LOCATE_PARENT		0

using namespace QDirStat;



bool CacheWriter::writeCache( const QString & fileName, const DirTree *tree )
{
    if ( !tree || !tree->root() )
	return false;

    gzFile cache = gzopen( (const char *) fileName.toUtf8(), "w" );

    if ( cache == 0 )
    {
	logError() << "Can't open " << fileName << ": " << formatErrno() << Qt::endl;
	return false;
    }

    gzprintf( cache, "[qdirstat %s cache file]\n", CACHE_FORMAT_VERSION );
    gzprintf( cache,
	     "# Do not edit!\n"
	     "#\n"
	     "# Type\tpath\t\tsize\tmtime\t\tmode\t<optional fields>\n"
	     "\n" );

    writeTree( cache, tree->root()->firstChild() );
    gzclose( cache );

    return true;
}


void CacheWriter::writeTree( gzFile cache, const FileInfo * item )
{
    if ( !item )
	return;

    // Write entry for this item
    if ( !item->isDotEntry() )
	writeItem( cache, item );

    // Write file children
    if ( item->dotEntry() )
	writeTree( cache, item->dotEntry() );

    // Recurse through subdirectories
    for ( FileInfo * child = item->firstChild(); child; child = child->next() )
	writeTree( cache, child );
}


void CacheWriter::writeItem( gzFile cache, const FileInfo * item )
{
    if ( !item )
	return;

    // Write file type
    const char * file_type = "";
    if	    ( item->isFile()		) file_type = "F";
    else if ( item->isDir()		) file_type = "D";
    else if ( item->isSymLink()		) file_type = "L";
    else if ( item->isBlockDevice()	) file_type = "BlockDev";
    else if ( item->isCharDevice()	) file_type = "CharDev";
    else if ( item->isFifo()		) file_type = "FIFO";
    else if ( item->isSocket()		) file_type = "Socket";
    gzprintf( cache, "%s", file_type );

    // Write name
    if ( item->isDirInfo() && !item->isDotEntry() )
	gzprintf( cache, " %s", urlEncoded( item->url() ).data() ); // absolute path
    else
	gzprintf( cache, "\t%s", urlEncoded( item->name() ).data() ); // relative path

    // Write size
    gzprintf( cache, "\t%s", formatSize( item->rawByteSize() ).toUtf8().data() );

    // Write mtime
    gzprintf( cache, "\t0x%lx", (unsigned long)item->mtime() );

    // For permissions, although also identifies the object type
    gzprintf( cache, "\t%07o", item->mode() );

    // Flags, also an excuse to keep an even number of extra fields
    // Currently the only flag is whether the object is excluded
    gzprintf( cache, "\t%0xlx", (unsigned long)item->isExcluded() );

    // Optional fields
    if ( item->isSparseFile() )
	gzprintf( cache, "\tblocks: %lld", item->blocks() );
    if ( item->isFile() && item->links() > 1 )
	gzprintf( cache, "\tlinks: %u", (unsigned) item->links() );

    // One item per line
    gzputc( cache, '\n' );
}


QByteArray CacheWriter::urlEncoded( const QString & path )
{
    // Using a protocol ("scheme") part to avoid directory names with a colon
    // ":" being cut off because it looks like a URL protocol.
    QUrl url;
    url.setScheme( "foo" );
    url.setPath( path );

    const QByteArray encoded = url.toEncoded( QUrl::RemoveScheme );
    if ( encoded.isEmpty() )
        logError() << "Invalid file/dir name: " << path << Qt::endl;

    return encoded;

}


QString CacheWriter::formatSize( FileSize size )
{
    // Only use size suffixes for exact multiples, otherwise the exact number of bytes
    if ( size >= TB && size % TB == 0 )
        return QString( "%1T" ).arg( size / TB );

    if ( size >= GB && size % GB == 0 )
        return QString( "%1G" ).arg( size / GB );

    if ( size >= MB && size % MB == 0 )
        return QString( "%1M" ).arg( size / MB );

    if ( size >= KB && size % KB == 0 )
        return QString( "%1K" ).arg( size / KB );

    return QString( "%1" ).arg( size );
}







CacheReader::CacheReader( const QString & fileName,
			  DirTree *	  tree,
			  DirInfo *	  parent ):
    QObject (),
    _tree { tree },
    _cache { gzopen( fileName.toUtf8(), "r" ) },
    _buffer { { 0 } },
    _line { _buffer },
    _lineNo { 0 },
    _fileName { fileName },
    _ok { true },
    _errorCount { 0 },
    _toplevel { parent },
    _lastDir { nullptr },
    _lastExcludedDir { nullptr },
    _multiSlash { "//+" } // cache regexp for multiple use
{
    if ( _cache == 0 )
    {
	logError() << "Can't open " << fileName << ": " << formatErrno() << Qt::endl;
	_ok = false;
	emit error();
	return;
    }

    //logDebug() << "Opening " << fileName << " OK" << Qt::endl;
    checkHeader();
}


CacheReader::~CacheReader()
{
    if ( _cache )
	gzclose( _cache );

    logDebug() << "Cache reading finished" << Qt::endl;

    if ( _toplevel )
    {
	// logDebug() << "Finalizing recursive for " << _toplevel << Qt::endl;
	finalizeRecursive( _toplevel );
	_toplevel->finalizeAll();
    }

    emit finished();
}


void CacheReader::rewind()
{
    if ( _cache )
    {
	gzrewind( _cache );
	checkHeader();		// skip cache header
    }
}


bool CacheReader::read( int maxLines )
{
    while ( !gzeof( _cache ) && _ok && ( maxLines == 0 || --maxLines > 0 ) )
    {
	if ( readLine() )
	{
	    splitLine();
	    addItem();
	}
    }

    return _ok && !gzeof( _cache );
}


void CacheReader::addItem()
{
    if ( fieldsCount() < 4 )
    {
	logError() << "Syntax error in " << _fileName << ":" << _lineNo
		   << ": Expected at least 4 fields, saw only " << fieldsCount()
		   << Qt::endl;

	setReadError( _lastDir );

	if ( ++_errorCount > MAX_ERROR_COUNT )
	{
	    logError() << "Too many syntax errors. Giving up." << Qt::endl;
	    _ok = false;
	    emit error();
	}

	return;
    }

    int n = 0;
    const char * type		= field( n++ );
    const char * raw_path	= field( n++ );
    const char * size_str	= field( n++ );
    const char * mtime_str	= field( n++ );
    const char * mode_str	= nullptr;
    const char * flags_str	= nullptr;
    const char * blocks_str	= nullptr;
    const char * links_str	= nullptr;

    // Look for mode first, but old formats won't have it at all
    if ( fieldsCount() > n )
    {
	mode_str = field( n );
	if ( *mode_str == '0' )
	{
	    // If we have mode, we should have flags next
	    ++n;
	    flags_str = field( n++ );
	}

    }

    // Optional key/value field pairs
    while ( fieldsCount() > n+1 )
    {
	const char * keyword	= field( n++ );
	const char * val_str	= field( n++ );

	// blocks: is used for sparse files to give the allocation
	if ( strcasecmp( keyword, "blocks:" ) == 0 ) blocks_str = val_str;

	// links: is used when there is more than one hard link
	if ( strcasecmp( keyword, "links:"  ) == 0 ) links_str	= val_str;
    }
/*
    // Type
    mode_t mode = S_IFREG;
    if	    ( strcasecmp( type, "F"	   ) == 0 )	mode = S_IFREG;
    else if ( strcasecmp( type, "D"	   ) == 0 )	mode = S_IFDIR;
    else if ( strcasecmp( type, "L"	   ) == 0 )	mode = S_IFLNK;
    else if ( strcasecmp( type, "BlockDev" ) == 0 )	mode = S_IFBLK;
    else if ( strcasecmp( type, "CharDev"  ) == 0 )	mode = S_IFCHR;
    else if ( strcasecmp( type, "FIFO"	   ) == 0 )	mode = S_IFIFO;
    else if ( strcasecmp( type, "Socket"   ) == 0 )	mode = S_IFSOCK;
*/
    mode_t mode;
    if ( mode_str )
    {
	mode = strtoll( mode_str, 0, 8 );
    }
    else
    {
	// No mode in old file formats,
	// get the object type from the first field, but no permissions
	switch ( toupper( *type ) )
	{
	    case 'F':
	    {
		// 'F' is ambiguous unfortunately
		if ( *(mode_str+1) == 0 )
		    mode = S_IFREG;
		else
		    mode = S_IFIFO;
		break;
	    }
	    case 'D': mode = S_IFDIR; break;
	    case 'L': mode = S_IFLNK; break;
	    case 'B': mode = S_IFBLK; break;
	    case 'C': mode = S_IFCHR; break;
	    case 'S': mode = S_IFSOCK; break;
	    default:  mode = S_IFREG; break;
	}
    }

    // Path
    if ( *raw_path == '/' )
	_lastDir = 0;

    const QString fullPath = unescapedPath( raw_path );
    QString path;
    QString name;
    splitPath( fullPath, path, name );

    if ( _lastExcludedDir && path.startsWith( _lastExcludedDirUrl ) )
    {
	// logDebug() << "Excluding " << path << "/" << name << Qt::endl;
	return;
    }

    // Size
    char * end = 0;
    FileSize size = strtoll( size_str, &end, 10 );
    if ( end )
    {
	switch ( *end )
	{
	    case 'K':	size *= KB; break;
	    case 'M':	size *= MB; break;
	    case 'G':	size *= GB; break;
	    case 'T':	size *= TB; break;
	    default: break;
	}
    }

    // MTime
    const time_t mtime = strtol( mtime_str, 0, 0 );

    // The only flag so far is isExlcuded, so just treat it as a straight boolean
    // isExcluded will only appear on directories ... excluded files don't appear
    const bool isExcluded = strtol( flags_str, 0, 0 );

    // Consider it a sparse file if the blocks field is present
    const bool isSparseFile = blocks_str;

    // Blocks: only stored for sparse files, otherwise just guess from the file size
    const FileSize blocks = blocks_str ?
			    strtoll( blocks_str, 0, 10 ) :
			    qCeil( (float)size / STD_BLOCK_SIZE );

    // Links
    const int links = links_str ? atoi( links_str ) : 1;

    // For sparse files, we can calculate the actual allocated size from the block count.
    // For other files, just use the file size with no assumptions about over-allocation
    const FileSize allocatedSize = isSparseFile ? blocks * STD_BLOCK_SIZE: size;

    // Find parent in tree.
    DirInfo * parent = _lastDir;
    if ( !parent && _tree->root() )
    {
	if ( !_tree->root()->hasChildren() )
	    parent = _tree->root();

	// Try the easy way first - the starting point of this cache
	if ( !parent && _toplevel )
	    parent = dynamic_cast<DirInfo *> ( _toplevel->locate( path ) );

#if DEBUG_LOCATE_PARENT
	if ( parent )
	    logDebug() << "Using cache starting point as parent for " << fullPath << Qt::endl;
#endif

	if ( !parent )
	{
	    // Fallback: Search the entire tree
	    parent = dynamic_cast<DirInfo *> ( _tree->locate( path ) );

#if DEBUG_LOCATE_PARENT
	    if ( parent )
		logDebug() << "Located parent " << path << " in tree" << Qt::endl;
#endif
	}

	if ( !parent && path.isEmpty() )
	{
	    // Everything except directories jas no path part in their filename, so if lastDir
	    // is missing, fro example, excluded, we can never find it by searching.  Skip
	    // (silently in release versions, or the log will be huge), but don't treat as an error.
#if DEBUG_LOCATE_PARENT
	    logWarning() << "no parent for " << path << " " << name << Qt::endl;
#endif
	    return;
	}

	if ( !parent ) // Still nothing?
	{
	    logError() << _fileName << ":" << _lineNo << ": "
		       << "Could not locate parent \"" << path << "\" for "
		       << name << Qt::endl;

            if ( ++_errorCount > MAX_ERROR_COUNT )
            {
                logError() << "Too many consistency errors. Giving up." << Qt::endl;
                _ok = false;
                emit error();
            }

#if DEBUG_LOCATE_PARENT
	    THROW( Exception( "Could not locate cache item parent" ) );
#endif
	    return;	// Ignore this cache line completely
	}
    }

    if ( ( mode & S_IFDIR ) == S_IFDIR ) // directory
    {
	QString url = ( parent == _tree->root() ) ? buildPath( path, name ) : name;
#if VERBOSE_CACHE_DIRS
	logDebug() << "Creating DirInfo for " << url << " with parent " << parent << Qt::endl;
#endif
	DirInfo * dir = new DirInfo( parent, _tree, url, mode, size, mtime );
	dir->setReadState( DirReading );
	_lastDir = dir;

	if ( parent )
	    parent->insertChild( dir );

	if ( !_tree->root() )
	{
	    _tree->setRoot( dir );
	    _toplevel = dir;
	}

	if ( !_toplevel )
	    _toplevel = dir;

	if ( !MountPoints::device( dir->url() ).isEmpty() )
	    dir->setMountPoint();

	_tree->childAddedNotify( dir );

	if ( dir != _toplevel )
	{
	    // Directories may be excluded because they were excluded at the time the cache
	    // was written, or because of a rule now.  Not perfect, but we can't show
	    // directories that no longer match an exclude rules because they just aren't
	    // populated in the cache file.
	    if ( isExcluded || ExcludeRules::instance()->match( dir->url(), dir->name() ) )
	    {
		logDebug() << "Excluding " << name << Qt::endl;
		dir->setExcluded();
		dir->setReadState( DirOnRequestOnly );
		dir->finalizeLocal();
		_tree->sendReadJobFinished( dir );

		_lastExcludedDir    = dir;
		_lastExcludedDirUrl = _lastExcludedDir->url();
		_lastDir	    = 0;
	    }
	}
    }
    else if ( parent ) // not directory
    {
#if VERBOSE_CACHE_FILE_INFOS
	logDebug() << "Creating FileInfo for " << buildPath( parent->debugUrl(), name ) << Qt::endl;
#endif

	FileInfo * item = new FileInfo( parent, _tree, name,
					mode, size, allocatedSize, mtime,
					isSparseFile, blocks, links );
	parent->insertChild( item );
	_tree->childAddedNotify( item );
    }
    else
    {
	logError() << _fileName << ":" << _lineNo << ": " << "No parent for item " << name << Qt::endl;
    }
}


bool CacheReader::eof() const
{
    if ( !_ok || !_cache )
	return true;

    return gzeof( _cache );
}


QString CacheReader::firstDir()
{
    while ( !gzeof( _cache ) && _ok )
    {
	if ( !readLine() )
	    return "";

	splitLine();

	if ( fieldsCount() < 2 )
	    return "";

	int n = 0;
	const char * type = field( n++ );
	const char * path = field( n++ );

	if ( strcasecmp( type, "D" ) == 0 )
	    return QString( path );
    }

    return "";
}


bool CacheReader::checkHeader()
{
    if ( !_ok || !readLine() )
	return false;

    // logDebug() << "Checking cache file header" << Qt::endl;
    const QString line( _line );
    splitLine();

    // Check for    [qdirstat <version> cache file]
    // or	    [kdirstat <version> cache file]

    if ( fieldsCount() != 4 )
	_ok = false;

    if ( _ok )
    {
	if ( ( strcmp( field( 0 ), "[qdirstat" ) != 0 &&
	       strcmp( field( 0 ), "[kdirstat" ) != 0 ) ||
	       strcmp( field( 2 ), "cache"     ) != 0 ||
	       strcmp( field( 3 ), "file]"     ) != 0 )
	{
	    _ok = false;
	    logError() << _fileName << ":" << _lineNo
		      << ": Unknown file format" << Qt::endl;
	}
    }

    if ( _ok )
    {
	QString version = field( 1 );

	// currently not checking version number
	// for future use

	if ( !_ok )
	    logError() << _fileName << ":" << _lineNo
		      << ": Incompatible cache file version" << Qt::endl;
    }

    // logDebug() << "Cache file header check OK: " << _ok << Qt::endl;

    if ( !_ok )
	emit error();

    return _ok;
}


bool CacheReader::readLine()
{
    if ( !_ok || !_cache )
	return false;

    _fieldsCount = 0;

    do
    {
	_lineNo++;

	if ( !gzgets( _cache, _buffer, MAX_CACHE_LINE_LEN-1 ) )
	{
	    _buffer[0]	= 0;
	    _line	= _buffer;

	    if ( !gzeof( _cache ) )
	    {
		_ok = false;
		logError() << _fileName << ":" << _lineNo << ": Read error" << Qt::endl;
		emit error();
	    }

	    return false;
	}

	_line = skipWhiteSpace( _buffer );
	killTrailingWhiteSpace( _line );

	// logDebug() << "line[ " << _lineNo << "]: \"" << _line<< "\"" << Qt::endl;

    } while ( !gzeof( _cache ) &&
	      ( *_line == 0   ||	// empty line
		*_line == '#'	  ) );	// comment line

    return true;
}


void CacheReader::splitLine()
{
    _fieldsCount = 0;

    if ( !_ok || !_line )
	return;

    if ( *_line == '#' )	// skip comment lines
	*_line = 0;

    char * current = _line;
    const char * end = _line + strlen( _line );

    while ( current
	    && current < end
	    && *current
	    && _fieldsCount < MAX_FIELDS_PER_LINE-1 )
    {
	_fields[ _fieldsCount++ ] = current;
	current = findNextWhiteSpace( current );

	if ( current && current < end )
	{
	    *current++ = 0;
	    current = skipWhiteSpace( current );
	}
    }
}


const char * CacheReader::field( int no ) const
{
    if ( no >= 0 && no < _fieldsCount )
	return _fields[ no ];
    else
	return nullptr;
}


char * CacheReader::skipWhiteSpace( char * cptr )
{
    if ( cptr == 0 )
	return nullptr;

    while ( *cptr != 0 && isspace( *cptr ) )
	cptr++;

    return cptr;
}


char * CacheReader::findNextWhiteSpace( char * cptr )
{
    if ( cptr == 0 )
	return nullptr;

    while ( *cptr != 0 && !isspace( *cptr ) )
	cptr++;

    return *cptr == 0 ? 0 : cptr;
}


void CacheReader::killTrailingWhiteSpace( char * cptr )
{
    if ( cptr == 0 )
	return;

    char * start = cptr;
    cptr = start + strlen( start ) - 1;

    while ( cptr >= start && isspace( *cptr ) )
	*cptr-- = 0;
}


void CacheReader::splitPath( const QString & fileNameWithPath,
			     QString	   & path_ret,
			     QString	   & name_ret ) const
{
    const bool absolutePath = fileNameWithPath.startsWith( "/" );
    QStringList components = fileNameWithPath.split( "/", Qt::SkipEmptyParts );

    if ( components.isEmpty() )
    {
	path_ret = "";
	name_ret = absolutePath ? "/" : "";
    }
    else
    {
	name_ret = components.takeLast();
	path_ret = components.join( "/" );

	if ( absolutePath )
	    path_ret.prepend( "/" );
    }
}


QString CacheReader::buildPath( const QString & path, const QString & name ) const
{
    if ( path.isEmpty() )
	return name;
    else if ( name.isEmpty() )
	return path;
    else if ( path == "/" )
	return path + name;
    else
	return path + "/" + name;
}


QString CacheReader::unescapedPath( const QString & rawPath ) const
{
    // Using a protocol part to avoid directory names with a colon ":"
    // being cut off because it looks like a URL protocol.
    const QString protocol = "foo:";
    const QString url = protocol + cleanPath( rawPath );

    return QUrl::fromEncoded( url.toUtf8() ).path();
}


QString CacheReader::cleanPath( const QString & rawPath ) const
{
    QString clean = rawPath;
    return clean.replace( _multiSlash, "/" );
}


void CacheReader::finalizeRecursive( DirInfo * dir )
{
    if ( dir->readState() != DirOnRequestOnly )
    {
	if ( !dir->readError() )
	    dir->setReadState( DirCached );

	dir->finalizeLocal();
	_tree->sendReadJobFinished( dir );
    }

    for ( FileInfo * child = dir->firstChild(); child; child = child->next() )
    {
	if ( child->isDirInfo() )
	    finalizeRecursive( child->toDirInfo() );
    }

}


void CacheReader::setReadError( DirInfo * dir ) const
{
    logDebug() << "Setting read error for " << dir << Qt::endl;

    while ( dir )
    {
	dir->setReadState( DirError );

	if ( dir == _toplevel )
	    return;

	dir = dir->parent();
    }
}
