
// Marius Negrutiu
// Jan 2017

#include "StdAfx.h"
#include "Defragment.h"
#include <Winioctl.h>

#ifdef __cplusplus
extern "C" {
#endif

//? VCN: Virtual Cluster Number (offset within the file or stream space)
//? LCN: Logical Cluster Number (offset within the volume space)


//+ DEFRAG_FILE
typedef struct _DEFRAG_FILE {
	struct _DEFRAG_FILE *Next;						/// Singly linked list
	LPCTSTR Path;									/// "X:\Dir\File"
	HANDLE Handle;
	ULONG ClusterSize;								/// Usually 4096
	PRETRIEVAL_POINTERS_BUFFER Fragments;
	ULONG FragmentsSize;
	ULONG64 ClusterCount;							/// Extracted from Fragments during analysis
} DEFRAG_FILE;


//+ DEFRAG_FILES
typedef struct {

	BOOLEAN Dirty;									/// The data is no longer accurate. A new analysis is required

	/// Volume information
	struct {
		TCHAR Name[20];								/// "\\.\X:"
		HANDLE Handle;
		PVOLUME_BITMAP_BUFFER Bitmap;
		ULONG BitmapSize;
	} Volume;

	/// File list
	DEFRAG_FILE *Files;

	/// Analysis
	DEFRAG_ANALYSIS Analysis;

	/// Moving
	DEFRAG_OPTIONS Options;							/// Options for analysis and defragmentation
	DEFRAG_MOVE Move;

} DEFRAG_FILES;


//+ Log
#define Log(__pData, __Fmt, ...) \
    { \
		if ((__pData)->Options.fnLogging) \
			(__pData)->Options.fnLogging( (__pData)->Options.lpLoggingParam, __Fmt, __VA_ARGS__ ); \
	}

//+ Trace
#define Trace(__pData, __iStep, __pParam1, __pParam2) \
    ((__pData)->Options.fnTracing ? (__pData)->Options.fnTracing((__pData)->Options.lpTracingParam, __iStep, (LPVOID)__pParam1, (LPVOID)__pParam2) : TRUE)


//+ ValidHandle
#define ValidHandle(__h) \
	((__h) != NULL && (__h) != INVALID_HANDLE_VALUE)


//++ DefragGetFileClusterSize
/// Retrieve volume's allocation unit (cluster) size. Usually 4 KB
DWORD DefragGetFileClusterSize( _In_ HANDLE hVolume, _Out_ PULONG piClusterSize )
{
	DWORD err = ERROR_SUCCESS;
	if (ValidHandle( hVolume ) && piClusterSize) {

		typedef struct _FILE_FS_SIZE_INFORMATION {
			LARGE_INTEGER TotalAllocationUnits;
			LARGE_INTEGER AvailableAllocationUnits;
			ULONG SectorsPerAllocationUnit;
			ULONG BytesPerSector;
		} FILE_FS_SIZE_INFORMATION, *PFILE_FS_SIZE_INFORMATION;

		typedef struct _IO_STATUS_BLOCK {
			union {
				LONG Status;
				PVOID Pointer;
			} DUMMYUNIONNAME;
			ULONG_PTR Information;
		} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

		typedef LONG( NTAPI *TfnNtQueryVolumeInformationFile )(_In_ HANDLE FileHandle, _Out_ PIO_STATUS_BLOCK IoStatusBlock, _Out_ PVOID FsInformation, _In_ ULONG Length, _In_ int FsInformationClass);
		TfnNtQueryVolumeInformationFile NtQueryVolumeInformationFile = (TfnNtQueryVolumeInformationFile)GetProcAddress( GetModuleHandle( _T( "ntdll" ) ), "NtQueryVolumeInformationFile" );
		*piClusterSize = 0;
		if (NtQueryVolumeInformationFile) {

			IO_STATUS_BLOCK iosb;
			FILE_FS_SIZE_INFORMATION fsinfo;
			err = NtQueryVolumeInformationFile( hVolume, &iosb, &fsinfo, sizeof( fsinfo ), 3 /*FileFsSizeInformation*/ );
			if (err == ERROR_SUCCESS) {
				*piClusterSize = fsinfo.SectorsPerAllocationUnit * fsinfo.BytesPerSector;
			}

		} else {
			err = ERROR_PROC_NOT_FOUND;
		}
	} else {
		err = ERROR_INVALID_PARAMETER;
	}
	return err;
}


//++ DefragGetVolumeBitmap
/// Retrieve volume's cluster usage bitmap. Each bit represents a cluster on disk
DWORD DefragGetVolumeBitmap(
	_In_ HANDLE hVolume,
	_Out_ VOLUME_BITMAP_BUFFER **ppBuffer,		/// Must be HeapFree( )
	_Out_ ULONG *piBufferSize
	)
{
	DWORD err = ERROR_SUCCESS;
	if (ValidHandle( hVolume ) && ppBuffer && piBufferSize) {

		DWORD dwBytes = 0;
		const DWORD dwGrowBy = 2 * 1024 * 1024;		/// 2 MiB
		STARTING_VCN_INPUT_BUFFER vcn = {0};

		*piBufferSize = dwGrowBy;
		*ppBuffer = (PVOLUME_BITMAP_BUFFER)HeapAlloc( GetProcessHeap(), 0, *piBufferSize );

		while (TRUE) {

			if (DeviceIoControl( hVolume, FSCTL_GET_VOLUME_BITMAP, &vcn, sizeof( vcn ), *ppBuffer, *piBufferSize, &dwBytes, NULL )) {

				// Success
				*piBufferSize = dwBytes;
				err = ERROR_SUCCESS;
				break;

			} else {

				err = GetLastError();
				if (err == ERROR_INSUFFICIENT_BUFFER || err == ERROR_MORE_DATA) {

					// Buffer too small
					LPVOID buf2 = HeapReAlloc( GetProcessHeap(), 0, *ppBuffer, (*piBufferSize += dwGrowBy) );
					if (buf2) {
						*ppBuffer = (PVOLUME_BITMAP_BUFFER)buf2;
						continue;
					} else {
						err = GetLastError();
						break;
					}

				} else {

					// Other error
					break;
				}
			}
		}

		if (err != ERROR_SUCCESS) {
			if (*ppBuffer) {
				HeapFree( GetProcessHeap(), 0, *ppBuffer );
				*ppBuffer = NULL;
			}
			*piBufferSize = 0;
		}

	} else {
		err = ERROR_INVALID_PARAMETER;
	}

	return err;
}


//++ DefragBitmapFindMaxUnused
/// Search for the largest available disk area
DWORD DefragBitmapFindMaxUnused( _In_ VOLUME_BITMAP_BUFFER *pBitmap, _Out_ PLONG64 pLcn, _Out_ PLONG64 pSize )
{
	DWORD err = ERROR_SUCCESS;

	if (pLcn)
		*pLcn = 0;
	if (pSize)
		*pSize = 0;

	if (pBitmap && pBitmap->BitmapSize.QuadPart && pLcn && pSize) {

		/// Inside the volume bitmap each bit represents one cluster
		/// For improved performance, we'll be working with bytes (groups of eight cluster)
		LONG64 i;
		LONG64 MaxLcnStart = 0, MaxLcnLen = 0;
		LONG64 CurLcnStart = 0, CurLcnLen = 0;
		LONG64 BitmapTotalBytes = pBitmap->BitmapSize.QuadPart / 8;		/// Round up to a multiple of 8

		err = ERROR_NOT_FOUND;
		for (i = 0; i <= BitmapTotalBytes; i++) {

			if ((i != BitmapTotalBytes) &&		/// Last bitmap byte
				(pBitmap->Buffer[i] == 0))		/// Free bitmap byte
			{
				/// Update the current sequence of free clusters
				if (CurLcnStart == 0)
					CurLcnStart = i;			/// Start a new sequence
				CurLcnLen++;

			} else {

				/// Finish the current sequence of free clusters
				if (CurLcnLen > MaxLcnLen) {
					MaxLcnStart = CurLcnStart;
					MaxLcnLen = CurLcnLen;
					err = ERROR_SUCCESS;
				}
				CurLcnStart = 0;
				CurLcnLen = 0;
			}
		}

		/// Return
		if (err == ERROR_SUCCESS) {
			*pLcn = pBitmap->StartingLcn.QuadPart + MaxLcnStart * 8;
			*pSize = MaxLcnLen * 8;
		}

	} else {
		err = ERROR_INVALID_PARAMETER;
	}
	return err;
}


//++ DefragBitmapFindUnused
DWORD DefragBitmapFindUnused( _In_ VOLUME_BITMAP_BUFFER *pBitmap, _In_ LONG64 iClusterCount, _Out_ PLONG64 pLcn )
{
	DWORD err = ERROR_SUCCESS;

	if (pLcn)
		*pLcn = 0;

	if (pBitmap && pBitmap->BitmapSize.QuadPart && iClusterCount && pLcn) {

		/// Inside the volume bitmap each bit represents one cluster
		/// For improved performance, we'll be working with bytes (groups of eight cluster)
		/// We'll be searching the bitmap for (iClusterCount / 8) consecutive empty bytes
		/// (Negrutiu)
		LONG64 i, BitmapSequenceStart, BitmapSequenceLen;
		LONG64 BitmapTotalBytes;

		BitmapSequenceStart = -1;
		BitmapSequenceLen = (iClusterCount + 8) / 8;		/// Round up to a multiple of 8
		BitmapTotalBytes = pBitmap->BitmapSize.QuadPart / 8;

		err = ERROR_NOT_FOUND;
		for (i = 0; i < BitmapTotalBytes; i++) {
			if (pBitmap->Buffer[i] == 0) {
				/// The current bitmap byte has 8 free clusters
				if (BitmapSequenceStart >= 0) {
					if (i - BitmapSequenceStart >= BitmapSequenceLen) {

						// Found
						*pLcn = pBitmap->StartingLcn.QuadPart + BitmapSequenceStart * 8;
						err = ERROR_SUCCESS;
						break;

					}
				} else {
					/// Start a new sequence
					BitmapSequenceStart = i;
				}
			} else {
				/// The current bitmap byte has used clusters
				BitmapSequenceStart = -1;
			}
		}

	} else {
		err = ERROR_INVALID_PARAMETER;
	}
	return err;
}


//++ DefragBitmapMarkUsed
DWORD DefragBitmapMarkUsed( _In_ VOLUME_BITMAP_BUFFER *pBitmap, _In_ BOOL bUsed, _In_ LONG64 Lcn, _In_ LONG64 iClusterCount )
{
	DWORD err = ERROR_SUCCESS;
	if (pBitmap && pBitmap->BitmapSize.QuadPart) {

		LONG64 BitmapLcn = Lcn - pBitmap->StartingLcn.QuadPart;
		if (BitmapLcn >= 0 &&
			BitmapLcn < pBitmap->BitmapSize.QuadPart &&
			BitmapLcn + iClusterCount < pBitmap->BitmapSize.QuadPart)
		{
			LONG64 i;
			LONG64 ByteIndex, BitIndex;

			for (i = 0; i < iClusterCount; i++, BitmapLcn++) {

				ByteIndex = BitmapLcn / 8;
				BitIndex = BitmapLcn % 8;

				// Mark used
				if (bUsed) {
					pBitmap->Buffer[ByteIndex] |= (1 << BitIndex);
				} else {
					pBitmap->Buffer[ByteIndex] &= ~(1 << BitIndex);
				}
			}

		} else {
			err = ERROR_INVALID_INDEX;
		}
	} else {
		err = ERROR_INVALID_PARAMETER;
	}
	return err;
}


//++ DefragGetFileRetrievalPointers
DWORD DefragGetFileRetrievalPointers(
	_In_ HANDLE hFile,
	_Out_ RETRIEVAL_POINTERS_BUFFER **ppBuffer,		/// Must be HeapFree( )
	_Out_ ULONG *piBufferSize
)
{
	DWORD err = ERROR_SUCCESS;
	if (ValidHandle( hFile ) && ppBuffer && piBufferSize) {

		DWORD dwBytes = 0;
		const DWORD dwGrowBy = 1024;	/// 1 KiB
		STARTING_VCN_INPUT_BUFFER vcn = {0};

		*piBufferSize = dwGrowBy;
		*ppBuffer = (PRETRIEVAL_POINTERS_BUFFER)HeapAlloc( GetProcessHeap(), 0, *piBufferSize );

		while (TRUE) {

			if (DeviceIoControl( hFile, FSCTL_GET_RETRIEVAL_POINTERS, &vcn, sizeof( vcn ), *ppBuffer, *piBufferSize, &dwBytes, NULL )) {

				// Success
				*piBufferSize = dwBytes;
				err = ERROR_SUCCESS;
				break;

			} else {

				err = GetLastError();
				if (err == ERROR_INSUFFICIENT_BUFFER || err == ERROR_MORE_DATA) {

					// Buffer too small
					LPVOID buf2 = HeapReAlloc( GetProcessHeap(), 0, *ppBuffer, (*piBufferSize += dwGrowBy) );
					if (buf2) {
						*ppBuffer = (PRETRIEVAL_POINTERS_BUFFER)buf2;
						continue;
					} else {
						err = GetLastError();
						break;
					}

				} else if (err == ERROR_HANDLE_EOF) {

					// File is too small, probably stored in the MFT
					// Not fragmented
					(*ppBuffer)->ExtentCount = 0;
					(*ppBuffer)->StartingVcn.QuadPart = 0;
					*piBufferSize = sizeof( **ppBuffer );
					err = ERROR_SUCCESS;
					break;

				} else {

					// Other error
					break;
				}
			}
		}

		if (err != ERROR_SUCCESS) {
			if (*ppBuffer) {
				HeapFree( GetProcessHeap(), 0, *ppBuffer );
				*ppBuffer = NULL;
			}
			*piBufferSize = 0;
		}

	} else {
		err = ERROR_INVALID_PARAMETER;
	}

	return err;
}


//++ DefragFileExists
BOOL DefragFileExists( LPCTSTR pszFile )
{
	if (pszFile && *pszFile) {
		WIN32_FIND_DATA fd;
		HANDLE h = FindFirstFile( pszFile, &fd );
		if (ValidHandle( h )) {
			FindClose( h );
			return TRUE;
		}
	}
	return FALSE;
}


//++ DefragGetVolumeName
VOID DefragGetVolumeName( _In_ LPCTSTR pszFile, _Out_ LPTSTR pszVolume, _In_ ULONG iVolumeLen )
{
	if (pszVolume && iVolumeLen) {
		pszVolume[0] = 0;
		if (pszFile && pszFile) {
			if (pszFile[1] == _T( ':' )) {
				StringCchPrintf( pszVolume, iVolumeLen, _T( "\\\\.\\%C:" ), pszFile[0] );
			}
		}
	}
}


//++ DefragDataFindFile
/// Find a file in list
DEFRAG_FILE* DefragDataFindFile( _In_ DEFRAG_FILES *Data, _In_ LPCTSTR pszFile )
{
	DEFRAG_FILE *f = NULL;
	if (Data && pszFile && *pszFile)
		for (f = Data->Files; f; f = f->Next)
			if (CompareString( CP_ACP, NORM_IGNORECASE, pszFile, -1, f->Path, -1 ) == CSTR_EQUAL)
				return f;
	return f;
}


//++ DefragDataCreate
DWORD DefragDataCreate( _Inout_ DEFRAG_FILES *Data, _In_ LPCTSTR *ppszFileList )
{
	DWORD err = ERROR_SUCCESS;
	ULONG i;

	for (i = 0; (err == ERROR_SUCCESS) && (ppszFileList[i] != NULL); i++) {

		LPCTSTR pszFile = ppszFileList[i];
		if (DefragFileExists( pszFile )) {

			// Volume
			if (!Data->Volume.Name[0])
				DefragGetVolumeName( pszFile, Data->Volume.Name, ARRAYSIZE( Data->Volume.Name ) );

			// File
			if (err == ERROR_SUCCESS) {

				/// Make sure this file is on the same volume as the others
				TCHAR szFileVolume[20];
				DefragGetVolumeName( pszFile, szFileVolume, ARRAYSIZE( szFileVolume ) );
				if (CompareString( CP_ACP, NORM_IGNORECASE, szFileVolume, -1, Data->Volume.Name, -1 ) == CSTR_EQUAL) {

					/// Skip duplicate files
					if (!DefragDataFindFile( Data, pszFile )) {

						/// New entry
						DEFRAG_FILE *File = (DEFRAG_FILE*)HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( DEFRAG_FILE ) );
						if (File) {

							File->Path = pszFile;
							File->Handle = CreateFile( pszFile, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );
							if (ValidHandle( File->Handle )) {

								DefragGetFileClusterSize( File->Handle, &File->ClusterSize );

								if (!Data->Files) {
									/// Empty file list
									Data->Files = File;
								} else {
									/// Append to file list
									DEFRAG_FILE *f;
									for (f = Data->Files; f->Next; f = f->Next);
									f->Next = File;
								}

							} else {
								err = GetLastError();
							}
						} else {
							err = GetLastError();
						}
					}
				} else {
					/// This file is on different volume...
					err = ERROR_NOT_SAME_DEVICE;
				}
			}
		} else {
			/// File doesn't exist. Skip it...
		}
	}

	return err;
}


//++ DefragDataDestroy
DWORD DefragDataDestroy( _Inout_ DEFRAG_FILES *Data )
{
	DWORD err = ERROR_SUCCESS;
	if (Data) {

		// File list
		DEFRAG_FILE *f, *next;
		for (f = Data->Files; f; ) {

			if (ValidHandle( f->Handle ))
				CloseHandle( f->Handle );
			if (f->Fragments)
				HeapFree( GetProcessHeap(), 0, f->Fragments );

			next = f->Next;
			HeapFree( GetProcessHeap(), 0, f );
			f = next;
		}

		// Volume
		if (ValidHandle( Data->Volume.Handle ))
			CloseHandle( Data->Volume.Handle );
		if (Data->Volume.Bitmap)
			HeapFree( GetProcessHeap(), 0, Data->Volume.Bitmap );

		// Others
		ZeroMemory( Data, sizeof( *Data ) );

	} else {
		err = ERROR_INVALID_PARAMETER;
	}
	return err;
}


//++ DefragDataAnalyze
DWORD DefragDataAnalyze( _Inout_ DEFRAG_FILES* Data )
{
	DWORD err = ERROR_SUCCESS;
	if (Data) {

		ULONG i;
		DEFRAG_FILE *f;
		ULONG64 StartingVcn, NextLcn = 0, Clusters;

		if (!Trace( Data, DEFRAG_STEP_BEFORE_ANALYSIS, NULL, NULL ))
			return ERROR_REQUEST_ABORTED;

		/// Clear
		Data->Dirty = FALSE;
		ZeroMemory( &Data->Analysis, sizeof( Data->Analysis ) );
		Data->Analysis.MinFileFragments = ULLONG_MAX;

		/// Files
		for (f = Data->Files; f; f = f->Next) {

			if (!Trace( Data, DEFRAG_STEP_ANALYZE_FILE, f->Path, NULL ))
				return ERROR_REQUEST_ABORTED;

			Log( Data, _T( "Analyze %s\n" ), f->Path );

			/// Refresh fragmentation data
			if (f->Fragments)
				HeapFree( GetProcessHeap(), 0, f->Fragments );

			err = DefragGetFileRetrievalPointers( f->Handle, &f->Fragments, &f->FragmentsSize );
			if (err == ERROR_SUCCESS) {

				Data->Analysis.FileCount++;
				Data->Analysis.MaxFileFragments = max( Data->Analysis.MaxFileFragments, f->Fragments->ExtentCount );
				if (f->Fragments->ExtentCount > 0)
					Data->Analysis.MinFileFragments = min( Data->Analysis.MinFileFragments, f->Fragments->ExtentCount );
				Data->Analysis.ExtentCount += f->Fragments->ExtentCount;
				f->ClusterCount = 0;

				for (i = 0; i < f->Fragments->ExtentCount; i++) {

					StartingVcn = (i == 0 ? f->Fragments->StartingVcn.QuadPart : f->Fragments->Extents[i - 1].NextVcn.QuadPart);
					Clusters = f->Fragments->Extents[i].NextVcn.QuadPart - StartingVcn;

					Data->Analysis.ClusterCount += Clusters;
					Data->Analysis.TotalSize += Clusters * f->ClusterSize;
					f->ClusterCount += Clusters;

					/// Determine if this fragment and the previous one are contiguous
					if ((NextLcn != 0) && (NextLcn != f->Fragments->Extents[i].Lcn.QuadPart))
						Data->Analysis.DiffuseExtentCount++;
					NextLcn = f->Fragments->Extents[i].Lcn.QuadPart + Clusters;

					Log( Data,
						_T( "  #%04u: Vcn:0x%04I64x-0x%04I64x, Lcn:0x%08I64x-0x%08I64x, Clusters:%I64u (%I64u bytes)\n" ),
						i + 1,
						StartingVcn,
						StartingVcn + Clusters,
						f->Fragments->Extents[i].Lcn.QuadPart,
						f->Fragments->Extents[i].Lcn.QuadPart + Clusters,
						Clusters,
						Clusters * f->ClusterSize
					);
				}

			} else {
				Log( Data, _T( "  Error reading retrieval pointers: 0x%x\n" ), err );
				break;
			}
		}

		if (Data->Analysis.MinFileFragments == ULLONG_MAX)
			Data->Analysis.MinFileFragments = 0;

		/// Results
		Log( Data, _T( "\n" ), 0 );
		Log( Data, _T( "  Files:    %u (most fragmented has %I64u extents, least fragmented has %I64u extents)\n" ), Data->Analysis.FileCount, Data->Analysis.MaxFileFragments, Data->Analysis.MinFileFragments );
		Log( Data, _T( "  Extents:  %I64u (%I64u clusters, %I64u bytes)\n" ), Data->Analysis.ExtentCount, Data->Analysis.ClusterCount, Data->Analysis.TotalSize );
		Log( Data, _T( "  Diffuse:  %I64u extents\n" ), Data->Analysis.DiffuseExtentCount );

	} else {
		err = ERROR_INVALID_PARAMETER;
	}
	return err;
}


//++ DefragDataFragment
DWORD DefragDataFragment( _In_ DEFRAG_FILES *Data )
{
	DWORD err = ERROR_SUCCESS;
	DEFRAG_FILE *f;
	ULONG BytesRead;
	ULONG i;

	assert( Data );
	assert( ValidHandle( Data->Volume.Handle ) );
	assert( Data->Volume.Bitmap );

	if (Data->Options.TargetFragmentCount < 2) {
		Log( Data, _T( "  Invalid target fragment count %u (must be >= 2)\n" ), Data->Options.TargetFragmentCount );
		return ERROR_INVALID_PARAMETER;
	}

	// Iterate files
	Log( Data, _T( "\n" ), 0 );
	for (f = Data->Files; f && (err == ERROR_SUCCESS); f = f->Next) {

		ULONG iTargetFragments = min( Data->Options.TargetFragmentCount, (ULONG)f->ClusterCount );
		ULONG64 iFileClusters = f->ClusterCount;

		LONG   SourceExtentIndex = -1;
		LONG64 SourceExtentVcn = 0;		/// Virtual cluster number (relative to file)
		LONG64 SourceExtentLcn = 0;		/// Logical cluster number (relative to volume)
		LONG64 SourceExtentSize = 0;	/// Number of clusters

		if (f->Fragments->ExtentCount == 0 || f->ClusterCount == 0)
			continue;

		{
			LONG64 FileSize = f->ClusterCount * f->ClusterSize;
			if (!Trace( Data, DEFRAG_STEP_MOVE_FILE, f->Path, &FileSize ))
				return ERROR_REQUEST_ABORTED;
		}

		Log( Data, _T( "Fragment %s\n" ), f->Path );
		Data->Move.FileCount++;

		// Target fragments
		for (i = 0; (i < iTargetFragments) && (err == ERROR_SUCCESS); i++) {

			LONG64 TargetExtentSize = ((i == iTargetFragments - 1) ? iFileClusters : (f->ClusterCount / iTargetFragments));
			LONG64 TargetExtentLcn, TargetExtentMaxSize;

			/// Find the largest free disk area
			err = DefragBitmapFindMaxUnused( Data->Volume.Bitmap, &TargetExtentLcn, &TargetExtentMaxSize );
			if (err != ERROR_SUCCESS) {
				Log( Data, _T( "  Not enough free space\n" ), 0 );
				break;
			}
			if (TargetExtentMaxSize < TargetExtentSize) {
				err = ERROR_DISK_FULL;
				Log( Data, _T( "  Not enough free space\n" ), 0 );
				break;
			}
#if _DEBUG || DBG
			Log( Data, _T( "  [d]    Largest free extent found at Lcn:0x%Ix-0x%Ix (%u clusters)\n" ), TargetExtentLcn, TargetExtentLcn + TargetExtentMaxSize, TargetExtentMaxSize );
#endif

			/// Construct a new (destination) fragment in the center of the largest free area
			TargetExtentLcn += ((TargetExtentMaxSize - TargetExtentSize) / 2);
			TargetExtentMaxSize = 0;

			for (; (TargetExtentSize > 0) && (err == ERROR_SUCCESS); ) {

				MOVE_FILE_DATA mfd = {0};

				if (SourceExtentSize == 0) {
					/// Move to the next (source) fragment
					SourceExtentIndex++;
					SourceExtentVcn = (SourceExtentIndex == 0 ? f->Fragments->StartingVcn.QuadPart : f->Fragments->Extents[SourceExtentIndex - 1].NextVcn.QuadPart);
					SourceExtentLcn = f->Fragments->Extents[SourceExtentIndex].Lcn.QuadPart;
					SourceExtentSize = f->Fragments->Extents[SourceExtentIndex].NextVcn.QuadPart - SourceExtentVcn;
				}

				mfd.FileHandle = f->Handle;
				mfd.StartingVcn.QuadPart = SourceExtentVcn;
				mfd.StartingLcn.QuadPart = TargetExtentLcn;
				mfd.ClusterCount = (DWORD)min( TargetExtentSize, SourceExtentSize );

				{
					LONG64 ExtentSize = mfd.ClusterCount * f->ClusterSize;
					if (!Trace( Data, DEFRAG_STEP_MOVE_EXTENT, f->Path, &ExtentSize ))
						return ERROR_REQUEST_ABORTED;
				}

				if (Data->Options.Flags & DEFRAG_FLAG_SIMULATE) {
					err = ERROR_SUCCESS;	/// Don't write to disk
				} else {
					err = DeviceIoControl( Data->Volume.Handle, FSCTL_MOVE_FILE, &mfd, sizeof( mfd ), NULL, 0, &BytesRead, NULL ) ? ERROR_SUCCESS : GetLastError();
				}

				Log( Data,
					_T( "  #%04u: Move %u clusters (%u bytes) {Vcn:0x%04I64x-0x%04I64x -> Lcn:0x%08I64x-0x%08I64x}: 0x%x%s\n" ),
					i + 1,
					mfd.ClusterCount,
					mfd.ClusterCount * f->ClusterSize,
					mfd.StartingVcn.QuadPart,
					mfd.StartingVcn.QuadPart + mfd.ClusterCount,
					mfd.StartingLcn.QuadPart,
					mfd.StartingLcn.QuadPart + mfd.ClusterCount,
					err,
					Data->Options.Flags & DEFRAG_FLAG_SIMULATE ? _T( " (simulated)" ) : _T( "" )
				);

				if (err == ERROR_SUCCESS) {

					/// Update volume bitmap
					//! TODO: Make sure DefragBitmapMarkUsed works correctly
					DefragBitmapMarkUsed( Data->Volume.Bitmap, TRUE, mfd.StartingLcn.QuadPart, mfd.ClusterCount );		/// New clusters: set in-use
					DefragBitmapMarkUsed( Data->Volume.Bitmap, FALSE, SourceExtentLcn, mfd.ClusterCount );				/// Old clusters: reset in-use

					/// Mark the data as dirty
					Data->Dirty = TRUE;

					/// Advance inside the (source) fragment
					SourceExtentVcn += mfd.ClusterCount;
					SourceExtentLcn += mfd.ClusterCount;
					SourceExtentSize -= mfd.ClusterCount;
					assert( SourceExtentSize >= 0 );

					/// Advance inside the (target) fragment
					TargetExtentLcn += mfd.ClusterCount;

					/// Remaining clusters
					TargetExtentSize -= mfd.ClusterCount;
					iFileClusters -= mfd.ClusterCount;

					/// Stats
					Data->Move.ClusterCount += mfd.ClusterCount;
					Data->Move.TotalSize += mfd.ClusterCount * f->ClusterSize;
				}

			}	/// for(SourceFragments)
		}	/// for(TargetFragments)
	}	/// for(files)

	return err;
}


//++ DefragDataDefragment
DWORD DefragDataDefragment( _In_ DEFRAG_FILES *Data )
{
	DWORD err = ERROR_SUCCESS;
	LONG64 VolumeLcn = -1;
	DEFRAG_FILE *f;
	ULONG i, BytesRead;

	assert( Data );
	assert( ValidHandle( Data->Volume.Handle ) );
	assert( Data->Volume.Bitmap );

	// Search for a new location (Compact == TRUE)
	if (Data->Options.Flags & DEFRAG_FLAG_COMPACT) {
		err = DefragBitmapFindUnused( Data->Volume.Bitmap, Data->Analysis.ClusterCount, &VolumeLcn );
		if (err != ERROR_SUCCESS) {
			Log( Data, _T( "  Not enough free space\n" ), 0 );
			return err;
		}
#if _DEBUG || DBG
		Log( Data, _T( "  [d]    Free extent found at Lcn:0x%Ix-0x%Ix (%u clusters)\n" ), VolumeLcn, VolumeLcn + Data->Analysis.ClusterCount, Data->Analysis.ClusterCount );
#endif
	}

	// Iterate files
	Log( Data, _T( "\n" ), 0 );
	for (f = Data->Files; f && (err == ERROR_SUCCESS); f = f->Next) {

		if (f->Fragments->ExtentCount == 0 || f->ClusterCount == 0)
			continue;

		{
			LONG64 FileSize = f->ClusterCount * f->ClusterSize;
			if (!Trace( Data, DEFRAG_STEP_MOVE_FILE, f->Path, &FileSize ))
				return ERROR_REQUEST_ABORTED;
		}

		Log( Data, _T( "Defragment %s\n" ), f->Path );
		Data->Move.FileCount++;

		// Search for a new location (Compact == FALSE)
		if (!(Data->Options.Flags & DEFRAG_FLAG_COMPACT)) {
			err = DefragBitmapFindUnused( Data->Volume.Bitmap, f->ClusterCount, &VolumeLcn );
			if (err != ERROR_SUCCESS) {
				Log( Data, _T( "  Not enough free space to defragment\n" ), 0 );
				break;
			}
		}

		// Iterate fragments
		for (i = 0; (i < f->Fragments->ExtentCount) && (err == ERROR_SUCCESS); i++) {

			MOVE_FILE_DATA mfd = {0};
			mfd.FileHandle = f->Handle;
			mfd.StartingVcn.QuadPart = (i == 0 ? f->Fragments->StartingVcn.QuadPart : f->Fragments->Extents[i - 1].NextVcn.QuadPart);
			mfd.StartingLcn.QuadPart = VolumeLcn;
			mfd.ClusterCount = (DWORD)(f->Fragments->Extents[i].NextVcn.QuadPart - mfd.StartingVcn.QuadPart);

			{
				LONG64 ExtentSize = mfd.ClusterCount * f->ClusterSize;
				if (!Trace( Data, DEFRAG_STEP_MOVE_EXTENT, f->Path, &ExtentSize ))
					return ERROR_REQUEST_ABORTED;
			}

			if (Data->Options.Flags & DEFRAG_FLAG_SIMULATE) {
				err = ERROR_SUCCESS;	/// Don't write to disk
			} else {
				err = DeviceIoControl( Data->Volume.Handle, FSCTL_MOVE_FILE, &mfd, sizeof( mfd ), NULL, 0, &BytesRead, NULL ) ? ERROR_SUCCESS : GetLastError();
			}

			Log( Data,
				_T( "  #%04u: Move %u clusters (%u bytes) {Vcn:0x%04I64x-0x%04I64x -> Lcn:0x%08I64x-0x%08I64x}: 0x%x%s\n" ),
				i + 1,
				mfd.ClusterCount,
				mfd.ClusterCount * f->ClusterSize,
				mfd.StartingVcn.QuadPart,
				mfd.StartingVcn.QuadPart + mfd.ClusterCount,
				mfd.StartingLcn.QuadPart,
				mfd.StartingLcn.QuadPart + mfd.ClusterCount,
				err,
				Data->Options.Flags & DEFRAG_FLAG_SIMULATE ? _T( " (simulated)" ) : _T( "" )
			);

			if (err == ERROR_SUCCESS) {

				/// Update volume bitmap
				//! TODO: Make sure DefragBitmapMarkUsed works correctly
				DefragBitmapMarkUsed( Data->Volume.Bitmap, TRUE, mfd.StartingLcn.QuadPart, mfd.ClusterCount );					/// New clusters: set in-use
				DefragBitmapMarkUsed( Data->Volume.Bitmap, FALSE, f->Fragments->Extents[i].Lcn.QuadPart, mfd.ClusterCount );	/// Old clusters: reset in-use

				/// Advance on disk
				VolumeLcn += mfd.ClusterCount;

				/// Mark the data as dirty
				Data->Dirty = TRUE;

				/// Stats
				Data->Move.ClusterCount += mfd.ClusterCount;
				Data->Move.TotalSize += mfd.ClusterCount * f->ClusterSize;
			}
		}	/// for(extents)
	}	/// for(files)

	return err;
}


//++ DefragDataMoveFiles
DWORD DefragDataMoveFiles( _In_ DEFRAG_FILES *Data )
{
	DWORD err = ERROR_SUCCESS;
	if (Data) {

		if (Data->Dirty)
			return ERROR_INVALID_DATA;					/// A new analysis is required

		if (Data->Analysis.FileCount == 0) {
			Log( Data, _T( "  No files specified.\n" ), 0 );
			return ERROR_SUCCESS;
		}

		if (!Trace( Data, DEFRAG_STEP_BEFORE_MOVE, &Data->Options, NULL ))
			return ERROR_REQUEST_ABORTED;

		// Any (de)fragmentation needed?
		if (Data->Options.Flags & DEFRAG_FLAG_FRAGMENT) {
			if (Data->Analysis.MinFileFragments >= Data->Options.TargetFragmentCount) {
				Log( Data, _T( "  Nothing to fragment (files already fragmented)\n" ), 0 );
				return ERROR_SUCCESS;
			}
		} else {
			if (Data->Analysis.MaxFileFragments <= 1 &&														/// No fragments
				(!(Data->Options.Flags & DEFRAG_FLAG_COMPACT) || Data->Analysis.DiffuseExtentCount == 0))	/// No diffuse fragments when compacting
			{
				if (Data->Options.Flags & DEFRAG_FLAG_COMPACT) {
					Log( Data, _T( "  Nothing to defragment (files already compact)\n" ), 0 );
				} else {
					Log( Data, _T( "  Nothing to defragment (no file extents)\n" ), 0 );
				}
				return ERROR_SUCCESS;
			}
		}

		/// Log input
		Log( Data, _T( "\n" ), 0 );
#if _DEBUG || DBG
		Log( Data, _T( "[d] Compact: %s\n" ), Data->Options.Flags & DEFRAG_FLAG_COMPACT ? _T( "TRUE" ) : _T( "FALSE" ) );
		Log( Data, _T( "[d] Fragment: %s (target fragments: %Iu)\n" ), Data->Options.Flags & DEFRAG_FLAG_FRAGMENT ? _T( "TRUE" ) : _T( "FALSE" ), Data->Options.TargetFragmentCount );
		Log( Data, _T( "[d] Simulate: %s\n" ), Data->Options.Flags & DEFRAG_FLAG_SIMULATE ? _T( "TRUE" ) : _T( "FALSE" ) );
		Log( Data, _T( "\n" ), 0 );
#endif
		Log( Data, _T( "Read %s volume bitmap\n" ), Data->Volume.Name );

		/// Clear
		ZeroMemory( &Data->Move, sizeof( Data->Move ) );

		// Open volume (once)
		if (!ValidHandle( Data->Volume.Handle )) {
			Data->Volume.Handle = CreateFile( Data->Volume.Name, FILE_READ_DATA | FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );
			if (!ValidHandle( Data->Volume.Handle ))
				err = GetLastError();
		}
		if (err == ERROR_SUCCESS) {

			// Refresh bitmap
			if (Data->Volume.Bitmap) {
				HeapFree( GetProcessHeap(), 0, Data->Volume.Bitmap );
				Data->Volume.Bitmap = NULL;
				Data->Volume.BitmapSize = 0;
			}
			err = DefragGetVolumeBitmap( Data->Volume.Handle, &Data->Volume.Bitmap, &Data->Volume.BitmapSize );
			if (err == ERROR_SUCCESS) {

				ULONG ClusterSize = 0;
				DefragGetFileClusterSize( Data->Volume.Handle, &ClusterSize );
				Log( Data, _T( "  Bitmap: %I64u clusters (%I64u bytes)\n" ), Data->Volume.Bitmap->BitmapSize.QuadPart, Data->Volume.Bitmap->BitmapSize.QuadPart * ClusterSize );

				// Defragment
				if (Data->Options.Flags & DEFRAG_FLAG_FRAGMENT) {
					err = DefragDataFragment( Data );
				} else {
					err = DefragDataDefragment( Data );
				}
				if (err == ERROR_SUCCESS) {

					/// Results
					Log( Data, _T( "\n" ), 0 );
					Log( Data, _T( "  Files moved:    %I64u\n" ), Data->Move.FileCount );
					Log( Data, _T( "  Clusters moved: %I64u\n" ), Data->Move.ClusterCount );
					Log( Data, _T( "  Bytes moved:    %I64u\n" ), Data->Move.TotalSize );
					if (Data->Options.Flags & DEFRAG_FLAG_SIMULATE)
						Log( Data, _T( "  Simulate:       Nothing was written to disk\n" ), 0 );
				}

			} else {
				/// No volume bitmap
				Log( Data, _T( "  Error 0x%x\n" ), err );
			}
		} else {
			/// Open failed
			Log( Data, _T( "  Error 0x%x\n" ), err );
		}

	} else {
		err = ERROR_INVALID_PARAMETER;
	}

	return err;
}


//++ DefragAnalyzeFiles
DWORD DefragAnalyzeFiles(
	_In_ LPCTSTR *ppszFiles,
	_In_opt_ PDEFRAG_OPTIONS pIn,
	_Out_opt_ PDEFRAG_ANALYSIS pOut
	)
{
	DWORD err = ERROR_SUCCESS;
	if (ppszFiles) {
		
		DEFRAG_FILES Data = {0};

		if (pIn)
			CopyMemory( &Data.Options, pIn, sizeof( Data.Options ) );
		if (pOut)
			ZeroMemory( pOut, sizeof( *pOut ) );

		err = DefragDataCreate( &Data, ppszFiles );
		if (err == ERROR_SUCCESS) {

			err = DefragDataAnalyze( &Data );
			if (pOut)
				CopyMemory( pOut, &Data.Analysis, sizeof( *pOut ) );
		}

		DefragDataDestroy( &Data );
	
	} else {
		err = ERROR_INVALID_PARAMETER;
	}
	return err;
}


//++ DefragMoveFiles
DWORD DefragMoveFiles(
	_In_ LPCTSTR *ppszFiles,
	_In_opt_ PDEFRAG_OPTIONS pIn,
	_Out_opt_ PDEFRAG_MOVE pOut
	)
{
	DWORD err = ERROR_SUCCESS;
	if (ppszFiles) {

		DEFRAG_FILES Data = {0};

		if (pIn)
			CopyMemory( &Data.Options, pIn, sizeof( Data.Options ) );
		if (pOut)
			ZeroMemory( pOut, sizeof( *pOut ) );

		err = DefragDataCreate( &Data, ppszFiles );
		if (err == ERROR_SUCCESS) {

			err = DefragDataAnalyze( &Data );
			if (err == ERROR_SUCCESS) {

				err = DefragDataMoveFiles( &Data );
				if (err == ERROR_SUCCESS) {

					if (pOut)
						CopyMemory( pOut, &Data.Move, sizeof( *pOut ) );
				}
			}
		}

		DefragDataDestroy( &Data );

	} else {
		err = ERROR_INVALID_PARAMETER;
	}
	return err;
}


#ifdef __cplusplus
}
#endif
