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
		_T( "  Defrag.exe <Command> [Options] <File list>\n" )
		_T( "\n" )
		_T( "Commands:\n" )
		_T( "  /analyze    Analyze fragmentation\n" )
		_T( "  /defrag     Defragment individual files\n" )
		_T( "\n" )
		_T( "Options:\n" )
		_T( "  /compact    In addition to defragmenting, files are moved close together to a contiguous disk area\n" )
		_T( "  /simulate   Conduct a dry run. Nothing is actually written to disk\n" )
		_T( "\n" )
		_T( "File list:\n" )
		_T( "  One or more files and/or directories\n" )
		_T( "  If prefixed by @, each file is treated as a textfile catalog (one file per line)\n" )
		_T( "\n" )
		_T( "Examples:\n" )
		_T( "  Defrag /analyze C:\\Dir\\File1 \"C:\\Dir with spaces\\File1\" \"@C:\\Dir with spaces\\Catalog.txt\"\n" )
		_T( "  Defrag /defrag /compact /simulate \"@C:\\Dir with spaces\\Catalog.txt\"\n" )
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


//++ _tmain
int __cdecl _tmain( _In_ int argc, _In_ _TCHAR* argv[], _In_ _TCHAR* envp[] )
{
#define COMMAND_NONE		0
#define COMMAND_ANALYZE		1
#define COMMAND_DEFRAG		2

	DWORD err = ERROR_SUCCESS;
	ULONG iCommand = COMMAND_NONE;
	ULONG iDefragFlags = 0;
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
			CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "/compact" ), -1 ) == CSTR_EQUAL ||
			CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "-compact" ), -1 ) == CSTR_EQUAL))
		{
			iDefragFlags |= DEFRAG_FLAG_COMPACT;

		} else if ((iCommand == COMMAND_DEFRAG) && (
			CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "/simulate" ), -1 ) == CSTR_EQUAL ||
			CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "-simulate" ), -1 ) == CSTR_EQUAL))
		{
			iDefragFlags |= DEFRAG_FLAG_SIMULATE;

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
	switch (iCommand)
	{
		case COMMAND_ANALYZE:
			err = DefragAnalyzeFiles( FileList.ppszFiles, NULL, DefragLogging, NULL );
			if (TRUE) {
				TCHAR szError[255];
				_tprintf( _T( "\n" ) );
				_tprintf( _T( "Error 0x%x \"%s\"\n" ), err, FormatError( err, szError, ARRAYSIZE( szError ) ) );
			}
			break;

		case COMMAND_DEFRAG:
			err = DefragDefragmentFiles( FileList.ppszFiles, iDefragFlags, NULL, DefragLogging, NULL );
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
