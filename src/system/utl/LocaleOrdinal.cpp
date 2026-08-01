#include "LocaleOrdinal.h"

#include "UTF8.h"
#include "os/System.h"

const char *LocalizeOrdinal(
    int num,
    LocaleGender gender,
    LocaleNumber number,
    bool superscriptMarkup,
    Symbol unusedLang,
    Locale &locale
) {
    char buf[255];
    strcpy(buf, LocalizeSeparatedInt(num));
    int len = strlen(buf);
    char code1, code2;
    if (len != 0)
        code1 = buf[len - 1];
    else
        code1 = '0';
    if (len > 1)
        code2 = buf[len - 2];
    else
        code2 = '0';

    static Symbol jpn("jpn");
    static Symbol eng("eng");
    static Symbol fre("fre");
    static Symbol deu("deu");
    static Symbol esl("esl");
    static Symbol ita("ita");

    Symbol lang = SystemLocale();
    if (lang != jpn) {
        if (lang == eng) {
            if (superscriptMarkup)
                strcat(buf, "<sup>");
            if (code1 == '1' && code2 != '1')
                strcat(buf, "st");
            else if (code1 == '2' && code2 != '1')
                strcat(buf, "nd");
            else if (code1 == '3' && code2 != '1')
                strcat(buf, "rd");
            else
                strcat(buf, "th");
            if (superscriptMarkup)
                strcat(buf, "</sup>");
        } else if (lang == fre) {
            if (superscriptMarkup)
                strcat(buf, "<sup>");
            if (strcmp(buf, "1") == 0) {
                if (gender == LocaleGenderMasculine) {
                    strcat(buf, "er");
                } else {
                    strcat(buf, "re");
                }
            } else {
                strcat(buf, "e");
            }
            if (superscriptMarkup)
                strcat(buf, "</sup>");
        } else if (lang == deu)
            strcat(buf, ".");
        else if (lang == esl || lang == ita) {
            String str;
            EncodeUTF8(str, 0xb0);
            if (gender == LocaleGenderMasculine) {
                if (number == 0)
                    strcat(buf, str.c_str());
                else {
                    strcat(buf, str.c_str());
                    strcat(buf, "s");
                }
            } else if (number == 0)
                strcat(buf, str.c_str());
            else {
                strcat(buf, str.c_str());
                strcat(buf, "s");
            }
        } else
            MILO_NOTIFY("Localizing Ordinal for unsupported language %s", lang);
    }
    return MakeString(buf);
}

#ifdef HX_NATIVE
// 4-arg overload: not yet decompiled in the retail xenon TU (its out-of-line
// body lives outside this object here), but DateTime.cpp references it, so the
// native rb3-dta build needs a real definition. Delegate to the 6-arg form with
// an empty lang (which makes it fall back to SystemLanguage — the exact behavior
// the 4-arg form must have) and TheLocale. Guarded so retail bytes are
// byte-identical (HX_NATIVE is native-only).
const char *LocalizeOrdinal(
    int num, LocaleGender gender, LocaleNumber number, bool superscriptMarkup
) {
    return LocalizeOrdinal(num, gender, number, superscriptMarkup, Symbol(), TheLocale);
}
#endif