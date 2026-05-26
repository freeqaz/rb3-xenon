#pragma once

void WordWrap_SetOption(unsigned int);
bool WordWrap_CanBreakLineAt(const wchar_t *, const wchar_t *);

extern unsigned int g_uOption;
extern bool IsEastAsianChar(wchar_t);

struct LineBreakEntry {
    wchar_t ch;
    unsigned char cantBreakBefore;
    unsigned char cantBreakAfter;
};

extern LineBreakEntry g_LineBreakTable[146];
