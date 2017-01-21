
// Marius Negrutiu
// Jan 2017

#pragma once
#ifndef _DEFRAGMENT_H_
#define _DEFRAGMENT_H_

#ifdef __cplusplus
extern "C" {
#endif


//+ struct DEFRAG_ANALYSIS
typedef struct {
	BOOL Dirty;									/// Set after defragmentation. If TRUE, all analysis data must be treated as obsolete. A new analysis should be performed
	ULONG FileCount;							/// Count of all files
	ULONG64 TotalSize;							/// Combined file sizes
	ULONG64 MaxFileFragments;					/// Most fragmented file
	ULONG64 ClusterCount;						/// Overall number of clusters
	ULONG64 ExtentCount;						/// Overall number of extents (fragments)
	ULONG64 DiffuseExtentCount;					/// Overall number of extents that are not contiguous
} DEFRAG_ANALYSIS, *PDEFRAG_ANALYSIS;


//+ struct DEFRAG_DEFRAGMENT
typedef struct {
	ULONG64 FileCount;
	ULONG64 TotalSize;
	ULONG64 ClusterCount;
} DEFRAG_DEFRAGMENT, *PDEFRAG_DEFRAGMENT;


// Callback useful for logging
typedef VOID( *DefragmentLoggingCallback )(_In_ LPVOID lpParam, _In_ LPCTSTR pszFmt, _In_ ...);


// Analyze files' fragmentation
// Files must be located on the same volume
DWORD DefragAnalyzeFiles(
	_In_ LPCTSTR *ppszFiles,
	_Out_opt_ PDEFRAG_ANALYSIS pOut,
	_In_opt_ DefragmentLoggingCallback fnLogging,
	_In_opt_ LPVOID lpLoggingParam
);


// Defragment multiple files
// Files must be located on the same volume
// If files are already compact, the function returns ERROR_SUCCESS and BytesMoved will be zero
// Be prepared for this function to fail if there's intense disk activity while defragmenting
// In such case it is recommended to retry at a later time
DWORD DefragDefragmentFiles(
	_In_ LPCTSTR *ppszFiles,
	_In_ BOOL bCompact,										/// If TRUE, all files will be compacted (moved together to a contiguous disk location)
	_Out_opt_ PDEFRAG_DEFRAGMENT pOut,
	_In_opt_ DefragmentLoggingCallback fnLogging,
	_In_opt_ LPVOID lpLoggingParam
);

#ifdef __cplusplus
}
#endif

#endif /// _DEFRAGMENT_H_