# BlueraybackupEx

Blu-ray Disc backup tool that uses libbluray to extract content from Blu-ray discs or disc image files.

## Usage

```
bluraybackup-ex <subcommand> [-k keyfile] [-b size] [...]
```

| Subcommand | Synopsis | Description |
|---|---|---|
| `extract` | `extract [-k keyfile] [-b size] <source> <output-dir>` | Extract the full disc file tree into `<output-dir>/<disc-label>/` |
| `patch` | `patch [-k keyfile] [-b size] <source.iso> <output.iso>` | Write a single decrypted ISO image |
| `check` | `check [-k keyfile] [-b size] <source>` | Verify disc readability and AACS MAC integrity |

Run `bluraybackup-ex <subcommand> --help` for subcommand-specific details.

## Options

| Option | Long form | Description |
|------:|--------|------|
| `-k FILE` | `--keydb=FILE` | Path to the AACS keys database file |
| `-b SIZE` | `--buffer=SIZE` | I/O read buffer size (e.g. `6144`, `60k`, `6m`). Must be ≥ 6144. Default: `6144`. |
| `-h` | `--help` | Show help (also works as `bluraybackup-ex --help` for global help) |
| `-v` | `--version` | Show version and license information (global flag only) |

## Detailed options

### Source argument

All subcommands accept a `<source>` positional argument. The source can be:
- A physical Blu-ray device path (e.g. `/dev/sr0`, `D:`)
- A local ISO/BIN disc image file
- A directory containing a BDMV structure

### Key database (`-k`)

Specify the AACS keys database file. If not provided, the program will search these locations:

- Windows: `%APPDATA%\aacs\KEYDB.cfg`
- Linux/macOS: `~/.config/aacs/KEYDB.cfg`

It will also try filename variants like `keydb.cfg`, `KeyDB.cfg`, and `KEYDB.CFG`.

### Subcommands

#### `extract <source> <output-dir>`

Extracts the full disc file tree. The disc label is automatically appended:
`<output-dir>/<disc-label>/`.

#### `patch <source.iso> <output.iso>`

Writes a single decrypted ISO image. Phase 1 copies every sector of the source image verbatim; Phase 2 overwrites all encrypted `.m2ts` streams with AACS-decrypted content via libbluray. The source must be a regular ISO/BIN file.

#### `check <source>`

Reads every file on the disc to verify readability. On AACS-encrypted discs each 6144-byte unit MAC is validated and any failed blocks are reported with their file path and byte offset.

### I/O buffer size (`-b`)

Controls how many bytes are read from the source in a single operation.

- Default: `6144` bytes (one AACS encryption block, 3 × 2048-byte BD sectors). Safe for physical drives with scratched discs — smaller reads mean more precise error recovery.
- For ISO images or fast SSDs, a larger value significantly improves throughput:
  - `60k` — minor boost, still conservative
  - `6m` — recommended for ISO/SSD backups.
- Value must be at least `6144`.

Supported suffixes: `k`/`K` (×1024), `m`/`M` (×1024²), `g`/`G` (×1024³), or plain bytes.

## Examples

```bash
# Extract file tree from a physical drive
bluraybackup-ex extract /dev/sr0 ~/backup

# Extract file tree from an ISO image
bluraybackup-ex extract /path/to/disc.iso /output/dir

# Extract from a BDMV directory and specify a keys file
bluraybackup-ex extract /path/to/BDMV_DIR -k /path/to/KEYDB.cfg ~/output

# Produce a single decrypted ISO image (6 MB buffer)
bluraybackup-ex patch -b 6m encrypted.iso decrypted.iso

# Use a 1 MB buffer for faster extraction from ISO
bluraybackup-ex extract -b 1m /path/to/disc.iso /output/dir

# Use a conservative 64 KB buffer for a scratched physical disc
bluraybackup-ex extract -b 64k /dev/sr0 ~/backup

# Verify disc readability
bluraybackup-ex check /dev/sr0
bluraybackup-ex check disc.iso
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

### macOS

Required libraries and tools are available in [Homebrew](https://brew.sh).

```bash
brew install cmake libaacs libbluray ninja

# If on Apple Silicon, make links to libaacs in /usr/local,
# so libbluray can find them
cd /usr/local/lib
sudo ln -s /opt/homebrew/lib/libaacs.*dylib .

# Clone and build
git clone https://github.com/kingsznhone/bluraybackupEx
cd bluraybackupEx
cmake --preset macos-release
cmake --build --preset macos-release
# Output: build/bluraybackup-ex

# Optional: install system-wide
sudo cmake --install build
````

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

