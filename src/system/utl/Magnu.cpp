#include "utl/Magnu.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/DataFunc.h"
#include "os/HolmesClient.h"
#include "os/System.h"
#include "utl/Locale.h"

DataNode MagnuRefreshStrings(DataArray *arr) {
    DataArray *yeet = 0;
    if (UsingHolmes(2)) {
        if (SystemExec(MakeString("%s\\sysutl_r.exe -magnu_strings", FileSystemRoot()))
            == 0) {
            yeet = DataReadFile("magnu.dta", true);
        } else
            MILO_NOTIFY("Failure running sysutl");
    } else
        MILO_NOTIFY("Must be using Holmes to retrieve Magnu strings");

    if (yeet) {
        TheLocale.SetMagnuStrings(yeet);
        return yeet->Size();
    } else
        return 0;
}

DataNode AddToken(DataArray *arr) {
    MILO_NOTIFY("Magnu add token only supported on PC");
    return 1;
}

DataNode EditToken(DataArray *arr) {
    MILO_NOTIFY("Magnu add token only supported on PC");
    return 1;
}

void MagnuInit() {
    DataRegisterFunc("add_token", AddToken);
    DataRegisterFunc("edit_token", EditToken);
    DataRegisterFunc("magnu_refresh_strings", MagnuRefreshStrings);
}
