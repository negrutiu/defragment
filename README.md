# De|Fragment Tool
![Rubiks Cube](src/res/Rubik.ico)

This is an experimental project to explore the use of file defragmentation techiques in Windows.
It can be used to defragment files, or to fragment them in a controlled way for testing purposes.

The project interacts with the following filesystem control codes to manipulate file fragments:
- [FSCTL_GET_VOLUME_BITMAP](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_get_volume_bitmap)
- [FSCTL_GET_RETRIEVAL_POINTERS](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_get_retrieval_pointers)
- [FSCTL_MOVE_FILE](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_move_file)

## Usage
The tool is a cli app that accepts the following arguments:

```bat
defragment.exe analyze C:\Dir\File1 "C:\Dir\File*.ext" "@C:\Dir\FileCatalog.txt"
defragment.exe defragment [--compact] [--simulate] "C:\Dir\File*.ext" "@C:\Dir\FileCatalog.txt"
defragment.exe fragment [--count=15] "C:\Dir\File1.iso"
defragment.exe prompt "C:\Dir\File*.ext" "@C:\Dir\FileCatalog.txt"
```

See full usage instructions by running `defragment.exe` with no arguments.

## Integration
Create a shortcut to `defragment.exe` and place it in the `Send to` folder for easy access.
> [!IMPORTANT]
> Remember to replace `<path>\defragment.exe` in the command below.
```cmd
powershell -ExecutionPolicy Bypass -c "$lnk = (New-Object -ComObject WScript.Shell).CreateShortcut($env:APPDATA+'\Microsoft\Windows\SendTo\Defragment Files.lnk'); $lnk.TargetPath='<path>\defragment.exe'; $lnk.Arguments='prompt'; $lnk.Save()"
```

## Requirements
Windows XP up to latest Windows 11.

## License
This project is licensed under the [MIT License](LICENSE).  
Third-party assets with different terms are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
