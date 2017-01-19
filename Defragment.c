
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
	PRETRIEVAL_POINTERS_BUFFER Fragments;
	ULONG FragmentsSize;
} DEFRAG_FILE;


//+ DEFRAG_FILES
typedef struct {

	/// Volume information
	struct {
		TCHAR Name[20];								/// "\\.\X:"
		HANDLE Handle;
		ULONG ClusterSize;							/// Usually 4096
		PVOLUME_BITMAP_BUFFER Bitmap;
		ULONG BitmapSize;
	} Volume;

	/// File list
	DEFRAG_FILE *Files;

	/// Analysis
	struct {
		BOOL Dirty;									/// Set after defragmentation. If TRUE, all analysis data must be treated as obsolete. A new analysis should be performed
		ULONG FileCount;							/// Count of all files
		ULONG MaxFileFragments;						/// Most fragmented file
		ULONG64 ClusterCount;						/// Overall number of clusters
		ULONG64 ExtentCount;						/// Overall number of extents (fragments)
		ULONG64 DiffuseExtentCount;					/// Overall number of extents that are not contiguous
	} Analysis;

} DEFRAG_FILES;


//+ ValidHandle
#define ValidHandle(__h) \
	((__h) != NULL && (__h) != INVALID_HANDLE_VALUE)


//++ DefragGetVolumeClusterSize
/// Retrieve volume's allocation unit (cluster) size. Usually 4 KB
DWORD DefragGetVolumeClusterSize( _In_ HANDLE hVolume, _Out_ PULONG piClusterSize )
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

			// Volume (initialized once)
			if (!ValidHandle( Data->Volume.Handle )) {
				DefragGetVolumeName( pszFile, Data->Volume.Name, ARRAYSIZE( Data->Volume.Name ) );
				Data->Volume.Handle = CreateFile( Data->Volume.Name, FILE_READ_DATA | FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );
				if (ValidHandle( Data->Volume.Handle )) {
					err = DefragGetVolumeClusterSize( Data->Volume.Handle, &Data->Volume.ClusterSize );
					if (err == ERROR_SUCCESS) {
						/// Get the volume bitmap later...
					}
				} else {
					err = GetLastError();
				}
			}

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

		ZeroMemory( &Data->Analysis, sizeof( Data->Analysis ) );

		/// Volume
		if (err == ERROR_SUCCESS) {
			if (Data->Volume.Bitmap)
				HeapFree( GetProcessHeap(), 0, Data->Volume.Bitmap );
			err = DefragGetVolumeBitmap( Data->Volume.Handle, &Data->Volume.Bitmap, &Data->Volume.BitmapSize );
			_tprintf( _T( "Analyze volume \"%s\"\n" ), *Data->Volume.Name ? Data->Volume.Name : _T( "n/a" ) );
			_tprintf( _T( "  ClusterSize: %u bytes\n" ), Data->Volume.ClusterSize );
			_tprintf( _T( "  BitmapSize: %u bytes\n" ), Data->Volume.BitmapSize );
		}

		/// Files
		if (err == ERROR_SUCCESS) {

			ULONG i;
			DEFRAG_FILE *f;
			ULONG64 StartingVcn, NextLcn = 0, Clusters;

			for (f = Data->Files; f; f = f->Next) {

				/// Refresh fragmentation data
				if (f->Fragments)
					HeapFree( GetProcessHeap(), 0, f->Fragments );
				err = DefragGetFileRetrievalPointers( f->Handle, &f->Fragments, &f->FragmentsSize );
				if (err == ERROR_SUCCESS) {

					Data->Analysis.FileCount++;
					Data->Analysis.MaxFileFragments = max( Data->Analysis.MaxFileFragments, f->Fragments->ExtentCount );
					Data->Analysis.ExtentCount += f->Fragments->ExtentCount;

					_tprintf( _T( "Analyze \"%s\"\n" ), f->Path );
					for (i = 0; i < f->Fragments->ExtentCount; i++) {

						StartingVcn = (i == 0 ? f->Fragments->StartingVcn.QuadPart : f->Fragments->Extents[i - 1].NextVcn.QuadPart);
						Clusters = f->Fragments->Extents[i].NextVcn.QuadPart - StartingVcn;

						Data->Analysis.ClusterCount += Clusters;

						/// Determine if this fragment and the previous one are contiguous
						if ((NextLcn != 0) && (NextLcn != f->Fragments->Extents[i].Lcn.QuadPart))
							Data->Analysis.DiffuseExtentCount++;
						NextLcn = f->Fragments->Extents[i].Lcn.QuadPart + Clusters;

						_tprintf(
							_T( "  #%u: Vcn:0x%I64x, Lcn:0x%I64x-0x%I64x, Clusters:%I64u (%I64u bytes)\n" ),
							i + 1,
							StartingVcn,
							f->Fragments->Extents[i].Lcn.QuadPart,
							f->Fragments->Extents[i].Lcn.QuadPart + Clusters,
							Clusters,
							Clusters * Data->Volume.ClusterSize
							);
					}

				} else {
					break;
				}
			}
		}

	} else {
		err = ERROR_INVALID_PARAMETER;
	}
	return err;
}


//++ DefragDataDefragment
DWORD DefragDataDefragment( _In_ DEFRAG_FILES *Data )
{
	DWORD err = ERROR_SUCCESS;
	if (Data) {

		if (Data->Analysis.Dirty)
			return ERROR_INVALID_DATA;					/// A new analysis is required

		_tprintf( _T( "  Files: %u, Fragments: %u, Clusters: %I64u (%I64u bytes)\n" ), Data->Analysis.FileCount, Data->Analysis.ExtentCount, Data->Analysis.ClusterCount, Data->Analysis.ClusterCount * Data->Volume.ClusterSize );
		_tprintf( _T( "  MaxFileFragments: %u\n" ), Data->Analysis.MaxFileFragments );
		_tprintf( _T( "  DiffuseFragmentCount: %u\n" ), Data->Analysis.DiffuseExtentCount );

		if (Data->Analysis.MaxFileFragments > 1 ||		/// There are fragmented files
			Data->Analysis.DiffuseExtentCount > 0)		/// There are diffuse (not compact) fragments
		{
			LONG64 j, BitmapSequenceStart, BitmapSequenceLen;
			LONG64 BitmapTotalSize;

			// Search for a new location
			/// Our files require a total of FileClusterCount clusters
			/// Within the volume bitmap, each bit represents a cluster
			/// We'll be searching the bitmap for (FileClusterCount / 8) empty bytes
			/// (Negrutiu)
			BitmapSequenceStart = -1;
			BitmapSequenceLen = (Data->Analysis.ClusterCount + 8) / 8;		/// Round up to a multiple of 8
			BitmapTotalSize = Data->Volume.Bitmap->BitmapSize.QuadPart / 8;

			for (j = 0; j < BitmapTotalSize; j++) {
				if (Data->Volume.Bitmap->Buffer[j] == 0) {
					/// The current bitmap byte has 8 free clusters
					if (BitmapSequenceStart >= 0) {
						if (j - BitmapSequenceStart >= BitmapSequenceLen) {
							// Found
							break;
						}
					} else {
						/// Start a new sequence
						BitmapSequenceStart = j;
					}
				} else {
					/// The current bitmap byte has used clusters
					BitmapSequenceStart = -1;
				}
			}

			if (j < BitmapTotalSize) {

				DEFRAG_FILE *f;
				ULONG i, BytesReaad;

				// Defragment
				LONG64 VolumeLcn = Data->Volume.Bitmap->StartingLcn.QuadPart + BitmapSequenceStart * 8;
				_tprintf( _T( "\n" ) );
				for (f = Data->Files; f && (err == ERROR_SUCCESS); f = f->Next) {
					_tprintf( _T( "Defragment \"%s\"\n" ), f->Path );
					for (i = 0; (i < f->Fragments->ExtentCount) && (err == ERROR_SUCCESS); i++) {

						MOVE_FILE_DATA mfd = {0};
						mfd.FileHandle = f->Handle;
						mfd.StartingVcn.QuadPart = (i == 0 ? f->Fragments->StartingVcn.QuadPart : f->Fragments->Extents[i - 1].NextVcn.QuadPart);
						mfd.StartingLcn.QuadPart = VolumeLcn;
						mfd.ClusterCount = (DWORD)(f->Fragments->Extents[i].NextVcn.QuadPart - mfd.StartingVcn.QuadPart);

						err = DeviceIoControl( Data->Volume.Handle, FSCTL_MOVE_FILE, &mfd, sizeof( mfd ), NULL, 0, &BytesReaad, NULL ) ? ERROR_SUCCESS : GetLastError();
						_tprintf(
							_T( "  #%u: Move %I64u clusters (%I64u bytes) {Vcn:0x%I64x-0x%I64x -> Lcn:0x%I64x-0x%I64x}: 0x%x\n" ),
							i + 1,
							mfd.ClusterCount,
							mfd.ClusterCount * Data->Volume.ClusterSize,
							mfd.StartingVcn.QuadPart,
							mfd.StartingVcn.QuadPart + mfd.ClusterCount,
							mfd.StartingLcn.QuadPart,
							mfd.StartingLcn.QuadPart + mfd.ClusterCount
							);

						if (err == ERROR_SUCCESS) {

							/// Advance on disk
							VolumeLcn += mfd.ClusterCount;

							/// Mark the analysis data as dirty
							Data->Analysis.Dirty = TRUE;

						}
					}
				}

			} else {
				/// There's no empty space for all files
				err = ERROR_DISK_FULL;
				_tprintf( _T( "  Not enough disk space to perform defragmentation\n" ) );
			}
		} else {
			_tprintf( _T( "  Defragmenting not needed.\n" ) );
		}

	} else {
		err = ERROR_INVALID_PARAMETER;
	}

	return err;
}


//++ DefragmentFile
DWORD DefragmentFile( _In_ LPCTSTR pszFile, _Out_ PULONG64 piBytesMoved )
{
	LPCTSTR Files[2] = {pszFile, NULL};
	return CompactFiles( Files, piBytesMoved );
}


//++ CompactFiles
DWORD CompactFiles( _In_ LPCTSTR *ppszFileList, _Out_ PULONG64 piBytesMoved )
{
	DWORD err = ERROR_SUCCESS;
	DEFRAG_FILES Data = {0};

	// Gather data
	err = DefragDataCreate( &Data, ppszFileList );
	if (err == ERROR_SUCCESS) {

		// Analyze fragmentation
		err = DefragDataAnalyze( &Data );
		if (err == ERROR_SUCCESS) {

			// Defragment and compact all files
			err = DefragDataDefragment( &Data );
		}
	}

	DefragDataDestroy( &Data );
	return err;
}


#ifdef __cplusplus
}
#endif
