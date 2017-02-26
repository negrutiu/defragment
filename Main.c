#include "StdAfx.h"
#include "Defragment.h"
#include "FileList.h"


#define DEFAULT_TARGET_FRAGMENT_COUNT		5


//++ PrintCopyright
VOID PrintCopyright()
{
	_tprintf(
		_T( "\n" )
		_T( "Fragmentation Tool\n" )
		_T( "(c)2017, Marius Negrutiu. All rights reserved.\n" )
		_T( "\n" )
	);
}


//++ PrintSyntax
VOID PrintSyntax()
{
	TCHAR szFile[MAX_PATH], *pszFilename;

	szFile[0] = 0;
	GetModuleFileName( NULL, szFile, ARRAYSIZE( szFile ) );
	pszFilename = _tcsrchr( szFile, _T( '\\' ) );
	if (pszFilename) {
		pszFilename++;
	} else {
		pszFilename = szFile;
	}

	_tprintf(
		_T( "Syntax:\n" )
		_T( "  %s <Command> [Options] [@]File1 [[@]File2 ... [@]FileN]\n" )
		_T( "\n" )
		_T( "Commands:\n" )
		_T( "  analyze     Analyze fragmentation :/\n" )
		_T( "  defragment  Defragment files      :)\n" )
		_T( "  fragment    Fragment files        ;)\n" )
		_T( "  prompt      Interactive mode      :P\n" )
		_T( "\n" )
		_T( "Options:\n" )
		_T( "  /compact    When defragmenting, files are moved together to a contiguous disk area\n" )
		_T( "  /count N    When fragmenting, files are broken into this many fragments. Default is %d\n" )
		_T( "  /simulate   Conduct a dry run. Nothing is actually written to disk\n" )
		_T( "\n" )
		_T( "Files:\n" )
		_T( "  Filenames can be either files or directories (parsed recursively)\n")
		_T( "  Filenames prefixed by \"@\" are treated as textfile catalogs (one file per line)\n" )
		_T( "\n" )
		_T( "Examples:\n" )
		_T( "  %s analyze C:\\Dir\\File1 \"C:\\Dir with spaces\\File1\" \"@C:\\Dir with spaces\\Catalog.txt\"\n" )
		_T( "  %s defrag /compact /simulate \"@C:\\Dir with spaces\\FileCatalog.txt\"\n" )
		_T( "  %s defrag /prompt \"@C:\\Dir with spaces\\FileCatalog.txt\"\n" )
		_T( "  %s fragment /count 10 \"C:\\Dir with spaces\\File1.iso\"\n" )
		_T( "\n" ),
		pszFilename,
		DEFAULT_TARGET_FRAGMENT_COUNT,
		pszFilename, pszFilename, pszFilename, pszFilename
	);
}


//++ FormatError
LPTSTR FormatError( _In_ DWORD err, _Out_ LPTSTR pszError, _In_ ULONG iErrorLen )
{
	if (pszError && iErrorLen) {
		DWORD iLen = 0;
		pszError[0] = _T( '\0' );
		if ((iLen = FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), pszError, iErrorLen, NULL )) == 0) {
			if ((iLen = FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE, GetModuleHandle( _T( "ntdll" ) ), err, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), pszError, iErrorLen, NULL )) == 0) {
				/// Try others...
			}
		}
		if (iLen > 0) {
			/// Trim trailing noise
			for (iLen--; (iLen > 0) && (pszError[iLen] == _T( '.' ) || pszError[iLen] == _T( ' ' ) || pszError[iLen] == _T( '\r' ) || pszError[iLen] == _T( '\n' )); iLen--)
				pszError[iLen] = _T( '\0' );
			iLen++;
		}
	}
	return pszError;
}


//++ DefragLogging
VOID DefragLogging( _In_ LPVOID lpParam, _In_ LPCTSTR pszFmt, _In_ ... )
{
	va_list args;
	UNREFERENCED_PARAMETER( lpParam );
	va_start( args, pszFmt );
	_vtprintf( pszFmt, args );
	va_end( args );
}


//++ DefragTrace
BOOL DefragTrace( _In_ LPVOID lpParam, _In_ int iStep, _In_opt_ LPVOID pParam1, _In_opt_ LPVOID pParam2 )
{
	LPBOOL pbInteractive = (LPBOOL)lpParam;
	switch (iStep)
	{
		case DEFRAG_STEP_BEFORE_ANALYSIS:
			///_tprintf( _T( "----- {%hs} Start Analysis...\n" ), __FUNCTION__ );
			break;
		case DEFRAG_STEP_ANALYZE_FILE:
			///_tprintf( _T( "----- {%hs} Analyze %s\n" ), __FUNCTION__, (LPCTSTR)pParam1 );
			break;
		case DEFRAG_STEP_BEFORE_MOVE:
			///_tprintf( _T( "----- {%hs} Start moving {Flags:0x%x, Interactive:%s}\n" ), __FUNCTION__, ((PDEFRAG_OPTIONS)pParam1)->Flags, *pbInteractive ? _T( "true" ) : _T( "false" ) );
			if (*pbInteractive) {

				TCHAR szInput[256];
				PDEFRAG_OPTIONS pOptions = (PDEFRAG_OPTIONS)pParam1;
				assert( pOptions );

				_tprintf(
					_T( "\n" )
					_T( "Actions:\n" )
					_T( "  s         Toggle simulation ON or OFF\n" )
					_T( "  c         Toggle compacting ON or OFF (\"defragment\" only)\n" )
					_T( "  fc <N>    Set target fragment count (\"fragment\" only)\n" )
					_T( "\n" )
					_T( "  d         Start defragmenting files\n" )
					_T( "  f         Start fragmenting files\n" )
					_T( "  q         Quit\n" )
					_T( "\n" )
				);

				while( TRUE ) {
					
					_tprintf( _T( "Action (Simulate:%s, Compact:%s, TargetFragments:%u) : " ), pOptions->Flags & DEFRAG_FLAG_SIMULATE ? _T( "ON" ) : _T( "OFF" ), pOptions->Flags & DEFRAG_FLAG_COMPACT ? _T( "ON" ) : _T( "OFF" ), pOptions->TargetFragmentCount );
					
					_tscanf( _T( "%255s" ), szInput );
					if (CompareString( CP_ACP, NORM_IGNORECASE, szInput, -1, _T( "c" ), -1 ) == CSTR_EQUAL) {
						pOptions->Flags ^= DEFRAG_FLAG_COMPACT;
					} else if (CompareString( CP_ACP, NORM_IGNORECASE, szInput, -1, _T( "fc" ), -1 ) == CSTR_EQUAL) {
						_tscanf( _T( "%255s" ), szInput );
						pOptions->TargetFragmentCount = (ULONG)_tstol( szInput );
					} else if (CompareString( CP_ACP, NORM_IGNORECASE, szInput, -1, _T( "s" ), -1 ) == CSTR_EQUAL) {
						pOptions->Flags ^= DEFRAG_FLAG_SIMULATE;
					} else if (CompareString( CP_ACP, NORM_IGNORECASE, szInput, -1, _T( "d" ), -1 ) == CSTR_EQUAL) {
						pOptions->Flags &= ~DEFRAG_FLAG_FRAGMENT;
						return TRUE;	/// Continue defragmenting
					} else if (CompareString( CP_ACP, NORM_IGNORECASE, szInput, -1, _T( "f" ), -1 ) == CSTR_EQUAL) {
						pOptions->Flags |= DEFRAG_FLAG_FRAGMENT;
						return TRUE;	/// Continue fragmenting
					} else if (CompareString( CP_ACP, NORM_IGNORECASE, szInput, -1, _T( "q" ), -1 ) == CSTR_EQUAL) {
						return FALSE;	/// Abort everything
					} else {
						_tprintf( _T( "  Unknown action \"%s\"\n" ), szInput );
					}
				}
			}
			break;
		case DEFRAG_STEP_MOVE_FILE:
			///_tprintf( _T( "----- {%hs} Defragment %s (%I64u bytes)\n" ), __FUNCTION__, (LPCTSTR)pParam1, *(PLONG64)pParam2 );
			break;
		case DEFRAG_STEP_MOVE_EXTENT:
			///_tprintf( _T( "----- {%hs} Defragment %s extent (%I64u bytes)\n" ), __FUNCTION__, (LPCTSTR)pParam1, *(PLONG64)pParam2 );
			break;
	}
	return TRUE;
}


//++ _tmain
int __cdecl _tmain( _In_ int argc, _In_ _TCHAR* argv[], _In_ _TCHAR* envp[] )
{
#define COMMAND_NONE		0
#define COMMAND_ANALYZE		1
#define COMMAND_DEFRAG		2

	DWORD err = ERROR_SUCCESS;
	ULONG iCommand = COMMAND_NONE;
	DEFRAG_OPTIONS DefragOpt = {0};
	BOOL bInteractive = FALSE;
	int i, i0;
	PFILE_LIST pFileList = NULL;

	PrintCopyright();

	/// Allocate data
	pFileList = (PFILE_LIST)HeapAlloc( GetProcessHeap(), 0, sizeof( *pFileList ) );
	if (!pFileList)
		return GetLastError();
	pFileList->Count = 0;
	pFileList->ppszFiles[0] = NULL;

	/// Try to determine whether the first argument is the executable name, or a command (starting with / or -)
	/// If *not* a command, we'll skip it
	i0 = 0;
	if (argc > 0 && (argv[0][0] != _T( '/' )) && (argv[0][0] != _T( '-' )))
		i0 = 1;

	// Command line
	for (i = i0; i < argc; i++)
	{
		if (iCommand == COMMAND_NONE) {

			// Commands
			if (CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "analyze" ), -1 ) == CSTR_EQUAL) {

				iCommand = COMMAND_ANALYZE;

			} else if (CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "defrag" ), -1 ) == CSTR_EQUAL || CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "defragment" ), -1 ) == CSTR_EQUAL) {

				iCommand = COMMAND_DEFRAG;
				DefragOpt.Flags &= ~DEFRAG_FLAG_FRAGMENT;
				DefragOpt.TargetFragmentCount = 0;

			} else if (CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "fragment" ), -1 ) == CSTR_EQUAL) {

				iCommand = COMMAND_DEFRAG;
				DefragOpt.Flags |= DEFRAG_FLAG_FRAGMENT;
				DefragOpt.TargetFragmentCount = DEFAULT_TARGET_FRAGMENT_COUNT;

			} else if (CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "prompt" ), -1 ) == CSTR_EQUAL) {

				bInteractive = TRUE;
				iCommand = COMMAND_DEFRAG;
				DefragOpt.Flags &= ~DEFRAG_FLAG_FRAGMENT;
				DefragOpt.TargetFragmentCount = DEFAULT_TARGET_FRAGMENT_COUNT;

			} else {
				_tprintf( _T( "Warning: Unknown command \"%s\"\n" ), argv[i] );
			}

		} else {

			// Options
			if (argv[i][0] == _T( '-' ) || argv[i][0] == _T( '/' )) {

				if ((iCommand == COMMAND_DEFRAG) && !(DefragOpt.Flags & DEFRAG_FLAG_FRAGMENT) && (CompareString( CP_ACP, NORM_IGNORECASE, argv[i] + 1, -1, _T( "compact" ), -1 ) == CSTR_EQUAL)) {

					DefragOpt.Flags |= DEFRAG_FLAG_COMPACT;

				} else if ((iCommand == COMMAND_DEFRAG) && (CompareString( CP_ACP, NORM_IGNORECASE, argv[i] + 1, -1, _T( "count" ), -1 ) == CSTR_EQUAL) && (i + 1 < argc)) {

					DefragOpt.TargetFragmentCount = (ULONG)_tstol( argv[i + 1] );
					i++;

				} else if ((iCommand == COMMAND_DEFRAG) && (CompareString( CP_ACP, NORM_IGNORECASE, argv[i] + 1, -1, _T( "simulate" ), -1 ) == CSTR_EQUAL)) {

					DefragOpt.Flags |= DEFRAG_FLAG_SIMULATE;
				
				} else {
					_tprintf( _T( "Warning: Unexpected option \"%s\"\n" ), argv[i] );
				}

			} else {

				// Files
				if (argv[i][0] == _T( '@' )) {
					FileListAddCatalog( pFileList, argv[i] + 1 );
				} else {
					FileListAddFile( pFileList, argv[i] );
				}

			}
		}
	}	/// for
	_tprintf( _T( "\n" ) );


	// Execute command
	DefragOpt.fnLogging = DefragLogging;
	DefragOpt.lpLoggingParam = NULL;
	DefragOpt.fnTracing = DefragTrace;
	DefragOpt.lpTracingParam = &bInteractive;

	switch (iCommand)
	{
		case COMMAND_ANALYZE:
			err = DefragAnalyzeFiles( pFileList->ppszFiles, &DefragOpt, NULL );
			if (TRUE) {
				TCHAR szError[255];
				_tprintf( _T( "\n" ) );
				_tprintf( _T( "Error 0x%x \"%s\"\n" ), err, FormatError( err, szError, ARRAYSIZE( szError ) ) );
			}
			break;

		case COMMAND_DEFRAG:
			err = DefragMoveFiles( pFileList->ppszFiles, &DefragOpt, NULL );
			if (TRUE) {
				TCHAR szError[255];
				_tprintf( _T( "\n" ) );
				_tprintf( _T( "Error 0x%x \"%s\"\n" ), err, FormatError( err, szError, ARRAYSIZE( szError ) ) );
			}
			break;

		default:
			err = ERROR_INVALID_PARAMETER;
			PrintSyntax();
			_tprintf( _T( "ERROR: Invalid command\n" ) );
	}

	FileListDestroy( pFileList );
	HeapFree( GetProcessHeap(), 0, pFileList );

	// Pause
	if (bInteractive && (err != ERROR_REQUEST_ABORTED)) {
		_tprintf( _T( "Press any key to exit . . . " ) );
		_gettch();
		_tprintf( _T( "\n" ) );
	}

	return err;
}
