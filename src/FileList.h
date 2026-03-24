
// Marius Negrutiu
// Jan 2017

#pragma once
#ifndef _FILELIST_H_
#define _FILELIST_H_

/// \brief Maximum number of files in the list. This is an arbitrary limit to prevent excessive memory usage.
#define FILE_LIST_MAX_COUNT 2048000


/// \brief A simple structure to hold an array of file paths.
typedef struct {
    LPCTSTR ppszFiles[FILE_LIST_MAX_COUNT + 1];
    LONG Count;
} FILE_LIST, *PFILE_LIST;


/// \brief Expand a file pattern and append the matching files to the list.
/// \param pList The file list to append to.
/// \param pszPath The file pattern to expand. Can include wildcards. Directories are expanded recursively.
BOOL FileListAddFile(_Inout_ PFILE_LIST pList, _In_ LPCTSTR pszFile);


/// \brief Read a catalog file and call \c FileListAddFile for each file pattern in the catalog.
/// \details The catalog file is a text file with one file pattern per line.
///   Environment variables in the file patterns are expanded.
BOOL FileListAddCatalog(_Inout_ PFILE_LIST pList, _In_ LPCTSTR pszCatalog);


/// \brief Free all memory allocated for the file list and reset it to an empty state.
BOOL FileListDestroy(_Inout_ PFILE_LIST pList);

#endif
