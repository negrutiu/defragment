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
		_T( "  Defrag.exe <Command> <File list>\n" )
		_T( "\n" )
		_T( "Commands:\n" )
		_T( "  /analyze    Analyze files' fragmentation\n" )
		_T( "  /defrag     Defragment individual files\n" )
		_T( "  /compact    Defragment and move files to a contiguous disk location\n" )
		_T( "\n" )
		_T( "File list:\n" )
		_T( "  One or more files and/or directories\n" )
		_T( "  If prefixed by @, each file is treated as a textfile catalog (one file per line)\n" )
		_T( "\n" )
		_T( "Examples:\n" )
		_T( "  Defrag /analyze C:\\Dir\\File1 \"C:\\Dir with spaces\\File1\" \"@C:\\Dir with spaces\\Catalog.txt\"\n" )
		_T( "  Defrag /defrag  C:\\Dir\\File1 \"C:\\Dir with spaces\\File1\" \"@C:\\Dir with spaces\\Catalog.txt\"\n" )
		_T( "  Defrag /compact C:\\Dir\\File1 \"C:\\Dir with spaces\\File1\" \"@C:\\Dir with spaces\\Catalog.txt\"\n" )
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
#define COMMAND_COMPACT		3

	DWORD err = ERROR_SUCCESS;
	ULONG iCommand = COMMAND_NONE;
	int i;
	FILE_LIST FileList = {0};

	PrintCopyright();

	// Command line
	for (i = 0; i < argc; i++)
	{
		if (iCommand != COMMAND_NONE) {
			
			if (argv[i][0] == _T( '@' )) {
				FileListAddCatalog( &FileList, argv[i] + 1 );
			} else {
				FileListAddFile( &FileList, argv[i] );
			}

		} else {

			if (CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "/analyze" ), -1 ) == CSTR_EQUAL ||
				CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "-analyze" ), -1 ) == CSTR_EQUAL)
			{
				iCommand = COMMAND_ANALYZE;
			}
			else if (CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "/defrag" ), -1 ) == CSTR_EQUAL ||
					 CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "-defrag" ), -1 ) == CSTR_EQUAL ||
					 CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "/defragment" ), -1 ) == CSTR_EQUAL ||
					 CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "-defragment" ), -1 ) == CSTR_EQUAL)
			{
				iCommand = COMMAND_DEFRAG;
			}
			else if (CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "/compact" ), -1 ) == CSTR_EQUAL ||
					 CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "-compact" ), -1 ) == CSTR_EQUAL)
			{
				iCommand = COMMAND_COMPACT;
			}
			else
			{
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
			err = DefragDefragmentFiles( FileList.ppszFiles, FALSE, NULL, DefragLogging, NULL );
			if (TRUE) {
				TCHAR szError[255];
				_tprintf( _T( "\n" ) );
				_tprintf( _T( "Error 0x%x \"%s\"\n" ), err, FormatError( err, szError, ARRAYSIZE( szError ) ) );
			}
			break;

		case COMMAND_COMPACT:
			err = DefragDefragmentFiles( FileList.ppszFiles, TRUE, NULL, DefragLogging, NULL );
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
