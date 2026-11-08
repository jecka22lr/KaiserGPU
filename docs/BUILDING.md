# Building

Everything here is plain C built with **CeGCC** (`arm-mingw32ce`, GCC 9.3), on Linux.

## 1. Toolchain

```sh
git clone --recursive https://github.com/MaxKellermann/cegcc-build
cd cegcc-build
# binutils' gdb fails to build against modern Python; turn it off.
# In build.sh, add these to the binutils configure line:
#   --disable-gdb --disable-gdbserver --disable-sim --without-python
./build.sh --prefix=/opt/cegcc
export PATH=/opt/cegcc/bin:$PATH
```

It takes about 15 minutes.

## 2. Build

```sh
./build.sh          # builds everything in src/ into bin/
```

Rules that matter:

- **Always strip DLLs (`-s`).** Unstripped CeGCC DLLs carry DWARF debug sections, and Windows Mobile refuses them with error 193 (`ERROR_BAD_EXE_FORMAT`). The same DLL stripped loads fine. The machine type (0x1C0 vs 0x1C2) doesn't matter.
- Don't use `--subsystem windowsce`; the default is right.
- CE's coredll has no `lstrlenA`, `wsprintfA`, `GdiFlush` or `CreateHatchBrush`. Use `strlen` and your own formatting.
- `libgles_cm.dll` is built from `rotwrap4.c` + `rotwrap4.def`. The `.def` lists all 237 exports of HTC's original wrapper with their ordinals; 11 point at our functions, the rest are forwarders (`name=libgles_cm_ks.name`).

## 3. CAB

```sh
cd cab && ./build_cab.sh    # needs gcab
```

`_setup.xml` is a standard `wap-provisioningdoc` (FileOperation / Registry / Shortcut). File names inside the CAB use the `NAME0000.001` 8.3 form.

Notes:

- `%CE1%` is localised. On an Italian ROM it's `\Programmi`, not `\Program Files`. Don't hardcode paths in tools; use `GetModuleFileName`.
- A modified **signed** DLL gets rejected (`NTE_BAD_SIGNATURE`, 0x80090006). Strip the security directory and it loads as unsigned.
- Uninstalling a CAB deletes the files it installed, even if another CAB has overwritten them since. Remove old CABs before installing replacements.

## 4. ROM tools

`tools/` has the Python used to get the HTC-CA DLLs out of ROMs:

| Script | Does |
|---|---|
| `nbh.py` | Splits HTC signed NBH containers into sections |
| `imgfs.py` | Extracts imgfs (XPR and LZX compressed) |
| `lzx.py` | Standalone LZX decoder (16-bit window) |
| `xip.py` | Lists XIP ROMHDR modules and files |
| `e32.py` | Loads WinCE e32/o32 module directories, exports/imports |
| `armdis.py` | Capstone-based ARM disassembly helpers for PE files |
