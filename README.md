# disp
Simple display settings manager for Windows 7+ that lives in the system tray.

## How to compile
Use [mingw-w64](https://mingw-w64.org/) with Msys2.

First, install libconfig:
```bash
$ pacman -S mingw64/mingw-w64-x86_64-libconfig
```

Then build:
```bash
$ cd project-directory
$ make
```
Or if you want a stripped release binary, use `make release` and check the bin folder.
