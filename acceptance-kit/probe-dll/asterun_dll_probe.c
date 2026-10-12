/*
 * Asterun acceptance kit - harmless DLL load probe. NOT part of Asterun.
 *
 * When any process loads this DLL, DllMain appends one line to
 *   %PUBLIC%\AsterunDllProbe\probe-log.txt
 * (UTF-16LE with a byte order mark) with the process id, the loading
 * process's image path and this DLL's own path, then returns TRUE. It exports nothing, starts no thread, changes no
 * other file, registry value or process state, and does not use the C
 * runtime. It is unsigned: antivirus may quarantine it. Use it only in a
 * disposable test VM or Windows Sandbox, never in System32 or on a daily-use
 * machine. A copy renamed to a DLL name a program searches for in its own
 * directory shows, by a log line, that the program loaded it from there.
 *
 * Build (either):
 *   MSVC:  cl /nologo /O1 /GS- /LD asterun_dll_probe.c /link /NODEFAULTLIB /ENTRY:DllMain kernel32.lib /OUT:asterun_dll_probe.dll
 *   LLVM:  see build-llvm.sh (clang-cl + lld-link, no Windows SDK needed)
 */

typedef int BOOL;
typedef unsigned long DWORD;
typedef void* HANDLE;
typedef void* HINSTANCE;
typedef void* LPVOID;
typedef unsigned short WCHAR;

#define DLL_PROCESS_ATTACH 1
#define GENERIC_WRITE 0x40000000UL
#define FILE_SHARE_READ 0x1UL
#define FILE_SHARE_WRITE 0x2UL
#define OPEN_ALWAYS 4UL
#define FILE_ATTRIBUTE_NORMAL 0x80UL
#define FILE_END 2UL
#define INVALID_HANDLE_VALUE ((HANDLE)(~(unsigned long long)0))

__declspec(dllimport) DWORD __stdcall GetEnvironmentVariableW(const WCHAR*, WCHAR*, DWORD);
__declspec(dllimport) BOOL __stdcall CreateDirectoryW(const WCHAR*, void*);
__declspec(dllimport) HANDLE __stdcall CreateFileW(const WCHAR*, DWORD, DWORD, void*, DWORD, DWORD, HANDLE);
__declspec(dllimport) DWORD __stdcall SetFilePointer(HANDLE, long, long*, DWORD);
__declspec(dllimport) BOOL __stdcall WriteFile(HANDLE, const void*, DWORD, DWORD*, void*);
__declspec(dllimport) BOOL __stdcall CloseHandle(HANDLE);
__declspec(dllimport) DWORD __stdcall GetModuleFileNameW(HINSTANCE, WCHAR*, DWORD);
__declspec(dllimport) DWORD __stdcall GetCurrentProcessId(void);

static DWORD Append(WCHAR* buffer, DWORD at, DWORD capacity, const WCHAR* text) {
    while (*text && at + 1 < capacity) buffer[at++] = *text++;
    buffer[at] = 0;
    return at;
}

static DWORD AppendNumber(WCHAR* buffer, DWORD at, DWORD capacity, DWORD value) {
    WCHAR digits[16];
    int count = 0;
    do { digits[count++] = (WCHAR)(L'0' + value % 10); value /= 10; } while (value && count < 15);
    while (count && at + 1 < capacity) buffer[at++] = digits[--count];
    buffer[at] = 0;
    return at;
}

BOOL __stdcall DllMain(HINSTANCE self, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason != DLL_PROCESS_ATTACH) return 1;

    /* Static buffers: DLL_PROCESS_ATTACH runs once per process under the
       loader lock, and large stack frames would need the CRT's __chkstk. */
    static WCHAR directory[600];
    DWORD length = GetEnvironmentVariableW(L"PUBLIC", directory, 520);
    if (length == 0 || length >= 520) return 1;
    length = Append(directory, length, 600, L"\\AsterunDllProbe");
    CreateDirectoryW(directory, 0);

    static WCHAR file[640];
    DWORD fileLength = Append(file, 0, 640, directory);
    Append(file, fileLength, 640, L"\\probe-log.txt");

    static WCHAR line[1200];
    static WCHAR path[520];
    DWORD at = 0;
    at = Append(line, at, 1200, L"pid=");
    at = AppendNumber(line, at, 1200, GetCurrentProcessId());
    at = Append(line, at, 1200, L"\tprocess=");
    path[0] = 0;
    GetModuleFileNameW(0, path, 520);
    path[519] = 0;
    at = Append(line, at, 1200, path);
    at = Append(line, at, 1200, L"\tdll=");
    path[0] = 0;
    GetModuleFileNameW(self, path, 520);
    path[519] = 0;
    at = Append(line, at, 1200, path);
    at = Append(line, at, 1200, L"\r\n");

    HANDLE handle = CreateFileW(file, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
                                OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (handle != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        /* A new (empty) log starts with a UTF-16LE byte order mark so that
           Windows PowerShell and Notepad read it as Unicode. */
        if (SetFilePointer(handle, 0, 0, FILE_END) == 0) {
            static const WCHAR bom = 0xFEFF;
            WriteFile(handle, &bom, (DWORD)sizeof(bom), &written, 0);
        }
        WriteFile(handle, line, at * (DWORD)sizeof(WCHAR), &written, 0);
        CloseHandle(handle);
    }
    return 1;
}
