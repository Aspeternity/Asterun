#!/bin/sh
# Builds asterun_dll_probe.dll for x64 and ARM64 with LLVM only (no Windows SDK):
# clang-cl compiles, llvm-dlltool makes a KERNEL32 import library from
# kernel32.def, lld-link links without any C runtime.
set -eu
cd "$(dirname "$0")"
for arch in x64 arm64; do
  case $arch in
    x64) target=x86_64-pc-windows-msvc; machine=x64; dlltool=i386:x86-64 ;;
    arm64) target=aarch64-pc-windows-msvc; machine=arm64; dlltool=arm64 ;;
  esac
  mkdir -p out/$arch
  clang-cl --target=$target /nologo /O1 /GS- /c asterun_dll_probe.c /Foout/$arch/asterun_dll_probe.obj
  llvm-dlltool -m $dlltool -d kernel32.def -l out/$arch/kernel32.lib
  lld-link /nologo /DLL /NODEFAULTLIB /ENTRY:DllMain /MACHINE:$machine \
    out/$arch/asterun_dll_probe.obj out/$arch/kernel32.lib /OUT:out/$arch/asterun_dll_probe.dll
done
sha256sum out/*/asterun_dll_probe.dll
