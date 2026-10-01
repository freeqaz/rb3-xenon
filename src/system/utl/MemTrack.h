#pragma once

class AllocInfo;

void MemTrackReportMemoryAlloc(const char *);
void MemTrackReportMemoryUsage(const char *);
void MemTrackReportClose(const char *);

void *DebugHeapAlloc(int);
void DebugHeapFree(void *);

void BeginMemTrackFileName(const char *);
void EndMemTrackFileName();

void BeginMemTrackObjectName(const char *);
void EndMemTrackObjectName();

#ifdef HX_NATIVE
void MemTrackInit(int, int, bool);
#else
// Retail 0x827c4bb8 takes two arguments: it never reads r5, and its only
// caller (MemInit, 0x827bd300) never sets it.
void MemTrackInit(int, int);
#endif
const AllocInfo *MemTrackGetInfo(void *);
