# disp
Simple display settings manager for Windows 7+ that lives in the system tray.

## How to compile
Use [mingw-w64](https://mingw-w64.org/) with Msys2.

1. Install gcc and other build tools:
   ```bash
   $ pacman -S mingw-w64-x86_64-toolchain make
   ```

2. Install dependencies (Jansson):
   ```bash
   $ pacman -S mingw64/mingw-w64-x86_64-jansson
   ```

3. Build:
   ```bash
   $ cd project-directory
   $ make
   ```
   Or if you want a stripped release binary, use `make release`.

   The built binary can be found in the `bin` folder.

## Configuration
disp stores its configuration in a JSON file, see `disp_config.example.json` for the format.

By default disp checks the working directory for `disp_config.json`. If the file doesn't exist, disp uses the local AppData folder `Zini.Disp` and creates `config.json` there.

If the AppData folder creation fails, disp will fall back to creating `disp_config.json` in the working directory.

You can give the config file path as a command line argument by using `-c <path>` or `--config <path>`. The path specified in the command line argument always takes priority. If the config file doesn't exist, it will be created using default settings.