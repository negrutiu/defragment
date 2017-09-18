
// Marius Negrutiu
// Jan 2017

#include "StdAfx.h"
#include "FileList.h"


//++ FileListPathHasWildcards
BOOL FileListPathHasWildcards( _In_ LPCTSTR pszPath )
{
	if (pszPath) {
		for (; *pszPath; pszPath++)
			if (*pszPath == _T( '*' ) || *pszPath == _T( '?' ))
				return TRUE;
	}
	return FALSE;
}


//++ FileListAddPattern
BOOL FileListAddPattern( _Inout_ PFILE_LIST pList, _In_ LPTSTR pszPath, _In_ size_t iDirMaxLen, _In_ size_t iDirLen )
{
	if (pList && pszPath && *pszPath) {

		LPTSTR psz = pszPath + iDirLen;
		size_t len = iDirMaxLen - iDirLen;
		WIN32_FIND_DATA fd = {0};
		HANDLE h;

#if _DEBUG || DBG
		_tprintf( _T( "[d] %hs( %s )\n" ), __FUNCTION__, pszPath );
#endif

		for (; (psz > pszPath) && (*psz != _T( '\\' )); psz--, len--);		/// Point to the last backslash

		h = FindFirstFile( pszPath, &fd );
		if (h != INVALID_HANDLE_VALUE) {
			do {
				if (CompareString( CP_ACP, NORM_IGNORECASE, fd.cFileName, -1, _T( "." ), -1 ) != CSTR_EQUAL &&
					CompareString( CP_ACP, NORM_IGNORECASE, fd.cFileName, -1, _T( ".." ), -1 ) != CSTR_EQUAL)
				{
					StringCchPrintf( psz, len, _T( "\\%s" ), fd.cFileName );
					FileListAddFile( pList, pszPath );
				}
			} while (FindNextFile( h, &fd ));

			return TRUE;
		}
	}
	return FALSE;
}


//++ FileListAddFile
BOOL FileListAddFile( _Inout_ PFILE_LIST pList, _In_ LPCTSTR pszFile )
{
	if (pList && pszFile && *pszFile) {
		if (pList->Count < FILE_LIST_MAX_COUNT) {
			
			DWORD Attr = GetFileAttributes( pszFile );
			if (Attr != INVALID_FILE_ATTRIBUTES) {

				if (Attr & FILE_ATTRIBUTE_DIRECTORY) {

					// Directory
					TCHAR szPath[1024];
					LPTSTR psz;
					size_t len;

					StringCchCopyEx( szPath, ARRAYSIZE( szPath ), pszFile, &psz, &len, 0 );
					for (; (psz > szPath) && (*(psz - 1) == _T( '\\' ) || *(psz - 1) == _T( '/' )); *(--psz) = _T( '\0' ), len++);
					StringCchCopyEx( psz, len, _T( "\\*.*" ), &psz, &len, 0 );		/// Convert to Dir\*.*
					len = ARRAYSIZE( szPath ) - len;

					return FileListAddPattern( pList, szPath, ARRAYSIZE( szPath ), len );
				
				} else {

					// File
					pList->ppszFiles[pList->Count++] = _tcsdup( pszFile );

					if (pList->Count < FILE_LIST_MAX_COUNT)
						pList->ppszFiles[pList->Count] = NULL;		/// Make sure the last element in list in NULL

#if _DEBUG || DBG
					_tprintf( _T( "[d] %hs( %s )\n" ), __FUNCTION__, pszFile );
#endif
				}
				return TRUE;

			} else {

				DWORD err = GetLastError();

				// Path with wildcards
				if (FileListPathHasWildcards( pszFile )) {

					// Directory
					TCHAR szPath[1024];
					LPTSTR psz;
					size_t len;

					StringCchCopyEx( szPath, ARRAYSIZE( szPath ), pszFile, &psz, &len, 0 );
					len = ARRAYSIZE( szPath ) - len;

					return FileListAddPattern( pList, szPath, ARRAYSIZE( szPath ), len );

				} else {
#if _DEBUG || DBG
					_tprintf( _T( "[d] GetFileAttributes( %s ) == 0x%x\n" ), pszFile, err );
#endif
				}
			}
		}
	}
	return FALSE;
}


//++ FileListAddCatalog
BOOL FileListAddCatalog( _Inout_ PFILE_LIST pList, _In_ LPCTSTR pszCatalog )
{
	if (pList && pszCatalog && *pszCatalog) {

		FILE *f = _tfopen( pszCatalog, _T( "rS, ccs=UTF-8" ) );
#if _DEBUG || DBG
		_tprintf( _T( "[d] %hs( %s )\n" ), __FUNCTION__, pszCatalog );
#endif
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

					/// Add to list
					FileListAddFile( pList, szBuf2 );
				}
			}

			fclose( f );
			return TRUE;
		}
	}
	return FALSE;
}


//++ FileListDestroy
BOOL FileListDestroy( _Inout_ PFILE_LIST pList )
{
	if (pList) {
		ULONG i;
		for (i = 0; (i < FILE_LIST_MAX_COUNT) && (pList->ppszFiles[i] != NULL); i++)
			free( (LPVOID)pList->ppszFiles[i] );
		ZeroMemory( pList, sizeof( *pList ) );
		return TRUE;
	}
	return FALSE;
}
