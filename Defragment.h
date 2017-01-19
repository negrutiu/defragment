
// Marius Negrutiu
// Jan 2017

#pragma once
#ifndef _DEFRAGMENT_H_
#define _DEFRAGMENT_H_

#ifdef __cplusplus
extern "C" {
#endif


// Defragment single file
// If the file is not fragmented, the function returns ERROR_SUCCESS and BytesMoved will be zero
// Be prepared for this function to fail if there's intense disk activity while defragmenting
// In such case it is recommended to retry at a later time
DWORD DefragmentFile(
	_In_ LPCTSTR pszFile,
	_Out_ PULONG64 piBytesMoved
);

// Defragment multiple files and move them close toghether
// Files must be located on the same volume
// If files are already compact, the function returns ERROR_SUCCESS and BytesMoved will be zero
// Be prepared for this function to fail if there's intense disk activity while defragmenting
// In such case it is recommended to retry at a later time
DWORD CompactFiles(
	_In_ LPCTSTR *ppszFileList,
	_Out_ PULONG64 piBytesMoved
);


#ifdef __cplusplus
}
#endif

#endif /// _DEFRAGMENT_H_