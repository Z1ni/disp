# disp
Simple display settings manager for Windows 7+ that lives in the system tray.

## How to compile
Use [mingw-w64](https://mingw-w64.org/) with Msys2.

1. Install gcc and other build tools:
   ```bash
   $ pacman -S mingw-w64-x86_64-toolchain make
   ```

2. Install dependencies (libconfig):
   ```bash
   $ pacman -S mingw64/mingw-w64-x86_64-libconfig
   ```

3. Build:
   ```bash
   $ cd project-directory
   $ make
   ```
   Or if you want a stripped release binary, use `make release`.

   The built binary can be found in the `bin` folder.