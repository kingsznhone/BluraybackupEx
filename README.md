# BlueraybackupEx

Blu-ray Disc backup tool that uses libbluray to extract content from Blu-ray discs or disc image files.

## Usage

```
bluraybackup-ex {-d device | -i input} [-k keyfile] [-m dir|iso] [-b size] [-c] [output]
```

## Command-line options

| Option | Long form | Description |
|------:|--------|------|
| `-d PATH` | `--device=PATH` | Physical Blu-ray device path. Example (Linux): `/dev/sr0`; (Windows): `D:` |
| `-i PATH` | `--input=PATH` | Local disc image file (ISO/BIN) or a directory containing a BDMV structure |
| `-k FILE` | `--keydb=FILE` | Path to the AACS keys database file |
| `-m MODE` | `--mode=MODE` | Output mode: `dir` (default) or `iso` |
| `-b SIZE` | `--buffer=SIZE` | I/O read buffer size (e.g. `6144`, `64k`, `6m`). Must be ≥ 6144. Default: `6144`. |
| `-c` | `--check` | Verify every file on the disc before (or instead of) extracting |
| `-h` | `--help` | Show help information |
| `-v` | `--version` | Show version and license information |
| `output` | | Destination path (directory for `dir` mode, file for `iso` mode) |

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

### Output mode (`-m`)

| Mode | Output | Notes |
|------|--------|-------|
| `dir` | `<output>/<disc-label>/` directory tree | Default when `-m` is omitted |
| `iso` | Single decrypted ISO file at `<output>` | Phase 1: raw sector copy; Phase 2: .m2ts streams replaced with AACS-decrypted content. Requires `-i <file.iso>`. |

If `output` is not provided at all, only disc information is printed and the program exits successfully.

### Disc integrity check (`-c`)

Reads every file on the disc without writing any output. On AACS-encrypted discs each 6144-byte unit MAC is validated and any failed blocks are reported with their file path and byte offset. Can be combined with an `output` path to verify before extracting.

### I/O buffer size (`-b`)

Controls how many bytes are read from the source in a single operation.

- Default: `6144` bytes (one AACS encryption block, 3 × 2048-byte BD sectors). Safe for physical drives with scratched discs — smaller reads mean more precise error recovery.
- For ISO images or fast SSDs, a larger value significantly improves throughput:
  - `64k` — minor boost, still conservative
  - `6m` — recommended for ISO/SSD backups.
- Value must be at least `6144`.

Supported suffixes: `k`/`K` (×1024), `m`/`M` (×1024²), `g`/`G` (×1024³), or plain bytes.

## Examples

```bash
# Print disc information only (no output path given)
bluraybackup-ex -d /dev/sr0
bluraybackup-ex -i /path/to/disc.iso

# Extract file tree from a physical drive
bluraybackup-ex -d /dev/sr0 ~/backup

# Extract file tree from an ISO image
bluraybackup-ex -i /path/to/disc.iso /output/dir

# Extract from a BDMV directory and specify a keys file
bluraybackup-ex -i /path/to/BDMV_DIR -k /path/to/KEYDB.cfg ~/output

# Produce a single decrypted ISO image (6 MB buffer)
bluraybackup-ex -i encrypted.iso -m iso -b 6m decrypted.iso

# Use a 1 MB buffer for faster dir-mode extraction from ISO
bluraybackup-ex -i /path/to/disc.iso -b 1m /output/dir

# Use a conservative 64 KB buffer for a scratched physical disc
bluraybackup-ex -d /dev/sr0 -b 64k ~/backup

# Verify disc before extracting
bluraybackup-ex -i disc.iso -c ~/backup

# Verify only (no output path)
bluraybackup-ex -i disc.iso -c
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

