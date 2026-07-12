/* XDK-free verification TU: calls imports from BOTH generated libs.
   No XDK headers — declarations only. Proves the .libs resolve the
   ordinal imports into a valid PE import table. */
typedef int            BOOL;
typedef unsigned long  DWORD;
typedef void*          HANDLE;
typedef struct { unsigned short Length, MaximumLength; const char* Buffer; } ANSI_STRING;

/* --- xboxkrnl.exe imports --- */
extern void  RtlInitAnsiString(ANSI_STRING* dst, const char* src);   /* @300 */
extern void* XexGetProcedureAddress(HANDLE mod, DWORD ordinal, void** out); /* @407 */
extern DWORD DbgPrint(const char* fmt, ...);                          /* @3   */

/* --- xam.xex imports --- */
extern int   NetDll_XNetStartup(int caller, void* params);           /* @51  */
extern DWORD XamGetSystemVersion(void);                              /* @642 */

static ANSI_STRING g_str;

BOOL __stdcall DllMain(HANDLE h, DWORD reason, void* r) {
    (void)h; (void)r;
    if (reason == 1) {
        void* proc = 0;
        RtlInitAnsiString(&g_str, "rb3e-oss");          /* xboxkrnl */
        DbgPrint("ver=%08x\n", XamGetSystemVersion());  /* xam + xboxkrnl */
        XexGetProcedureAddress((HANDLE)0, 407, &proc);  /* xboxkrnl */
        NetDll_XNetStartup(0, 0);                       /* xam */
        return proc != 0 ? 1 : 1;
    }
    return 1;
}
