
// Marius Negrutiu
// Jan 2017

#pragma once
#ifndef _DEFRAGMENT_H_
#define _DEFRAGMENT_H_

#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
	ULONG FileCount;
	ULONG64 TotalSize;							// Size of all files
	ULONG64 MinFileFragments;					// Least fragmented file
	ULONG64 MaxFileFragments;					// Most fragmented file
	ULONG64 ClusterCount;						// Overall number of clusters
	ULONG64 ExtentCount;						// Overall number of extents (fragments)
	ULONG64 DiffuseExtentCount;					// Overall number of extents that are not contiguous
} DEFRAG_ANALYSIS, *PDEFRAG_ANALYSIS;


typedef struct {
	ULONG64 FileCount;							// Files moved
	ULONG64 TotalSize;							// Bytes moved
	ULONG64 ClusterCount;						// Clusters moved
} DEFRAG_MOVE, *PDEFRAG_MOVE;


#define DEFRAG_FLAG_COMPACT			(1 << 0)	// In addition to defragmenting, move the files close together to a contiguous disk area
#define DEFRAG_FLAG_FRAGMENT		(1 << 1)	// Fragment instead of defragmenting. Mutually exclusive with DEFRAG_FLAG_COMPACT. Useful for debugging ;)
#define DEFRAG_FLAG_SIMULATE		(1 << 31)	// Dry run, nothing is actually written to disk

// Logging Callback
typedef VOID( *DefragmentLoggingCallback )(_In_ LPVOID lpParam, _In_ LPCTSTR pszFmt, _In_ ...);

// Tracing Callback
#define DEFRAG_STEP_BEFORE_ANALYSIS		1		// Analysis is about to start			{ pParam1:NULL, pParam2:NULL }
#define DEFRAG_STEP_ANALYZE_FILE		2		// Called for each analysed file		{ pParam1:(LPCTSTR)pszFile, pParam2:NULL }
#define DEFRAG_STEP_BEFORE_MOVE			3		// File moving is about to start		{ pParam1:(PDEFRAG_OPTIONS)pOptions, pParam2:NULL }
#define DEFRAG_STEP_MOVE_FILE			4		// Called for each file				{ pParam1:(LPCTSTR)pszFile, pParam2:(PLONG64)piFileSize }
#define DEFRAG_STEP_MOVE_EXTENT			5		// Called for each file fragment		{ pParam1:(LPCTSTR)pszFile, pParam2:(PLONG64)piExtentSize }
typedef BOOL( *DefragmentTraceCallback )(_In_ LPVOID lpParam, _In_ int iStep, _In_opt_ LPVOID pParam1, _In_opt_ LPVOID pParam2);		// If returns FALSE, all operations will abort

typedef struct {
	ULONG Flags;								// Combination of DEFRAG_FLAG_*
	ULONG TargetFragmentCount;					// Used with DEFRAG_FLAG_FRAGMENT. Each file will be broken into this many fragments
	DefragmentLoggingCallback fnLogging;		// Optional logging function
	LPVOID lpLoggingParam;						// Optional logging function custom parameter
	DefragmentTraceCallback fnTracing;			// Optional tracing function
	LPVOID lpTracingParam;						// Optional tracing function custom parameter
} DEFRAG_OPTIONS, *PDEFRAG_OPTIONS;	



// Analyze fragmentation
// All files must be located on the same volume
DWORD DefragAnalyzeFiles(
	_In_ LPCTSTR *ppszFiles,								// Array of LPCTSTRs. The last entry must be NULL
	_In_opt_ PDEFRAG_OPTIONS pIn,
	_Out_opt_ PDEFRAG_ANALYSIS pOut
);


// Move (defragment or fragment) files
// All files must be located on the same volume
// If files are already (de)fragmented, the function returns ERROR_SUCCESS and pOut will indicate no data being moved
// Be prepared for this function to fail if there's intense disk activity while (de)fragmenting
// In such case it is recommended to retry later
DWORD DefragMoveFiles(
	_In_ LPCTSTR *ppszFiles,								// Array of LPCTSTRs. The last entry must be NULL
	_In_opt_ PDEFRAG_OPTIONS pIn,
	_Out_opt_ PDEFRAG_MOVE pOut
);


#ifdef __cplusplus
}
#endif

#endif