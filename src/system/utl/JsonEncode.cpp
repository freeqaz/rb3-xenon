#include "utl/JsonEncode.h"

void JSONStrEncode(const char *cc, String &str) {
    int length = strlen(cc);
    for (int i = 0; i < length; i++) {
        unsigned char cur = cc[i];
        if (cur == '"') {
            str += "\\\"";
        } else if (cur == '\\') {
            str += "\\\\";
        } else if (cur == '/') {
            str += "\\/";
        } else if (cur == '\b') {
            str += "\\b";
        } else if (cur == '\r') {
            str += "\\r";
        } else if (cur == '\n') {
            str += "\\n";
        } else if (cur == '\t') {
            str += "\\t";
        } else if (cur < ' ') {
            str += "\\u00";
            str += "0123456789ABCDEF"[cur >> 4];
            str += "0123456789ABCDEF"[cur & 0xF];
        } else {
            str += cur;
        }
    }
}
