# Uninstall.exe DLL-planting regression (Windows only, run by CTest). F-1.
#
# Uninstall.exe imports Restart Manager (rstrtmgr.dll). RstrtMgr.dll statically
# imports ncrypt.dll, which delay-imports ntasn1.dll. When rstrtmgr.dll was a
# static import, that chain resolved during process initialization, before
# SetDefaultDllDirectories ran in wWinMain, and a copy of ncrypt.dll in the
# executable's own directory was loaded. The fix delay-loads rstrtmgr.dll and
# loads it explicitly from System32 at the first Restart Manager call.
#
# Method. For each DLL name separately (so one load cannot mask the next) and
# for each process role, a genuine copy of System32\<name> is placed in the
# directory of the Uninstall.exe under test. The copy is functional, so a
# process that loads it keeps running; which copy was loaded is read from the
# live process's module list (ProcessModule.FileName), not inferred from a
# crash or a log. The case FAILS when any module is loaded from the
# executable