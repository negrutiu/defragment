
// Marius Negrutiu
// Jan 2017

#pragma once
#ifndef _FILELIST_H_
#define _FILELIST_H_

#define FILE_LIST_MAX_COUNT 1024

typedef struct {
	LPCTSTR ppszFiles[FILE_LIST_MAX_COUNT];
	LONG Count;
} FILE_LIST, *PFILE_LIST;


BOOL FileListAddFile( _Inout_ PFILE_LIST pList, _In_ LPCTSTR pszFile );
BOOL FileListAddCatalog( _Inout_ PFILE_LIST pList, _In_ LPCTSTR pszCatalog );

BOOL FileListDestroy( _Inout_ PFILE_LIST pList );

#endif /// _FILELIST_H_