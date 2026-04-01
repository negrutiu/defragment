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

## Example
> defragment.exe prompt C:\Downloads\binaries.zip

```
De|Fragment Tool [26.3.29.59]
https://github.com/negrutiu/defragment
(C)2017-2026, Marius Negrutiu. All rights reserved.

Analyze "C:\Downloads\binaries.zip"
  #0001: Vcn:0x0000-0x0106, Lcn:0x02d3da96-0x02d3db9c, Clusters:262 (1073152 bytes)
  #0002: Vcn:0x0106-0x01b0, Lcn:0x03b9b358-0x03b9b402, Clusters:170 (696320 bytes)
  #0003: Vcn:0x01b0-0x06b0, Lcn:0x03d22e58-0x03d23358, Clusters:1280 (5242880 bytes)
  #0004: Vcn:0x06b0-0x0bb0, Lcn:0x02907a34-0x02907f34, Clusters:1280 (5242880 bytes)
  #0005: Vcn:0x0bb0-0x10b0, Lcn:0x053b1734-0x053b1c34, Clusters:1280 (5242880 bytes)
  #0006: Vcn:0x10b0-0x15b5, Lcn:0x04a15d57-0x04a1625c, Clusters:1285 (5263360 bytes)
  #0007: Vcn:0x15b5-0x1ab1, Lcn:0x023a7f10-0x023a840c, Clusters:1276 (5226496 bytes)
  #0008: Vcn:0x1ab1-0x1fb9, Lcn:0x02941e12-0x0294231a, Clusters:1288 (5275648 bytes)
  #0009: Vcn:0x1fb9-0x24b1, Lcn:0x0451e3a8-0x0451e8a0, Clusters:1272 (5210112 bytes)
  #0010: Vcn:0x24b1-0x29b9, Lcn:0x05cfc820-0x05cfcd28, Clusters:1288 (5275648 bytes)
  #0011: Vcn:0x29b9-0x2eb1, Lcn:0x047e5150-0x047e5648, Clusters:1272 (5210112 bytes)
  #0012: Vcn:0x2eb1-0x33bd, Lcn:0x06907d4c-0x06908258, Clusters:1292 (5292032 bytes)
  #0013: Vcn:0x33bd-0x38b5, Lcn:0x0678df74-0x0678e46c, Clusters:1272 (5210112 bytes)
  #0014: Vcn:0x38b5-0x3db1, Lcn:0x034226c8-0x03422bc4, Clusters:1276 (5226496 bytes)
  #0015: Vcn:0x3db1-0x42c0, Lcn:0x0c1c93eb-0x0c1c98fa, Clusters:1295 (5304320 bytes)
  #0016: Vcn:0x42c0-0x47b2, Lcn:0x0dfb7c54-0x0dfb8146, Clusters:1266 (5185536 bytes)
  #0017: Vcn:0x47b2-0x4cd6, Lcn:0x026fffc4-0x027004e8, Clusters:1316 (5390336 bytes)
  #0018: Vcn:0x4cd6-0x51b2, Lcn:0x01923728-0x01923c04, Clusters:1244 (5095424 bytes)
  #0019: Vcn:0x51b2-0x56d6, Lcn:0x045a54b4-0x045a59d8, Clusters:1316 (5390336 bytes)
  #0020: Vcn:0x56d6-0x5bb4, Lcn:0x044b8e8a-0x044b9368, Clusters:1246 (5103616 bytes)
  #0021: Vcn:0x5bb4-0x60b0, Lcn:0x040188e4-0x04018de0, Clusters:1276 (5226496 bytes)
  #0022: Vcn:0x60b0-0x65dc, Lcn:0x005c1170-0x005c169c, Clusters:1324 (5423104 bytes)
  #0023: Vcn:0x65dc-0x66a8, Lcn:0x050d7704-0x050d77d0, Clusters:204 (835584 bytes)

  Files:    1 (most fragmented has 23 extents, least fragmented has 23 extents)
  Extents:  23 (26280 clusters, 107642880 bytes)
  Diffuse:  22 extents
  Note:     Nothing was written to disk

Actions:
> d         Begin defragmenting files
  dc        Toggle compact defragmenting ON or OFF

> f         Begin fragmenting files
  fc <N>    Set target fragment count

  s         Toggle simulation ON or OFF
  q         Quit

Action (simulate:OFF, compact:OFF, fragments:5) : ...
 ```

## Integration
Create a shortcut to `defragment.exe` and place it in the `Send to` folder for easy access.
> [!IMPORTANT]
> Remember to replace `<path>\defragment.exe` in the command below.
```cmd
powershell -ExecutionPolicy Bypass -c "$lnk = (New-Object -ComObject WScript.Shell).CreateShortcut($env:APPDATA+'\Microsoft\Windows\SendTo\Defragment Files.lnk'); $lnk.TargetPath='<path>\defragment.exe'; $lnk.Arguments='prompt'; $lnk.Save()"
```

## Requirements
Windows XP and newer.

## License
This project is licensed under the [MIT License](LICENSE).  
Third-party assets with different terms are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
