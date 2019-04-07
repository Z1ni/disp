# disp
Simple display settings manager for Windows 7+ that lives in the system tray.

## How to compile
Use [mingw-w64](https://mingw-w64.org/) with Msys2.

```bash
$ cd project-directory
$ make
```
Or if you want a stripped release binary, use `make release` and check the bin folder.

## Task list
- [x] Show information about displays
- [x] Show displays' friendly names
- [x] Support changing display orientation
- [ ] Support changing virtual display positions
- [ ] Support saving current display configuration
- [ ] Support loading display configurations
- [ ] Update automatically when system display configuration is changed
