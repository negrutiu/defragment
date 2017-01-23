#include "StdAfx.h"
#include "Defragment.h"
#include "FileList.h"


//++ PrintCopyright
VOID PrintCopyright()
{
	_tprintf(
		_T( "\n" )
		_T( "Defragment and compact files\n" )
		_T( "(c)2017, Marius Negrutiu. All rights reserved.\n" )
		_T( "\n" )
	);
}


//++ PrintSyntax
VOID PrintSyntax()
{
	_tprintf(
		_T( "Syntax:\n" )
		_T( "  Defrag.exe <Command> [Options] [@]File1 [[@]File2] ... [[@]FileN]\n" )
		_T( "\n" )
		_T( "Commands:\n" )
		_T( "  /analyze    Analyze fragmentation\n" )
		_T( "  /defrag     Defragment files\n" )
		_T( "\n" )
		_T( "Options:\n" )
		_T( "  /compact    In addition to defragmenting, files are moved close together to a contiguous disk area\n" )
		_T( "  /simulate   Conduct a dry run. Nothing is actually written to disk\n" )
		_T( "  /prompt     Enable interactive mode. Prompt before defragmenting\n" )
		_T( "\n" )
		_T( "Files:\n" )
		_T( "  Each filename can be either a file or directory. Directories are parsed recursively\n")
		_T( "  If a file is prefixed by @, it's treated as textfile catalog (one file per line)\n" )
		_T( "\n" )
		_T( "Examples:\n" )
		_T( "  Defrag /analyze C:\\Dir\\File1 \"C:\\Dir with spaces\\File1\" \"@C:\\Dir with spaces\\Catalog.txt\"\n" )
		_T( "  Defrag /defrag /compact /simulate \"@C:\\Dir with spaces\\FileCatalog.txt\"\n" )
		_T( "  Defrag /defrag /prompt \"@C:\\Dir with spaces\\FileCatalog.txt\"\n" )
		_T( "\n" )
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
		case DEFRAG_STEP_BEFORE_DEFRAGMENT:
			///_tprintf( _T( "----- {%hs} Start Defragmenting {Flags:0x%x, Interactive:%s}\n" ), __FUNCTION__, ((PDEFRAG_OPTIONS)pParam1)->Flags, *pbInteractive ? _T( "true" ) : _T( "false" ) );
			if (*pbInteractive) {

				TCHAR ch;
				PDEFRAG_OPTIONS pOptions = (PDEFRAG_OPTIONS)pParam1;
				assert( pOptions );

				_tprintf(
					_T( "\n" )
					_T( "Actions:\n" )
					_T( "  c   Toggle file compacting ON or OFF\n" )
					_T( "  s   Toggle simulation ON or OFF\n" )
					_T( "  d   Start defragmenting files\n" )
					_T( "  q   Quit\n" )
					_T( "\n" )
				);

				while( TRUE ) {
					
					_tprintf( _T( "Action (Compact:%s, Simulate:%s) : " ), pOptions->Flags & DEFRAG_FLAG_COMPACT ? _T( "ON" ) : _T( "OFF" ), pOptions->Flags & DEFRAG_FLAG_SIMULATE ? _T( "ON" ) : _T( "OFF" ) );
					
					ch = _gettche();
					if (ch == 0 || ch == 0xe0)
						ch = _gettche();		/// When reading a function key or an arrow key, each function must be called twice; the first call returns 0 or 0xE0, and the second call returns the actual key code
					if (_istupper( ch ))
						ch = (TCHAR)tolower( ch );
					_tprintf( _T( "\n" ) );

					if (ch == _T( 'c' )) {
						pOptions->Flags ^= DEFRAG_FLAG_COMPACT;
					} else if (ch == _T( 's' )) {
						pOptions->Flags ^= DEFRAG_FLAG_SIMULATE;
					} else if (ch == _T( 'd' )) {
						return TRUE;	/// Continue defragmenting
					} else if (ch == _T( 'q' )) {
						return FALSE;	/// Abort everything
					} else {
						_tprintf( _T( "  Unknown action \"%c\"\n" ), ch );
					}
				}
			}
			break;
		case DEFRAG_STEP_DEFRAGMENT_FILE:
			///_tprintf( _T( "----- {%hs} Defragment %s (%I64u bytes)\n" ), __FUNCTION__, (LPCTSTR)pParam1, *(PLONG64)pParam2 );
			break;
		case DEFRAG_STEP_DEFRAGMENT_EXTENT:
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
	FILE_LIST FileList = {0};

	PrintCopyright();

	/// Try to determine whether the first argument is the executable name, or a command (starting with / or -)
	/// If *not* a command, we'll skip it
	i0 = 0;
	if (argc > 0 && (argv[0][0] != _T( '/' )) && (argv[0][0] != _T( '-' )))
		i0 = 1;

	// Command line
	for (i = i0; i < argc; i++)
	{
		if ((iCommand == COMMAND_NONE) && (
			CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "/analyze" ), -1 ) == CSTR_EQUAL ||
			CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "-analyze" ), -1 ) == CSTR_EQUAL))
		{
			iCommand = COMMAND_ANALYZE;

		} else if ((iCommand == COMMAND_NONE) && (
			CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "/defrag" ), -1 ) == CSTR_EQUAL ||
			CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "-defrag" ), -1 ) == CSTR_EQUAL ||
			CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "/defragment" ), -1 ) == CSTR_EQUAL ||
			CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "-defragment" ), -1 ) == CSTR_EQUAL))
		{
			iCommand = COMMAND_DEFRAG;

		} else if ((iCommand == COMMAND_DEFRAG) && (
			CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "/prompt" ), -1 ) == CSTR_EQUAL ||
			CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "-prompt" ), -1 ) == CSTR_EQUAL))
		{
			bInteractive = TRUE;

		} else if ((iCommand == COMMAND_DEFRAG) && (
			CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "/compact" ), -1 ) == CSTR_EQUAL ||
			CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "-compact" ), -1 ) == CSTR_EQUAL))
		{
			DefragOpt.Flags |= DEFRAG_FLAG_COMPACT;

		} else if ((iCommand == COMMAND_DEFRAG) && (
			CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "/simulate" ), -1 ) == CSTR_EQUAL ||
			CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "-simulate" ), -1 ) == CSTR_EQUAL))
		{
			DefragOpt.Flags |= DEFRAG_FLAG_SIMULATE;

		} else {

		if (iCommand != COMMAND_NONE) {
			
				/// File list
			if (argv[i][0] == _T( '@' )) {
				FileListAddCatalog( &FileList, argv[i] + 1 );
			} else {
				FileListAddFile( &FileList, argv[i] );
			}

		} else {
				_tprintf( _T( "Warning: Unknown command \"%s\"\n" ), argv[i] );
			}
		}
	}
	_tprintf( _T( "\n" ) );


	// Execute command
	DefragOpt.fnLogging = DefragLogging;
	DefragOpt.lpLoggingParam = NULL;
	DefragOpt.fnTracing = DefragTrace;
	DefragOpt.lpTracingParam = &bInteractive;

	switch (iCommand)
	{
		case COMMAND_ANALYZE:
			err = DefragAnalyzeFiles( FileList.ppszFiles, &DefragOpt, NULL );
			if (TRUE) {
				TCHAR szError[255];
				_tprintf( _T( "\n" ) );
				_tprintf( _T( "Error 0x%x \"%s\"\n" ), err, FormatError( err, szError, ARRAYSIZE( szError ) ) );
			}
			break;

		case COMMAND_DEFRAG:
			err = DefragDefragmentFiles( FileList.ppszFiles, &DefragOpt, NULL );
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

	FileListDestroy( &FileList );
	return err;
}
