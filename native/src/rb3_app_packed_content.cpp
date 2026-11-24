// Read a file directly from a user-supplied STFS container on macOS.
// The install index describes contiguous runs of data between STFS hash tables.
#include <sqlite3.h>
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <limits.h>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

bool NativeContentPath(const char *, char *, size_t);

namespace {
struct Run {
    int64_t end;
    int64_t physical;
};
struct Stream {
    int fd = -1;
    int64_t pos = 0;
    int64_t size = 0;
    std::vector<Run> runs;
    ~Stream() { if (fd >= 0) close(fd); }
};
int Read(void *cookie, char *dest, int count) {
    auto &s = *static_cast<Stream *>(cookie);
    int done = 0;
    auto it = std::upper_bound(s.runs.begin(), s.runs.end(), s.pos,
        [](int64_t pos, const Run &run) { return pos < run.end; });
    while (done < count && it != s.runs.end()) {
        const auto start = it == s.runs.begin() ? 0 : (it - 1)->end;
        int len = (int)std::min<int64_t>(count - done, it->end - s.pos);
        ssize_t got = pread(s.fd, dest + done, len, it->physical + s.pos - start);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) { if (!got) errno = EIO; return done ? done : -1; }
        s.pos += got;
        done += (int)got;
        if (s.pos == it->end) ++it;
    }
    return done;
}
fpos_t Seek(void *cookie, fpos_t amount, int from) {
    auto &s = *static_cast<Stream *>(cookie);
    int64_t base = from == SEEK_SET ? 0 : from == SEEK_CUR ? s.pos : s.size;
    if ((from != SEEK_SET && from != SEEK_CUR && from != SEEK_END) ||
        amount < -base || amount > INT64_MAX - base) { errno = EINVAL; return -1; }
    return s.pos = base + amount;
}
int Close(void *cookie) { delete static_cast<Stream *>(cookie); return 0; }
}

FILE *NativeOpenPackedContent(const char *logical) {
    const char *colon = strchr(logical, ':');
    if (!colon || colon - logical != 11 || colon[1] != '/') return nullptr;
    std::string device(logical, colon + 1);
    char mount[PATH_MAX], root[PATH_MAX];
    if (!NativeContentPath(device.c_str(), mount, sizeof(mount)) || !realpath(mount, root))
        return nullptr;
    std::string dir(root);
    sqlite3 *conn = nullptr;
    int rc = sqlite3_open_v2((dir + "/.rb3-index").c_str(), &conn, SQLITE_OPEN_READONLY, nullptr);
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(conn, sqlite3_close);
    if (rc != SQLITE_OK) return nullptr;
    sqlite3_stmt *stmt = nullptr;
    rc = sqlite3_prepare_v2(conn, "SELECT path,bytes,hash,size,spans FROM source,files WHERE name=?",
                            -1, &stmt, nullptr);
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> query(stmt, sqlite3_finalize);
    if (rc != SQLITE_OK || sqlite3_bind_text(stmt, 1, colon + 2, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_ROW) return nullptr;
    const char *path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    const auto sourceSize = sqlite3_column_int64(stmt, 1);
    auto s = std::make_unique<Stream>();
    if (!path || sqlite3_column_bytes(stmt, 2) != 20) return nullptr;
    s->fd = open((dir + "/" + path).c_str(), O_RDONLY | O_CLOEXEC);
    struct stat st;
    uint8_t hash[20];
    if (s->fd < 0 || fstat(s->fd, &st) || st.st_size != sourceSize ||
        pread(s->fd, hash, sizeof(hash), 0x381) != sizeof(hash) ||
        memcmp(hash, sqlite3_column_blob(stmt, 2), sizeof(hash))) return nullptr;
    const auto *spans = static_cast<const uint8_t *>(sqlite3_column_blob(stmt, 4));
    int bytes = sqlite3_column_bytes(stmt, 4);
    if (bytes % 12) return nullptr;
    for (int off = 0; off < bytes; off += 12) {
        uint64_t physical;
        uint32_t length;
        memcpy(&physical, spans + off, 8);
        memcpy(&length, spans + off + 8, 4);
        if (!length || length > sourceSize || physical > uint64_t(sourceSize - length))
            return nullptr;
        s->size += length;
        s->runs.push_back({s->size, (int64_t)physical});
    }
    if (s->size != sqlite3_column_int64(stmt, 3)) return nullptr;
    FILE *fp = funopen(s.get(), Read, nullptr, Seek, Close);
    if (fp) s.release();
    return fp;
}
