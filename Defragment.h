
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
	ULONG FileCount;
	ULONG64 TotalSize;							/// Size of all files
	ULONG64 MaxFileFragments;					/// Most fragmented file
	ULONG64 ClusterCount;						/// Overall number of clusters
	ULONG64 ExtentCount;						/// Overall number of extents (fragments)
	ULONG64 DiffuseExtentCount;					/// Overall number of extents that are not contiguous
} DEFRAG_ANALYSIS, *PDEFRAG_ANALYSIS;


//+ struct DEFRAG_DEFRAGMENT
typedef struct {
	ULONG64 FileCount;							/// Number of defragmented files
	ULONG64 TotalSize;							/// Size of data moved during defragmentation
	ULONG64 ClusterCount;						/// Clusters moved during defragmentation
} DEFRAG_DEFRAGMENT, *PDEFRAG_DEFRAGMENT;


/// Logging Callback
typedef VOID( *DefragmentLoggingCallback )(_In_ LPVOID lpParam, _In_ LPCTSTR pszFmt, _In_ ...);


//+ Analyze fragmentation
/// All files must be located on the same volume
DWORD DefragAnalyzeFiles(
	_In_ LPCTSTR *ppszFiles,								/// Array of LPCTSTRs. The last entry must be NULL
	_Out_opt_ PDEFRAG_ANALYSIS pOut,
	_In_opt_ DefragmentLoggingCallback fnLogging,
	_In_opt_ LPVOID lpLoggingParam
);


#define DEFRAG_FLAG_COMPACT			(1 << 0)				/// In addition to defragmenting, move the files close together to a contiguous disk area
#define DEFRAG_FLAG_SIMULATE		(1 << 1)				/// Dry run, nothing is actually written to disk


//+ Defragment files
/// All files must be located on the same volume
/// If files are already defragmented, the function returns ERROR_SUCCESS and pOut will indicate no data being moved
/// Be prepared for this function to fail if there's intense disk activity while defragmenting
/// In such case it is recommended to retry later
DWORD DefragDefragmentFiles(
	_In_ LPCTSTR *ppszFiles,								/// Array of LPCTSTRs. The last entry must be NULL
	_In_ ULONG iFlags,										/// Combination of DEFRAG_FLAG_*
	_Out_opt_ PDEFRAG_DEFRAGMENT pOut,
	_In_opt_ DefragmentLoggingCallback fnLogging,
	_In_opt_ LPVOID lpLoggingParam
);


#ifdef __cplusplus
}
#endif

#endif /// _DEFRAGMENT_H_