# BlueraybackupEx

Blu-ray Disc backup tool that uses libbluray to extract content from Blu-ray discs or disc image files.

## Usage

```
bluraybackup-ex {-d device | -i input} [-k keyfile] [-b size] [-o outdir]
bluraybackup-ex {-d device | -i input} [-k keyfile] [-b size] -m [-o outdir]
```

## Command-line options

| Option | Long form | Description |
|------:|--------|------|
| `-d PATH` | `--device=PATH` | Physical Blu-ray device path. Example (Linux): `/dev/sr0`; (Windows): `D:` |
| `-i PATH` | `--input=PATH` | Local disc image file (ISO/BIN) or a directory containing a BDMV structure |
| `-k FILE` | `--keydb=FILE` | Path to the AACS keys database file |
| `-m` | `--main` | Extract only the main title and save as `main_title.m2ts` |
| `-o DIR` | `--output=DIR` | Output directory |
| `-b SIZE` | `--buffer=SIZE` | I/O read buffer size (e.g. `6144`, `60k`, `6m`,). Must be ≥ 6144. Default: `6144`. |
| `-h` | `--help` | Show help information |
| `-v` | `--version` | Show version and license information |

## Detailed options

### Input source (`-d` / `-i`, choose one)

- **`-d`**: Specify a physical Blu-ray device path (e.g. `/dev/sr0`, `D:`).
- **`-i`**: Specify a local disc image file (ISO/BIN) or a folder containing the BDMV directory structure.
- `-d` and `-i` are **mutually exclusive**; do not use both.
- If neither is provided, the default device is used (Linux: `/dev/sr0`, Windows: `D:`).

### Key database (`-k`)

Specify the AACS keys database file. If not provided, the program will search these locations:

- Windows: `%APPDATA%\aacs\KEYDB.cfg`
- Linux/macOS: `~/.config/aacs/KEYDB.cfg`

It will also try filename variants like `keydb.cfg`, `KeyDB.cfg`, and `KEYDB.CFG`.

### Output directory (`-o`)

- Specify the output directory for backup files. The directory will be created if it does not exist.
- If `-o` is not specified:
  - When using `-i`, output is written to the directory containing the image file.
  - Otherwise, output is written to the current working directory.

### Main-title mode (`-m`)

- Extract only the disc's main-title stream and save it as `main_title.m2ts`.
- The file is saved to the `-o` directory if specified, or to the default output location.

### I/O buffer size (`-b`)

Controls how many bytes are read from the source and written to the destination in a single operation.

- Default: `6144` bytes (one AACS encryption block, 3 × 2048-byte BD sectors). Safe for physical drives with scratched discs — smaller reads mean more precise error recovery.
- For ISO images or fast SSDs, a larger value significantly improves throughput:
  - `60k` — minor boost, still conservative
  - `6m` — recommended for ISO/SSD backups, optimal for most case.
- Value must be at least `6144`. Larger sizes do not need to be a multiple of `6144`, though aligned values can still be a sensible choice.

Supported suffixes: `k`/`K` (×1024), `m`/`M` (×1024²), `g`/`G` (×1024³), or plain bytes.

## Examples

```bash
# Backup an entire disc from a physical drive to a specified directory
bluraybackup-ex -d /dev/sr0 -o ~/backup

# Extract an entire disc from an ISO image (output to the ISO's directory)
bluraybackup-ex -i /path/to/disc.iso

# Extract an entire disc from an ISO into a specific directory
bluraybackup-ex -i /path/to/disc.iso -o /output/dir

# Extract only the main title from a physical drive
bluraybackup-ex -d D: -m -o C:\backup

# Extract from a BDMV directory and specify a keys file
bluraybackup-ex -i /path/to/BDMV_DIR -k /path/to/KEYDB.cfg -o ~/output

# Use a 1 MB buffer for faster ISO extraction
bluraybackup-ex -i /path/to/disc.iso -b 1m -o /output/dir

# Use a conservative 64 KB buffer for a scratched physical disc
bluraybackup-ex -d /dev/sr0 -b 64k -o ~/backup
```

## Exit codes

- `0`: Backup completed successfully
- `1`: An error occurred

## Build

### Windows (MSVC + vcpkg)

**Prerequisites:** [Visual Studio 2026](https://visualstudio.microsoft.com/) with the "Desktop development with C++" workload, and [vcpkg](https://github.com/microsoft/vcpkg).

```powershell
# 1. Set up vcpkg (skip if already done)
git clone https://github.com/microsoft/vcpkg "$env:USERPROFILE\vcpkg"
& "$env:USERPROFILE\vcpkg\bootstrap-vcpkg.bat"
$env:VCPKG_ROOT = "$env:USERPROFILE\vcpkg"   # also set this permanently in system env vars

# 2. Clone and build — vcpkg.json auto-installs libbluray
git clone https://github.com/kingsznhone/bluraybackupEx
cd bluraybackupEx
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release
pwsh ./tools/collect-windows-artifacts.ps1
# Output: dist\windows\bluraybackup-ex.exe
```

### Linux (GCC / Clang + CMake)

**Prerequisites:** `gcc`, `cmake`, `ninja-build`, and `libbluray-dev`.

```bash
# Debian / Ubuntu
sudo apt install gcc cmake ninja-build libbluray-dev

# Fedora / RHEL
sudo dnf install gcc cmake ninja-build libbluray-devel

# Arch Linux
sudo pacman -S gcc cmake ninja libbluray

# Clone and build
git clone https://github.com/kingsznhone/bluraybackupEx
cd bluraybackupEx
cmake --preset linux-release
cmake --build --preset linux-release
# Output: build/bluraybackup-ex

# Optional: install system-wide
sudo cmake --install build
```

### Debian package build, install, and removal

On Debian or Ubuntu you can also build a native `.deb` package and collect the artifacts into `dist/debian/`:

```bash
sudo apt install build-essential cmake debhelper dpkg-dev libbluray-dev

# Build the binary package
sh tools/build-deb.sh

# Install the generated package
sudo apt install ./dist/debian/bluraybackup-ex_*.deb

# Verify the binary and man page
bluraybackup-ex --version
man bluraybackup-ex

# Remove the package
sudo dpkg -r bluraybackup-ex

# Optional: purge package state (if any conffiles are added in the future)
sudo dpkg -P bluraybackup-ex
```

## Versioning

The top-level `VERSION` file is the single source of truth for the upstream version.

- `CMakeLists.txt` reads `VERSION` and uses it as `PROJECT_VERSION`.
- The program's `--version` output uses that CMake project version.
- `vcpkg.json` and `debian/changelog` should be updated together with the upstream version.

Use the helper script to bump versions consistently:

```powershell
pwsh ./tools/bump-version.ps1 -Version 2.2.0
```

To publish a new Debian revision without changing the upstream version:

```powershell
pwsh ./tools/bump-version.ps1 -Version 2.2.0 -DebianRevision 2
```

## AACS / BD+ libraries

To read protected Blu-ray discs you need **libaacs** (for AACS) and optionally **libbdplus** (for BD+). These are **not** bundled with this program and must be installed separately.

- **Windows:** Download pre-built DLLs from <https://github.com/KnugiHK/libaacs-libbdplus-windows> and place them somewhere on your `PATH` (e.g. `C:\Windows\System32` or next to `bluraybackup-ex.exe`).
- **Linux:** Install via your package manager (e.g. `sudo apt install libaacs0 libbdplus0`).

You also need a valid AACS keys database file (`KEYDB.cfg`). If you don't pass `-k`, the program searches these locations in order:

**Windows:**
1. `%APPDATA%\aacs\KEYDB.cfg`
2. Same path with filename variants: `keydb.cfg`, `KeyDB.cfg`, `KEYDB.CFG`

**Linux / macOS:**
1. `~/.config/aacs/KEYDB.cfg`
2. Same path with filename variants: `keydb.cfg`, `KeyDB.cfg`, `KEYDB.CFG`

If `%APPDATA%` (Windows) or `$HOME` (Linux/macOS) is not set, the fallback is `KEYDB.cfg` in the current working directory.

## 📦 Help Get This into `apt`

Currently, this tool can only be installed by cloning the repo and building from source.  

A `debian/` directory is included, and the package passes `lintian` (no output = clean).

If you’re a Debian Developer or Maintainer and can **sponsor** this package for official inclusion (i.e., review & upload to Debian), please get in touch!  

*(Note: This is technical sponsorship—no money involved. See [Debian Sponsorship](https://wiki.debian.org/Sponsorship).)*

## Testing and Limitations

Testing note: I do not have access to a physical Blu-ray drive, so I could not test extraction from real discs. The physical-device conversion code remains as close as possible to the original author's implementation. I have tested extraction from raw ISO dump files on both Windows and Ubuntu.

## License

GPLv3+

## Contributors

- **Matteo Bini** — Original author.
- **kingsznhone** — Major improvements and contributions: Windows port modernization, C17 upgrade, `-b` buffer option, CMake build system, and documentation.

