#include "StdAfx.h"
#include "Defragment.h"


//++ PrintSyntax
VOID PrintSyntax()
{
	_tprintf(
		_T( "\n" )
		_T( "Defragment and compact files\n" )
		_T( "(c)2017, Marius Negrutiu. All rights reserved.\n" )
		_T( "\n" )
		_T( "Syntax:\n" )
		_T( "  Defrag.exe <Files.txt>\n" )
		_T( "\n" )
		_T( "Files.txt is a text file containing a list of files (one file per line) to be defragmented\n" )
		_T( "Files will be defragmented and moved close together to a contiguous disk location\n" )
		_T( "\n" )
		);
}


//++ ImportFileList
DWORD ImportFileList( _In_ LPCTSTR pszFile, _Out_ LPCTSTR ppszFileList[255] )
{
	DWORD err = ERROR_SUCCESS;
	FILE *f = _tfopen( pszFile, _T( "rS, ccs=UTF-8" ) );
	if (f) {

		TCHAR szBuf[512];
		TCHAR szBuf2[512];
		size_t Len;
		ULONG FileListIndex = 0;

		while (_fgetts( szBuf, ARRAYSIZE( szBuf ), f ) != NULL) {
			if (SUCCEEDED( StringCchLength( szBuf, ARRAYSIZE( szBuf ), &Len ) )) {
				
				///	Remove trailing EOLN
				if (Len > 0 && szBuf[Len - 1] == _T( '\n' ))
					szBuf[--Len] = _T( '\0' );

				/// Expand environment variables
				ExpandEnvironmentStrings( szBuf, szBuf2, ARRAYSIZE( szBuf2 ) );

				/// Clone and store
				ppszFileList[FileListIndex++] = _tcsdup( szBuf2 );
			}
		}
		ppszFileList[FileListIndex++] = NULL;		/// Last string is NULL
		
		fclose( f );

	} else {
		err = ERROR_FILE_CORRUPT;
	}
	return err;
}


//++ DestroyFileList
DWORD DestroyFileList( _Out_ LPCTSTR ppszFileList[255] )
{
	DWORD err = ERROR_SUCCESS;
	ULONG i;
	for (i = 0; (i < 255) && (ppszFileList[i] != NULL); i++) {
		free( (LPVOID)ppszFileList[i] );
		ppszFileList[i] = NULL;
	}
	return err;
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
	DWORD err = ERROR_SUCCESS;

	PrintSyntax();

	if (argc == 2) {
		ULONG64 iBytesMoved;
		LPCTSTR ppszFileList[255] = {0};

		err = ImportFileList( argv[1], ppszFileList );
		if (err == ERROR_SUCCESS) {

			err = CompactFiles( (LPCTSTR*)ppszFileList, &iBytesMoved, DefragLogging, NULL );
			if (err == ERROR_SUCCESS) {
				
				// Success
				DefragLogging( NULL, _T( "\n" ) );
				DefragLogging( NULL, _T( "Success (%I64u bytes moved)\n" ), iBytesMoved );

			} else {
				TCHAR szError[255];
				DefragLogging( NULL, _T( "\n" ) );
				DefragLogging( NULL, _T( "ERROR: 0x%x \"%s\"\n" ), err, FormatError( err, szError, ARRAYSIZE( szError ) ) );
			}
		} else {
			TCHAR szError[255];
			DefragLogging( NULL, _T( "\n" ) );
			DefragLogging( NULL, _T( "ERROR: Failed to read from \"%s\". Error 0x%x \"%s\"\n" ), argv[1], err, FormatError( err, szError, ARRAYSIZE( szError ) ) );
		}
		DestroyFileList( ppszFileList );

	} else {
		DefragLogging( NULL, _T( "\n" ) );
		DefragLogging( NULL, _T( "ERROR: No input specified\n" ) );
	}

	return err;
}
