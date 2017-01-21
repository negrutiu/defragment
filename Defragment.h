
// Marius Negrutiu
// Jan 2017

#pragma once
#ifndef _DEFRAGMENT_H_
#define _DEFRAGMENT_H_

#ifdef __cplusplus
extern "C" {
#endif

// Callback useful for logging
typedef VOID( *DefragmentLoggingCallback )(_In_ LPVOID lpParam, _In_ LPCTSTR pszFmt, _In_ ...);

// Defragment single file
// If the file is not fragmented, the function returns ERROR_SUCCESS and BytesMoved will be zero
// Be prepared for this function to fail if there's intense disk activity while defragmenting
// In such case it is recommended to retry at a later time
DWORD DefragmentFile(
	_In_ LPCTSTR pszFile,
	_Out_opt_ PULONG64 piBytesMoved,
	_In_opt_ DefragmentLoggingCallback fnLogging,
	_In_opt_ LPVOID lpLoggingParam
);

// Defragment multiple files and move them close toghether
// Files must be located on the same volume
// If files are already compact, the function returns ERROR_SUCCESS and BytesMoved will be zero
// Be prepared for this function to fail if there's intense disk activity while defragmenting
// In such case it is recommended to retry at a later time
DWORD CompactFiles(
	_In_ LPCTSTR *ppszFileList,
	_Out_opt_ PULONG64 piBytesMoved,
	_In_opt_ DefragmentLoggingCallback fnLogging,
	_In_opt_ LPVOID lpLoggingParam
);

#ifdef __cplusplus
}
#endif

#endif /// _DEFRAGMENT_H_