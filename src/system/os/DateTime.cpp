#include "os/DateTime.h"
#include "os/Debug.h"
#include "os/System.h"
#include "utl/Locale.h"
#include "utl/LocaleOrdinal.h"
#include "xdk/XAPILIB.h"

void GetDateAndTime(DateTime &dt) {
    SYSTEMTIME time;
    GetLocalTime(&time);
    dt.mYear = time.wYear - 108;
    dt.mMonth = time.wMonth - 1;
    dt.mDay = time.wDay;
    dt.mHour = time.wHour;
    dt.mMin = time.wMinute;
    dt.mSec = time.wSecond;
}

void GetSystemDateAndTime(DateTime &dt) {
    SYSTEMTIME time;
    GetSystemTime(&time);
    dt.mYear = time.wYear - 108;
    dt.mMonth = time.wMonth - 1;
    dt.mDay = time.wDay;
    dt.mHour = time.wHour;
    dt.mMin = time.wMinute;
    dt.mSec = time.wSecond;
}

void GetTimeZoneBias(long &bias) {
    TIME_ZONE_INFORMATION info;
    bias = 0;
    if (GetTimeZoneInformation(&info) != -1) {
        bias = info.Bias;
    }
}

DateTime::DateTime(unsigned int code) {
    unsigned int a = code / 0x1FA4000;
    unsigned int b = code - a * 0x1FA4000;
    mMonth = b / 0x2A3000;
    mYear = a + 100;
    b = b - mMonth * 0x2A3000;
    mDay = b / 0x15180;
    b = b - mDay * 0x15180;
    mHour = b / 0xE10;
    b = b - mHour * 0xE10;
    mMin = b / 0x3C;
    b -= mMin * 0x3C;
    mSec = b;
}

DateTime::DateTime(
    unsigned short year,
    unsigned char month,
    unsigned char day,
    unsigned char hr,
    unsigned char min,
    unsigned char sec
) {
    mYear = year - 1900;
    mMonth = month - 1;
    mDay = day;
    mHour = hr;
    mMin = min;
    mSec = sec;
}

unsigned int DateTime::ToCode() const {
    return (mDay * 0x15180) + (mMonth * 0x2A3000) + (mYear - 100) * 0x1FA4000
        + (mHour * 0xE10) + (mMin * 0x3C) + mSec;
}

int DateTime::Year() const { return mYear + 1900; }
int DateTime::Month() const { return mMonth + 1; }

bool DateTime::IsLeapYear() const {
    int year = Year();
    return (!(year & 3) && year % 100) || !(year % 400);
}

int DateTime::DayOfYear() const {
    static const unsigned short days[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    int leap = (mMonth > 1 && IsLeapYear()) ? 1 : 0;
    return days[mMonth] + mDay + leap;
}

unsigned int DateTime::DiffSeconds(DateTime &dt) { return ToSeconds() - dt.ToSeconds(); }

void DateTime::FromFileTime(const FILETIME &ft) {
    SYSTEMTIME st;
    FileTimeToSystemTime(&ft, &st);
    mYear = st.wYear - 108;
    mMonth = st.wMonth - 1;
    mDay = st.wDay;
    mHour = st.wHour;
    mMin = st.wMinute;
    mSec = st.wSecond;
}

namespace {
    Symbol MonthToken(int month) {
        MILO_ASSERT((0) <= (month) && (month) <= (11), 0x1A1);
        static Symbol month_symbols[12] = {
            "month_january",   "month_february", "month_march",    "month_april",
            "month_may",       "month_june",     "month_july",     "month_august",
            "month_september", "month_october",  "month_november", "month_december"
        };
        return month_symbols[month];
    }
}

void DateTime::Format(class String &str) const {
    char buf[256];

    if (SearchReplace(str.c_str(), "%d", MakeString("%02d", mDay), buf)) {
        str = buf;
    }
    static Symbol fre("fre");
    static Symbol ita("ita");
    static Symbol esl("esl");
    Symbol lang = SystemLanguage();
    if (lang == fre || lang == ita || lang == esl) {
        if (SearchReplace(str.c_str(), "%e", MakeString("%d", mDay), buf)) {
            str = buf;
        }
    } else {
        if (SearchReplace(
                str.c_str(),
                "%e",
                LocalizeOrdinal(mDay, LocaleGenderMasculine, LocaleSingular, false, Symbol(gNullStr), TheLocale),
                buf
            )) {
            str = buf;
        }
    }

    if (SearchReplace(str.c_str(), "%m", MakeString("%02d", mMonth + 1), buf)) {
        str = buf;
    }
    if (strstr(str.c_str(), "%M")) {
        if (SearchReplace(
                str.c_str(), "%M", Localize(MonthToken(mMonth), nullptr, TheLocale), buf
            )) {
            str = buf;
        }
    }

    if (SearchReplace(str.c_str(), "%Y", MakeString("%04d", mYear + 1900), buf)) {
        str = buf;
    }
    if (SearchReplace(str.c_str(), "%H", MakeString("%02d", mHour), buf)) {
        str = buf;
    }
    if (SearchReplace(str.c_str(), "%i", MakeString("%02d", mMin), buf)) {
        str = buf;
    }
    if (SearchReplace(str.c_str(), "%s", MakeString("%02d", mSec), buf)) {
        str = buf;
    }
}

DateFormat DateTime::GetDateFormatting() {
    static Symbol aus("aus");
    static Symbol aut("aut");
    static Symbol bel("bel");
    static Symbol bra("bra");
    static Symbol can("can");
    static Symbol chi("chi");
    static Symbol chn("chn");
    static Symbol col("col");
    static Symbol cze("cze");
    static Symbol den("den");
    static Symbol esp("esp");
    static Symbol fin("fin");
    static Symbol fra("fra");
    static Symbol gbr("gbr");
    static Symbol ger("ger");
    static Symbol gre("gre");
    static Symbol hkg("hkg");
    static Symbol hun("hun");
    static Symbol ind("ind");
    static Symbol irl("irl");
    static Symbol ita("ita");
    static Symbol jpn("jpn");
    static Symbol kor("kor");
    static Symbol mex("mex");
    static Symbol ned("ned");
    static Symbol nor("nor");
    static Symbol nzl("nzl");
    static Symbol pol("pol");
    static Symbol por("por");
    static Symbol rsa("rsa");
    static Symbol rus("rus");
    static Symbol sin("sin");
    static Symbol svk("svk");
    static Symbol swe("swe");
    static Symbol sui("sui");
    static Symbol tpe("tpe");
    static Symbol usa("usa");
    Symbol locale = SystemLocale();
    Symbol lang = SystemLanguage();
    if (locale == usa || locale == can)
        return kMDY;
    else if (locale == swe)
        return kYMD;
    else if (locale == chn || locale == hkg || locale == jpn || locale == kor
             || locale == sin || locale == tpe)
        return kISO;
    else
        return kDMY;
}

BinStream &operator<<(BinStream &bs, const DateTime &dt) {
    bs << dt.mSec << dt.mMin << dt.mHour << dt.mDay << dt.mMonth << dt.mYear;
    return bs;
}

BinStream &operator>>(BinStream &bs, DateTime &dt) {
    bs >> dt.mSec >> dt.mMin >> dt.mHour >> dt.mDay >> dt.mMonth >> dt.mYear;
    return bs;
}

void DateTime::ToDateString(String &str) const {
    switch (GetDateFormatting()) {
    case kMDY:
        str += MakeString("%02d/%02d/%02d", Month(), mDay, Year() % 100);
        break;
    case kDMY:
        str += MakeString("%02d/%02d/%02d", mDay, Month(), Year() % 100);
        break;
    case kYMD:
        str += MakeString("%02d/%02d/%02d", Year() % 100, Month(), mDay);
        break;
    case kISO:
        str += MakeString("%04d/%02d/%02d", Year(), Month(), mDay);
        break;
    default:
        break;
    }
}

void DateTime::ToString(String &str) const {
    ToDateString(str);
    str += MakeString(" %02d:%02d:%02d", mHour, mMin, mSec);
}

int DateTime::DayOfWeek() const {
    int y = mYear + 1899;
    int days = y * 365 + y / 4 - y / 100 + y / 400;
    return (DayOfYear() + days % 7) % 7;
}

int DateTime::ToDayNumber() {
    int m = (mMonth + 10) % 12;
    int y = mYear - m / 10;
    return (m * 306 + 5) / 10 + mDay + y * 365 + y / 4 - y / 100 + y / 400 - 1;
}

void DateTime::FromDayNumber(int dayNumber) {
    int y = (dayNumber * 10000 + 14780) / 3652425;
    int doy = dayNumber - (y * 365 + y / 4 - y / 100 + y / 400);
    if (doy < 0) {
        y--;
        doy = dayNumber - (y * 365 + y / 4 - y / 100 + y / 400);
    }
    int mi = (doy * 100 + 52) / 3060;
    int month = mi + 2;
    mMonth = month % 12;
    mYear = y + month / 12;
    mDay = doy - (mi * 306 + 5) / 10 + 1;
}

unsigned int DateTime::ToSeconds() {
    return ((ToDayNumber() * 24 + mHour) * 60 + mMin) * 60 + mSec;
}

void DateTime::FromUtcToLocal() {
    long bias;
    GetTimeZoneBias(bias);
    unsigned int secs = ToSeconds() - bias * 60;
    int days = secs / 86400;
    secs -= days * 86400;
    mHour = secs / 3600;
    secs -= mHour * 3600;
    mMin = secs / 60;
    mSec = secs - mMin * 60;
    FromDayNumber(days);
}

void DateTime::ParseDate(const char *str) {
    mSec = 0;
    mMin = 0;
    mHour = 0;
    int len = strlen(str);
    char yearBuf[5];
    char dayBuf[3];
    char hourBuf[3];
    char secBuf[3];
    char monthBuf[3];
    char minBuf[3];
    strncpy(yearBuf, str, 4);
    yearBuf[4] = '\0';
    mYear = atoi(yearBuf) - 1900;
    strncpy(monthBuf, str + 5, 2);
    monthBuf[2] = '\0';
    mMonth = atoi(monthBuf) - 1;
    if (mMonth > 11) {
        MILO_NOTIFY("month %i out of range in %s", mMonth, str);
        mMonth = 0;
    }
    strncpy(dayBuf, str + 8, 2);
    dayBuf[2] = '\0';
    mDay = atoi(dayBuf);
    if (mDay < 1 || mDay > 31) {
        MILO_NOTIFY("day %i out of range in %s", mDay, str);
        mDay = 1;
    }
    if (len == 10) {
        TheDebug << MakeString("Old format YYYY-MM-DD.\n");
    } else {
        strncpy(hourBuf, str + 11, 2);
        hourBuf[2] = '\0';
        mHour = atoi(hourBuf);
        strncpy(minBuf, str + 14, 2);
        minBuf[2] = '\0';
        mMin = atoi(minBuf);
        strncpy(secBuf, str + 17, 2);
        secBuf[2] = '\0';
        mSec = atoi(secBuf);
    }
}

int DateTimeCmp(const DateTime &a, const DateTime &b) {
    unsigned int dateA = (a.mYear * 256 + a.mMonth) * 256 + a.mDay;
    unsigned int dateB = (b.mYear * 256 + b.mMonth) * 256 + b.mDay;
    unsigned int timeA = (a.mHour * 256 + a.mMin) * 256 + a.mSec;
    unsigned int timeB = (b.mHour * 256 + b.mMin) * 256 + b.mSec;
    if (dateA < dateB) return -1;
    if (dateA == dateB) {
        if (timeA < timeB) return -1;
        if (timeA == timeB) return 0;
    }
    return 1;
}

void DateTimeInit() {}
