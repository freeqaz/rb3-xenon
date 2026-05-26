#include "utl/Option.h"
#include "obj/DataFunc.h"
#include "os/System.h"
#include <vector>

std::vector<char *>::iterator FindOption(const char *option) {
    std::vector<char *>::iterator it;
    for (it = TheSystemArgs.begin(); it != TheSystemArgs.end(); it++) {
        if (**it == '-' && (streq(*it + 1, option)))
            break;
    }
    return it;
}

bool OptionBool(const char *option, bool def) {
    std::vector<char *>::iterator opt = FindOption(option);
    if (opt == TheSystemArgs.end())
        return def;
    else {
        TheSystemArgs.erase(opt);
        return !def;
    }
}

const char *OptionStr(const char *option, const char *def) {
    std::vector<char *>::iterator i = FindOption(option);
    if (i == TheSystemArgs.end())
        return def;
    else {
        std::vector<char *>::iterator erased = TheSystemArgs.erase(i);
        MILO_ASSERT(i != TheSystemArgs.end(), 0x5C);
        def = *erased;
        TheSystemArgs.erase(erased);
        return def;
    }
}

DataNode DataOptionStr(DataArray *arr) {
    const char *str = OptionStr(arr->Str(1), 0);
    if (str) {
        *arr->Var(2) = str;
        return 1;
    } else
        return 0;
}

DataNode DataOptionSym(DataArray *arr) {
    const char *str = OptionStr(arr->Str(1), 0);
    Symbol s(str);
    if (s.Null())
        return 0;
    else {
        *arr->Var(2) = s;
        return 1;
    }
}

DataNode DataOptionBool(DataArray *arr) { return OptionBool(arr->Str(1), false); }

void OptionInit() {
    DataRegisterFunc("option_bool", DataOptionBool);
    DataRegisterFunc("option_str", DataOptionStr);
    DataRegisterFunc("option_sym", DataOptionSym);
}
