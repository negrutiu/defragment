
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
} DEFRAG_FILE;


//+ DEFRAG_FILES
typedef struct {

	/// Logging (optional)
	DefragmentLoggingCallback fnLogging;
	LPVOID lpLoggingParam;

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
	struct {
		BOOL Dirty;									/// Set after defragmentation. If TRUE, all analysis data must be treated as obsolete. A new analysis should be performed
		ULONG FileCount;							/// Count of all files
		ULONG64 TotalSize;							/// Combined file sizes
		ULONG64 MaxFileFragments;					/// Most fragmented file
		ULONG64 ClusterCount;						/// Overall number of clusters
		ULONG64 ExtentCount;						/// Overall number of extents (fragments)
		ULONG64 DiffuseExtentCount;					/// Overall number of extents that are not contiguous
	} Analysis;

	/// Defragment
	struct {
		ULONG64 FileCount;
		ULONG64 TotalSize;
		ULONG64 ClusterCount;
	} Defrag;

} DEFRAG_FILES;


//+ Log
#define Log(__pData, __Fmt, ...) \
    { \
		if ((__pData)->fnLogging) \
			(__pData)->fnLogging( (__pData)->lpLoggingParam, __Fmt, __VA_ARGS__ ); \
	}


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

		/// Clear
		ZeroMemory( &Data->Analysis, sizeof( Data->Analysis ) );

		/// Files
		for (f = Data->Files; f; f = f->Next) {

			Log( Data, _T( "Analyze %s\n" ), f->Path );

			/// Refresh fragmentation data
			if (f->Fragments)
				HeapFree( GetProcessHeap(), 0, f->Fragments );
			err = DefragGetFileRetrievalPointers( f->Handle, &f->Fragments, &f->FragmentsSize );
			if (err == ERROR_SUCCESS) {

				Data->Analysis.FileCount++;
				Data->Analysis.MaxFileFragments = max( Data->Analysis.MaxFileFragments, f->Fragments->ExtentCount );
				Data->Analysis.ExtentCount += f->Fragments->ExtentCount;

				for (i = 0; i < f->Fragments->ExtentCount; i++) {

					StartingVcn = (i == 0 ? f->Fragments->StartingVcn.QuadPart : f->Fragments->Extents[i - 1].NextVcn.QuadPart);
					Clusters = f->Fragments->Extents[i].NextVcn.QuadPart - StartingVcn;

					Data->Analysis.ClusterCount += Clusters;
					Data->Analysis.TotalSize += Clusters * f->ClusterSize;

					/// Determine if this fragment and the previous one are contiguous
					if ((NextLcn != 0) && (NextLcn != f->Fragments->Extents[i].Lcn.QuadPart))
						Data->Analysis.DiffuseExtentCount++;
					NextLcn = f->Fragments->Extents[i].Lcn.QuadPart + Clusters;

					Log( Data,
						_T( "  #%04u: Vcn:0x%04I64x, Lcn:0x%08I64x-0x%08I64x, Clusters:%I64u (%I64u bytes)\n" ),
						i + 1,
						StartingVcn,
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

		/// Results
		Log( Data, _T("%s"), _T( "\n" ) );
		Log( Data, _T( "  Files:    %u (most fragmented has %I64u extents)\n" ), Data->Analysis.FileCount, Data->Analysis.MaxFileFragments );
		Log( Data, _T( "  Extents:  %I64u (%I64u clusters, %I64u bytes)\n" ), Data->Analysis.ExtentCount, Data->Analysis.ClusterCount, Data->Analysis.TotalSize );
		Log( Data, _T( "  Diffuse:  %I64u extents\n" ), Data->Analysis.DiffuseExtentCount );

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
		if (Data->Analysis.FileCount == 0)
			return ERROR_INVALID_DATA;					/// Nothing to defragment

		if (Data->Analysis.MaxFileFragments > 1 ||		/// There are fragmented files
			Data->Analysis.DiffuseExtentCount > 0)		/// There are diffuse (not compact) fragments
		{
			LONG64 i, BitmapSequenceStart, BitmapSequenceLen;
			LONG64 BitmapTotalBytes;

			ZeroMemory( &Data->Defrag, sizeof( Data->Defrag ) );

			// Open volume
			Log( Data, _T( "%s" ), _T( "\n" ) );
			Log( Data, _T( "Retrieve %s volume bitmap\n" ), Data->Volume.Name );
			if (!ValidHandle( Data->Volume.Handle )) {
				Data->Volume.Handle = CreateFile( Data->Volume.Name, FILE_READ_DATA | FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );
				if (!ValidHandle( Data->Volume.Handle ))
					err = GetLastError();
			}
			if (err == ERROR_SUCCESS) {

				/// Refresh bitmap
				if (Data->Volume.Bitmap) {
					HeapFree( GetProcessHeap(), 0, Data->Volume.Bitmap );
					Data->Volume.Bitmap = NULL;
					Data->Volume.BitmapSize = 0;
				}
				err = DefragGetVolumeBitmap( Data->Volume.Handle, &Data->Volume.Bitmap, &Data->Volume.BitmapSize );
				if (err == ERROR_SUCCESS) {
					ULONG ClusterSize;
					DefragGetFileClusterSize( Data->Volume.Handle, &ClusterSize );
					Log( Data, _T( "  Bitmap: %I64u clusters (%I64u bytes)\n" ), Data->Volume.Bitmap->BitmapSize.QuadPart, Data->Volume.Bitmap->BitmapSize.QuadPart * ClusterSize );
				}
			}

			if (err == ERROR_SUCCESS) {

				// Search for a new location
				/// Our files require a total of FileClusterCount clusters
				/// Within the volume bitmap, each bit represents a cluster
				/// We'll be searching the bitmap for (FileClusterCount / 8) empty bytes
				/// (Negrutiu)
				BitmapSequenceStart = -1;
				BitmapSequenceLen = (Data->Analysis.ClusterCount + 8) / 8;		/// Round up to a multiple of 8
				BitmapTotalBytes = Data->Volume.Bitmap->BitmapSize.QuadPart / 8;

				for (i = 0; i < BitmapTotalBytes; i++) {
					if (Data->Volume.Bitmap->Buffer[i] == 0) {
						/// The current bitmap byte has 8 free clusters
						if (BitmapSequenceStart >= 0) {
							if (i - BitmapSequenceStart >= BitmapSequenceLen) {
								// Found
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

				if (i < BitmapTotalBytes) {

					DEFRAG_FILE *f;
					ULONG i, BytesRead;

					// Defragment
					LONG64 VolumeLcn = Data->Volume.Bitmap->StartingLcn.QuadPart + BitmapSequenceStart * 8;
					Log( Data, _T( "%s" ), _T( "\n" ) );
					for (f = Data->Files; f && (err == ERROR_SUCCESS); f = f->Next) {
						Log( Data, _T( "Defragment %s\n" ), f->Path );
						if (f->Fragments->ExtentCount > 1)
							Data->Defrag.FileCount++;
						for (i = 0; (i < f->Fragments->ExtentCount) && (err == ERROR_SUCCESS); i++) {

							MOVE_FILE_DATA mfd = {0};
							mfd.FileHandle = f->Handle;
							mfd.StartingVcn.QuadPart = (i == 0 ? f->Fragments->StartingVcn.QuadPart : f->Fragments->Extents[i - 1].NextVcn.QuadPart);
							mfd.StartingLcn.QuadPart = VolumeLcn;
							mfd.ClusterCount = (DWORD)(f->Fragments->Extents[i].NextVcn.QuadPart - mfd.StartingVcn.QuadPart);

							err = DeviceIoControl( Data->Volume.Handle, FSCTL_MOVE_FILE, &mfd, sizeof( mfd ), NULL, 0, &BytesRead, NULL ) ? ERROR_SUCCESS : GetLastError();
							Log( Data,
								_T( "  #%04u: Move %u clusters (%u bytes) {Vcn:0x%04I64x-0x%04I64x -> Lcn:0x%08I64x-0x%08I64x}: 0x%x\n" ),
								i + 1,
								mfd.ClusterCount,
								mfd.ClusterCount * f->ClusterSize,
								mfd.StartingVcn.QuadPart,
								mfd.StartingVcn.QuadPart + mfd.ClusterCount,
								mfd.StartingLcn.QuadPart,
								mfd.StartingLcn.QuadPart + mfd.ClusterCount,
								err
							);

							if (err == ERROR_SUCCESS) {

								/// Advance on disk
								VolumeLcn += mfd.ClusterCount;

								/// Mark the analysis data as dirty
								Data->Analysis.Dirty = TRUE;

								/// Stats
								Data->Defrag.ClusterCount += mfd.ClusterCount;
								Data->Defrag.TotalSize += mfd.ClusterCount * f->ClusterSize;
							}
						}
					}

				} else {
					/// There's no empty space for all files
					err = ERROR_DISK_FULL;
					Log( Data, _T( "%s" ), _T( "  Not enough disk space to perform defragmentation\n" ) );
				}
			} else {
				/// No volume bitmap
				Log( Data, _T( "  Error 0x%x\n" ), err );
			}
		} else {
			Log( Data, _T( "%s" ), _T( "  Nothing to defragment.\n" ) );
		}

	} else {
		err = ERROR_INVALID_PARAMETER;
	}

	return err;
}


//++ DefragmentFile
DWORD DefragmentFile( _In_ LPCTSTR pszFile, _Out_opt_ PULONG64 piBytesMoved, _In_opt_ DefragmentLoggingCallback fnLogging, _In_opt_ LPVOID lpLoggingParam )
{
	LPCTSTR Files[2] = {pszFile, NULL};
	return CompactFiles( Files, piBytesMoved, fnLogging, lpLoggingParam );
}


//++ CompactFiles
DWORD CompactFiles( _In_ LPCTSTR *ppszFileList, _Out_opt_ PULONG64 piBytesMoved, _In_opt_ DefragmentLoggingCallback fnLogging, _In_opt_ LPVOID lpLoggingParam )
{
	DWORD err = ERROR_SUCCESS;
	DEFRAG_FILES Data = {0};

	if (piBytesMoved)
		*piBytesMoved = 0;

	Data.fnLogging = fnLogging;
	Data.lpLoggingParam = lpLoggingParam;

	// Gather data
	err = DefragDataCreate( &Data, ppszFileList );
	if (err == ERROR_SUCCESS) {

		// Analyze fragmentation
		err = DefragDataAnalyze( &Data );
		if (err == ERROR_SUCCESS) {

			// Defragment and compact all files
			err = DefragDataDefragment( &Data );
			if (err == ERROR_SUCCESS) {

				if (piBytesMoved)
					*piBytesMoved = Data.Defrag.TotalSize;
			}
		}
	}

	DefragDataDestroy( &Data );
	return err;
}


#ifdef __cplusplus
}
#endif
