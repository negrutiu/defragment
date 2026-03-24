
// Marius Negrutiu
// Jan 2017

#pragma once
#ifndef _DEFRAGMENT_H_
#define _DEFRAGMENT_H_

#include "windows.h"

#ifdef __cplusplus
extern "C" {
#endif

/// \brief Fragmentation analysis results.
typedef struct {
    ULONG FileCount;            ///< Number of files.
    ULONG64 TotalSize;          ///< Total size of all files in bytes.
    ULONG64 MinFileFragments;   ///< The least fragmented file (1 means no fragments, 2 means 2 fragments, etc.)
    ULONG64 MaxFileFragments;   ///< The most fragmented file.
    ULONG64 ClusterCount;       ///< Total number of clusters used by all files.
    ULONG64 ExtentCount;        ///< Total number of extents (fragments) used by all files.
    ULONG64 DiffuseExtentCount; ///< Number of extents that are not adjacent to any other extent of the same file.
} DEFRAG_ANALYSIS, *PDEFRAG_ANALYSIS;


/// \brief File moving results/statistics.
typedef struct {
    ULONG64 FileCount;    ///< Number of files moved.
    ULONG64 TotalSize;    ///< Number of bytes moved.
    ULONG64 ClusterCount; ///< Number of clusters moved.
} DEFRAG_MOVE, *PDEFRAG_MOVE;


/// \brief Logging callback function, compatible with \c _tprintf format.
typedef VOID (*DefragmentLoggingCallback)(_In_ LPVOID lpParam, _In_ LPCTSTR pszFmt, _In_...);


/// \brief Fragmentation analysis is about to begin (param1:\c NULL, param2:\c NULL)
#define DEFRAG_STEP_BEFORE_ANALYSIS 1
/// \brief A file is about to be analyzed (param1:(\c LPCTSTR)pszFile, param2:\c NULL)
#define DEFRAG_STEP_ANALYZE_FILE 2
/// \brief File moving is about to begin (param1:(\c PDEFRAG_OPTIONS)pOptions, param2:\c NULL)
#define DEFRAG_STEP_BEFORE_MOVE 3
/// \brief A file or file fragment is about to be moved (param1:(\c LPCTSTR)pszFile, param2:(\c PLONG64)piSize)
#define DEFRAG_STEP_MOVE_FILE 4
/// \brief A file extent is about to be moved (param1:(\c LPCTSTR)pszFile, param2:(\c PLONG64)piExtentSize)
#define DEFRAG_STEP_MOVE_EXTENT 5

/// \brief Tracing callback function, called at various steps to provide progress information.
/// \returns \c TRUE to continue, \c FALSE to abort the entire operation.
typedef BOOL (*DefragmentTraceCallback)(_In_ LPVOID lpParam, _In_ int iStep, _In_opt_ LPVOID pParam1, _In_opt_ LPVOID pParam2);


/// \brief Move files together to a contiguous disk area. Mutually exclusive with \c DEFRAG_FLAG_FRAGMENT.
#define DEFRAG_FLAG_COMPACT (1UL << 0)
/// \brief Fragment instead of defragmenting. Mutually exclusive with \c DEFRAG_FLAG_COMPACT.
#define DEFRAG_FLAG_FRAGMENT (1UL << 1)
/// \brief Dry run, nothing is actually written to disk.
#define DEFRAG_FLAG_SIMULATE (1UL << 31)

/// \brief Options for analysis and de/fragmentation.
typedef struct {
    ULONG Flags;                         ///< Combination of DEFRAG_FLAG_XXX.
    ULONG TargetFragmentCount;           ///< Break files into this many fragments when fragmenting. Ignored when defragmenting.
    DefragmentLoggingCallback fnLogging; ///< Optional logging function.
    LPVOID lpLoggingParam;               ///< Optional logging function custom parameter.
    DefragmentTraceCallback fnTracing;   ///< Optional tracing (progress) function.
    LPVOID lpTracingParam;               ///< Optional tracing function custom parameter.
} DEFRAG_OPTIONS, *PDEFRAG_OPTIONS;


/// \brief Analyze fragmentation of files.
/// \details Files must be located on the same volume.
/// \param ppszFiles Array of \c LPCTSTR file patterns. Wildcards are allowed. The last entry must be \c NULL.
/// \param pIn Input options.
/// \param pOut Output analysis results.
DWORD DefragAnalyzeFiles(_In_ LPCTSTR *ppszFiles, _In_opt_ PDEFRAG_OPTIONS pIn, _Out_opt_ PDEFRAG_ANALYSIS pOut);


/// \brief Move (defragment or fragment) files.
/// \details Files must be located on the same volume.
///   If files are already de/fragmented, the function returns \c ERROR_SUCCESS and \p pOut indicates no data movement.
/// \param ppszFiles Array of \c LPCTSTR file patterns. Wildcards are allowed. The last entry must be \c NULL.
/// \param pIn Input options.
/// \param pOut Output move results/statistics.
/// \return Win32 error code.
///   De/Fragmenting may fail when there's intense disk activity while moving the fragments.
///   In such case it's recommended to retry multiple times until success.
DWORD DefragMoveFiles(_In_ LPCTSTR *ppszFiles, _In_opt_ PDEFRAG_OPTIONS pIn, _Out_opt_ PDEFRAG_MOVE pOut);


#ifdef __cplusplus
}
#endif

#endif
