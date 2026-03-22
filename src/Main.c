#include "StdAfx.h"
#include "Defragment.h"
#include "FileList.h"

#define ENABLE_PROMPT
#define ENABLE_FRAGMENTATION
#define DEFAULT_TARGET_FRAGMENT_COUNT		5


DWORD VersionString(HMODULE module, LPCTSTR stringName, LPTSTR stringData, ULONG stringCapacity)
{
    DWORD err = ERROR_SUCCESS;
    module = (module ? module : GetModuleHandle(NULL));
    if (stringData)
        stringData[0] = 0;

    if (stringName && stringName[0] && stringData) {
        HRSRC versionRes = FindResource(module, MAKEINTRESOURCE(VS_VERSION_INFO), RT_VERSION);
        if (versionRes) {
            HGLOBAL versionGlobal = LoadResource(module, versionRes);
            if (versionGlobal) {
                void* versionPtr = LockResource(versionGlobal);
                if (versionPtr) {

                    struct {
                        WORD language;
                        WORD codepage;
                    }* codepage;
                    UINT length = sizeof(*codepage);

                    if (VerQueryValue(versionPtr, L"\\VarFileInfo\\Translation", (LPVOID*)&codepage, &length)) {
                        TCHAR* stringPtr = NULL;
                        TCHAR subblock[128];
                        _stprintf_s(subblock, _countof(subblock), _T("\\StringFileInfo\\%04hx%04hx\\%s"), codepage->language, codepage->codepage, stringName);

                        if (VerQueryValue(versionPtr, subblock, (void**)&stringPtr, &length)) {
                            _tcsncpy_s(stringData, stringCapacity, stringPtr, length);
                        } else {
                            err = GetLastError();    // ERROR_RESOURCE_TYPE_NOT_FOUND
                        }
                    } else {
                        err = GetLastError();    // ERROR_RESOURCE_TYPE_NOT_FOUND
                    }
                }
            }
        } else {
            err = GetLastError();
        }
    } else {
        err = ERROR_INVALID_PARAMETER;
    }
    return err;
}


//++ PrintHeader
void PrintHeader()
{
    TCHAR description[64], copyright[128], company[64], version[32];
    VersionString(NULL, _T("FileDescription"), description, _countof(description));
    VersionString(NULL, _T("LegalCopyright"), copyright, _countof(copyright));
    VersionString(NULL, _T("CompanyName"), company, _countof(company));
    VersionString(NULL, _T("FileVersion"), version, _countof(version));

    _tprintf(_T("\n%s [%s]\n%s\n%s\n\n"), description, version, company, copyright);
}


void PrintUsage()
{
    TCHAR path[MAX_PATH], *filename;

    path[0] = 0;
    GetModuleFileName(NULL, path, ARRAYSIZE(path));
    filename = _tcsrchr(path, _T('\\'));
    if (filename) {
        filename++;
    } else {
        filename = path;
    }

    _tprintf(
        _T("Usage:\n")
        _T("  %s analyze <files>\n")
        _T("  %s defragment [--compact] [--simulate] <files>\n")
#ifdef ENABLE_FRAGMENTATION
        _T("  %s fragment [--count=N] [--simulate] <files>\n")
#endif
#ifdef ENABLE_PROMPT
#ifdef ENABLE_FRAGMENTATION
        _T("  %s [prompt] [--count=N] [--compact] [--simulate] <files>\n")
#else
        _T("  %s [prompt] [--compact] [--simulate] <files>\n")
#endif
#endif
        _T("\n")
        _T("Commands:\n")
        _T("  analyze     Analyze file fragments\n")
        _T("  defragment  Defragment files\n")
#ifdef ENABLE_FRAGMENTATION
        _T("  fragment    Fragment files\n")
#endif
#ifdef ENABLE_PROMPT
        _T("  prompt      Interactive mode (default command)\n")
#endif
        _T("\n")
        _T("Options:\n")
        _T("  --compact   Move files together to a contiguous disk area\n")
#ifdef ENABLE_FRAGMENTATION
        _T("  --count=N   Break files into this many fragments. Default is %d\n")
#endif
        _T("  --simulate  Conduct a dry run. Nothing is actually written to disk\n")
        _T("  files       [@]File1 [@]File2 ... [@]FileN\n")
        _T("\n")
        _T("Files:\n")
        _T("  Filenames can be either files or directories. Wildcards are accepted\n")
        _T("  Filenames prefixed by \"@\" are treated as textfile catalogs (one file per line)\n")
        _T("\n")
        _T("Examples:\n")
        _T("  %s analyze C:\\Dir\\File1 \"C:\\Dir with spaces\\File*.jpg\" \"@C:\\Dir with spaces\\Catalog.txt\"\n")
        _T("  %s defragment --compact --simulate \"@C:\\Dir with spaces\\FileCatalog.txt\"\n")
#ifdef ENABLE_FRAGMENTATION
        _T("  %s fragment --count=15 \"C:\\Dir with spaces\\File1.iso\"\n")
#endif
#ifdef ENABLE_PROMPT
        _T("  %s prompt \"@C:\\Dir with spaces\\FileCatalog.txt\"\n")
#endif
        _T("\n"),
        filename,
        filename,
#ifdef ENABLE_FRAGMENTATION
        filename,
#endif
#ifdef ENABLE_PROMPT
        filename,
#endif
#ifdef ENABLE_FRAGMENTATION
        DEFAULT_TARGET_FRAGMENT_COUNT,
#endif
        filename,
        filename
#ifdef ENABLE_PROMPT
        ,
        filename
#endif
#ifdef ENABLE_FRAGMENTATION
        ,
        filename
#endif
    );
}


LPTSTR FormatError( _In_ DWORD err, _Out_ LPTSTR pszError, _In_ ULONG iErrorLen )
{
	if (pszError && iErrorLen) {
		DWORD len = 0;
		pszError[0] = _T( '\0' );
		if ((len = FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), pszError, iErrorLen, NULL )) == 0) {
			if ((len = FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE, GetModuleHandle( _T( "ntdll" ) ), err, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), pszError, iErrorLen, NULL )) == 0) {
				/// Try others...
			}
		}
		if (len > 0) {
			/// Trim trailing noise
			for (len--; (len > 0) && (pszError[len] == _T( '.' ) || pszError[len] == _T( ' ' ) || pszError[len] == _T( '\r' ) || pszError[len] == _T( '\n' )); len--)
				pszError[len] = _T( '\0' );
			len++;
		}
	}
	return pszError;
}


VOID DefragLog( _In_ LPVOID lpParam, _In_ LPCTSTR pszFmt, _In_ ... )
{
	va_list args;
	UNREFERENCED_PARAMETER( lpParam );
	va_start( args, pszFmt );
	_vtprintf( pszFmt, args );
	va_end( args );
}


BOOL DefragTrace( _In_ LPVOID lpParam, _In_ int iStep, _In_opt_ LPVOID pParam1, _In_opt_ LPVOID pParam2 )
{
	LPBOOL pbInteractive = (LPBOOL)lpParam;
	switch (iStep)
	{
		case DEFRAG_STEP_BEFORE_ANALYSIS:
			///_tprintf( _T( "----- {%hs} Start Analysis...\n" ), __FUNCTION__ );
			break;
		case DEFRAG_STEP_ANALYZE_FILE:
			///_tprintf( _T( "----- {%hs} Analyze %s\n" ), __FUNCTION__, (LPCTSTR)pParam1 );
			break;
		case DEFRAG_STEP_BEFORE_MOVE:
			///_tprintf( _T( "----- {%hs} Start moving {Flags:0x%x, Interactive:%s}\n" ), __FUNCTION__, ((PDEFRAG_OPTIONS)pParam1)->Flags, *pbInteractive ? _T( "true" ) : _T( "false" ) );
#ifdef ENABLE_PROMPT
			if (*pbInteractive) {

				TCHAR szInput[256];
				PDEFRAG_OPTIONS pOptions = (PDEFRAG_OPTIONS)pParam1;
				assert( pOptions );

				_tprintf(
					_T( "\n" )
					_T( "Actions:\n" )
					_T( "  s         Toggle simulation ON or OFF\n" )
					_T( "  c         Toggle compact defragmenting ON or OFF\n" )
#ifdef ENABLE_FRAGMENTATION
					_T( "  fc <N>    Set target fragment count\n" )
#endif
					_T( "\n" )
					_T( "  d         Begin defragmenting files\n" )
#ifdef ENABLE_FRAGMENTATION
					_T( "  f         Begin fragmenting files\n" )
#endif
					_T( "  q         Quit\n" )
					_T( "\n" )
				);

				while( TRUE ) {
					
#ifdef ENABLE_FRAGMENTATION
					_tprintf( _T( "Action (Simulate:%s, Compact:%s, Fragmentats:%u) : " ), pOptions->Flags & DEFRAG_FLAG_SIMULATE ? _T( "ON" ) : _T( "OFF" ), pOptions->Flags & DEFRAG_FLAG_COMPACT ? _T( "ON" ) : _T( "OFF" ), pOptions->TargetFragmentCount );
#else
					_tprintf( _T( "Action (Simulate:%s, Compact:%s) : " ), pOptions->Flags & DEFRAG_FLAG_SIMULATE ? _T( "ON" ) : _T( "OFF" ), pOptions->Flags & DEFRAG_FLAG_COMPACT ? _T( "ON" ) : _T( "OFF" ) );
#endif
					
					_tscanf( _T( "%255s" ), szInput );
					if (CompareString( CP_ACP, NORM_IGNORECASE, szInput, -1, _T( "c" ), -1 ) == CSTR_EQUAL) {
						pOptions->Flags ^= DEFRAG_FLAG_COMPACT;
#ifdef ENABLE_FRAGMENTATION
					} else if (CompareString( CP_ACP, NORM_IGNORECASE, szInput, -1, _T( "fc" ), -1 ) == CSTR_EQUAL) {
						_tscanf( _T( "%255s" ), szInput );
						pOptions->TargetFragmentCount = (ULONG)_tcstoul( szInput, NULL, 10 );
#endif
					} else if (CompareString( CP_ACP, NORM_IGNORECASE, szInput, -1, _T( "s" ), -1 ) == CSTR_EQUAL) {
						pOptions->Flags ^= DEFRAG_FLAG_SIMULATE;
					} else if (CompareString( CP_ACP, NORM_IGNORECASE, szInput, -1, _T( "d" ), -1 ) == CSTR_EQUAL) {
						pOptions->Flags &= ~DEFRAG_FLAG_FRAGMENT;
						return TRUE;	/// Continue defragmenting
#ifdef ENABLE_FRAGMENTATION
					} else if (CompareString( CP_ACP, NORM_IGNORECASE, szInput, -1, _T( "f" ), -1 ) == CSTR_EQUAL) {
						pOptions->Flags |= DEFRAG_FLAG_FRAGMENT;
						return TRUE;	/// Continue fragmenting
#endif
					} else if (CompareString( CP_ACP, NORM_IGNORECASE, szInput, -1, _T( "q" ), -1 ) == CSTR_EQUAL) {
						return FALSE;	/// Abort everything
					} else {
						_tprintf( _T( "  Unknown action \"%s\"\n" ), szInput );
					}
				}
			}
#endif
			break;
		case DEFRAG_STEP_MOVE_FILE:
			///_tprintf( _T( "----- {%hs} Defragment %s (%I64u bytes)\n" ), __FUNCTION__, (LPCTSTR)pParam1, *(PLONG64)pParam2 );
			break;
		case DEFRAG_STEP_MOVE_EXTENT:
			///_tprintf( _T( "----- {%hs} Defragment %s extent (%I64u bytes)\n" ), __FUNCTION__, (LPCTSTR)pParam1, *(PLONG64)pParam2 );
			break;
	}
	return TRUE;
}


//++ _tmain
int __cdecl _tmain( _In_ int argc, _In_ _TCHAR* argv[], _In_ _TCHAR* envp[] )
{
#define COMMAND_NONE		0
#define COMMAND_ANALYZE		1
#define COMMAND_DEFRAG		2	// defragment, fragment, prompt

	DWORD err = ERROR_SUCCESS;
	ULONG command = COMMAND_NONE;
	DEFRAG_OPTIONS options = {0};
	BOOL prompt = FALSE;
	int i, i0;
	PFILE_LIST filelist = NULL;

#ifdef _M_IX86
	typedef BOOL(WINAPI * TypeWow64DisableWow64FsRedirection)(PVOID * ppOldValue);
    typedef BOOL(WINAPI * TypeWow64RevertWow64FsRedirection)(PVOID pOlValue);
    TypeWow64DisableWow64FsRedirection fnDisableRedirection =
        (TypeWow64DisableWow64FsRedirection)GetProcAddress(GetModuleHandle(_T("kernel32")), "Wow64DisableWow64FsRedirection");
	TypeWow64RevertWow64FsRedirection fnRevertRedirection =
        (TypeWow64RevertWow64FsRedirection)GetProcAddress(GetModuleHandle(_T("kernel32")), "Wow64RevertWow64FsRedirection");

    PVOID pRedirectionState = NULL;
	if (fnDisableRedirection) {
        fnDisableRedirection(&pRedirectionState);
    }
#endif

	PrintHeader();

	/// Allocate data
	filelist = (PFILE_LIST)HeapAlloc( GetProcessHeap(), 0, sizeof( *filelist ) );
	if (!filelist)
		return (int)GetLastError();
	filelist->Count = 0;
	filelist->ppszFiles[0] = NULL;

	/// Try to determine whether the first argument is the executable name, or a command (starting with / or --)
	/// If *not* a command, we'll skip it
	i0 = 0;
	if (argc > 0 && (argv[0][0] != _T( '/' )) && (argv[0][0] != _T( '-' )))
		i0 = 1;

	// Command line
	for (i = i0; i < argc; i++)
	{
		// Commands
		if (command == COMMAND_NONE && CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "analyze" ), -1 ) == CSTR_EQUAL) {

			command = COMMAND_ANALYZE;

		} else if (command == COMMAND_NONE && CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "defrag" ), -1 ) == CSTR_EQUAL || CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "defragment" ), -1 ) == CSTR_EQUAL) {

			command = COMMAND_DEFRAG;
			options.Flags &= ~DEFRAG_FLAG_FRAGMENT;
			options.TargetFragmentCount = 0;

#ifdef ENABLE_FRAGMENTATION
		} else if (command == COMMAND_NONE && CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "fragment" ), -1 ) == CSTR_EQUAL) {

			command = COMMAND_DEFRAG;
			options.Flags |= DEFRAG_FLAG_FRAGMENT;
			options.TargetFragmentCount = DEFAULT_TARGET_FRAGMENT_COUNT;
#endif
#ifdef ENABLE_PROMPT
		} else if (command == COMMAND_NONE && CompareString( CP_ACP, NORM_IGNORECASE, argv[i], -1, _T( "prompt" ), -1 ) == CSTR_EQUAL) {

			prompt = TRUE;
			command = COMMAND_DEFRAG;
			options.Flags &= ~DEFRAG_FLAG_FRAGMENT;
			options.TargetFragmentCount = DEFAULT_TARGET_FRAGMENT_COUNT;
#endif
		} else {

			if (command == COMMAND_NONE)
            {
				// no command specified. use defaults
#ifdef ENABLE_PROMPT
                prompt = TRUE;
                command = COMMAND_DEFRAG;
                options.Flags &= ~DEFRAG_FLAG_FRAGMENT;
                options.TargetFragmentCount = DEFAULT_TARGET_FRAGMENT_COUNT;
#else
                command = COMMAND_ANALYZE;
#endif

            }

			// Options
			if (argv[i][0] == _T( '-' ) || argv[i][0] == _T( '/' )) {

                LPCTSTR option = argv[i] + 1;
                if (option[0] == _T('-')) {
                    option++;
                }

                if ((command == COMMAND_DEFRAG) && !(options.Flags & DEFRAG_FLAG_FRAGMENT) &&
                    (CompareString(CP_ACP, NORM_IGNORECASE, option, -1, _T( "compact" ), -1) == CSTR_EQUAL))
                {

					options.Flags |= DEFRAG_FLAG_COMPACT;

#ifdef ENABLE_FRAGMENTATION
				} else if ((command == COMMAND_DEFRAG) && (CompareString( CP_ACP, NORM_IGNORECASE, option, 6, _T( "count=" ), -1 ) == CSTR_EQUAL) && (i + 1 < argc)) {

					options.TargetFragmentCount = (ULONG)_tcstoul( option + 6, NULL, 10 );	// --count=123
#endif
				} else if ((command == COMMAND_DEFRAG) && (CompareString( CP_ACP, NORM_IGNORECASE, option, -1, _T( "simulate" ), -1 ) == CSTR_EQUAL)) {

					options.Flags |= DEFRAG_FLAG_SIMULATE;
				
				} else {
					_tprintf( _T( "WARNING: Unexpected option \"%s\"\n" ), argv[i] );
				}

			} else {

				// Files
				if (argv[i][0] == _T( '@' )) {
					FileListAddCatalog( filelist, argv[i] + 1 );
				} else {
					FileListAddFile( filelist, argv[i] );
				}

			}
		}
	}	/// for


	// Execute command
	options.fnLogging = DefragLog;
	options.lpLoggingParam = NULL;
	options.fnTracing = DefragTrace;
	options.lpTracingParam = &prompt;

	switch (command)
	{
		case COMMAND_ANALYZE:
			_tprintf( _T( "\n" ) );
			err = DefragAnalyzeFiles( filelist->ppszFiles, &options, NULL );
			if (TRUE) {
				TCHAR szError[255];
				_tprintf( _T( "\n" ) );
				_tprintf( _T( "Error 0x%x \"%s\"\n" ), err, FormatError( err, szError, ARRAYSIZE( szError ) ) );
			}
			break;

		case COMMAND_DEFRAG:
			_tprintf( _T( "\n" ) );
			err = DefragMoveFiles( filelist->ppszFiles, &options, NULL );
			if (TRUE) {
				TCHAR szError[255];
				_tprintf( _T( "\n" ) );
				_tprintf( _T( "Error 0x%x \"%s\"\n" ), err, FormatError( err, szError, ARRAYSIZE( szError ) ) );
			}
			break;

		default:
			err = ERROR_INVALID_PARAMETER;
			PrintUsage();
			_tprintf( _T( "ERROR: Invalid command\n" ) );
	}

	FileListDestroy( filelist );
	HeapFree( GetProcessHeap(), 0, filelist );

	// Pause
	if (prompt && (err != ERROR_REQUEST_ABORTED)) {
		_tprintf( _T( "Press any key to exit . . . " ) );
		_gettch();
		_tprintf( _T( "\n" ) );
	}

#ifdef _M_IX86
    if (fnRevertRedirection) {
        fnRevertRedirection(pRedirectionState);
    }
#endif

	return (int)err;
}
