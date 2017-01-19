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


//++ EnableWow64FsRedirection
BOOLEAN EnableWow64FsRedirection( IN BOOLEAN bEnable )
{
	BOOLEAN ret = FALSE;
	typedef BOOL( WINAPI *FNIsWow64Process )(HANDLE, PBOOL);
	FNIsWow64Process fnIsWow64Process = NULL;

	// only enable/disable redirection if we're running in WOW64
	fnIsWow64Process = (FNIsWow64Process)GetProcAddress( GetModuleHandle( TEXT( "kernel32.dll" ) ), "IsWow64Process" );
	if (fnIsWow64Process) {
		BOOL bWow64 = FALSE;
		if (fnIsWow64Process( GetCurrentProcess(), &bWow64 ) && bWow64) {
			// enable/disable file system redirection
			typedef BOOLEAN( WINAPI *FNEnableWow64FsRedir )(BOOLEAN);
			FNEnableWow64FsRedir fnEnableWow64FsRedir = (FNEnableWow64FsRedir)GetProcAddress( GetModuleHandle( TEXT( "kernel32.dll" ) ), "Wow64EnableWow64FsRedirection" );
			if (fnEnableWow64FsRedir) {
				if (fnEnableWow64FsRedir( bEnable )) {
					ret = TRUE;
				}
			}
		}
	}
	return ret;
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


//++ _tmain
int __cdecl _tmain( __in int argc, __in _TCHAR* argv[], __in _TCHAR* envp[] )
{
	DWORD err = ERROR_SUCCESS;

	PrintSyntax();
	EnableWow64FsRedirection( FALSE );			/// Disable file system redirection in WOW64 processes

	if (argc == 2) {
		ULONG64 iBytesMoved;
		LPCTSTR ppszFileList[255] = {0};

		err = ImportFileList( argv[1], ppszFileList );
		if (err == ERROR_SUCCESS) {

			err = CompactFiles( (LPCTSTR*)ppszFileList, &iBytesMoved );
			if (err == ERROR_SUCCESS) {
				
				// Success

			} else {
				_tprintf( _T( "\nERROR: Error 0x%x\n" ), err );
			}
		} else {
			_tprintf( _T( "\nERROR: Error 0x%x reading from file \"%s\"\n" ), err, argv[1] );
		}
		DestroyFileList( ppszFileList );

	} else {
		_tprintf( _T( "\nERROR: No input specified\n" ) );
	}

	return err;
}
