// rb3-xenon native -- A9: a proxy RndDir's Enter chains its owning directory
// as a message source through that directory's MsgSource subobject.
//
// THE DEFECT. Native RndDir::Enter (rndobj/Dir.cpp, the HX_NATIVE arm) called
// ChainSourceSubdir(Dir(), this): Dir() converts to the owning dir's
// Hmx::Object base, and ChainSourceSubdir forwards its first argument to
// MsgSource::ChainSource as a MsgSource * with a bare reinterpret_cast. In a
// native RndDir the MsgSource base is not at the Hmx::Object address, so
// AddSink walked "mSinks" of the wrong subobject. rb3-app's App::Run died on
// frame 3 (intro_movie_screen -> splash_screen, BackdropPanel::Enter) in
// MsgSource::RemoveSink <- AddSink <- ChainSource, reading x86 prologue bytes
// (a vptr -> vtable -> code walked as list nodes).
//
// THE REFERENCE is retail RndDir::Enter, 0x82404EA8 (the X360 arm of the same
// function): `dynamic_cast<MsgSource *>(Dir())`, and only when non-NULL,
// ChainSourceSubdir(that, this). ChainSourceSubdir (0x824037C8) then calls
// ChainSource with `this` = the proxy's MsgSource: if the proxy has sinks, the
// owning dir gains the proxy as a sink. So after Enter the owning dir's own
// mSinks must hold exactly one entry, naming the proxy.
//
// The case runs in a forked child: without the fix it corrupts the heap or
// faults, and a crash must read as a FAIL, not end rb3-milo before its RESULT
// line. The control calls ChainSourceSubdir directly with the retail cast, in
// process; a failing control means the model, not Enter, is wrong.

#include "obj/Dir.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "rndobj/Dir.h"

#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

typedef void (*GateFn)(const char *, bool, const char *);

namespace {

    GateFn gGate = nullptr;

    void Gate(const char *name, bool ok, const char *fmt, ...)
        __attribute__((format(printf, 3, 4)));
    void Gate(const char *name, bool ok, const char *fmt, ...) {
        char buf[512];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        gGate(name, ok, buf);
    }

    struct Scene {
        RndDir *owner;
        RndDir *proxy;
        Hmx::Object *listener;
    };

    // An owning RndDir and a proxy RndDir named in it (proxy->Dir() == owner,
    // so IsProxy()), the proxy carrying one global sink so ChainSource takes
    // its mSinks branch.
    Scene MakeScene(const char *tag) {
        Scene s;
        s.owner = Hmx::Object::New<RndDir>();
        s.proxy = Hmx::Object::New<RndDir>();
        std::string name = std::string("a9_proxy_") + tag;
        s.proxy->SetName(name.c_str(), s.owner);
        s.listener = Hmx::Object::New<Hmx::Object>();
        s.proxy->AddSink(s.listener);
        return s;
    }

    // Retail's postcondition: the owner's own sink list holds the proxy once.
    bool OwnerChainsProxy(const Scene &s, std::string &detail) {
        MsgSource *ownerSrc = dynamic_cast<MsgSource *>(s.owner);
        Hmx::Object *proxyObj = s.proxy;
        int entries = 0, toProxy = 0;
        for (std::list<MsgSource::Sink>::const_iterator it = ownerSrc->mSinks.begin();
             it != ownerSrc->mSinks.end() && entries < 64;
             ++it, ++entries) {
            if (it->obj == proxyObj)
                toProxy++;
        }
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "proxy->IsProxy()=%d; owner's mSinks: %d entr%s, %d naming the proxy "
                 "(retail 0x82404EA8: 1 entry, the proxy)",
                 s.proxy->IsProxy(), entries, entries == 1 ? "y" : "ies", toProxy);
        detail = buf;
        return s.proxy->IsProxy() && entries == 1 && toProxy == 1;
    }

    void Teardown(Scene &s) {
        delete s.proxy;
        delete s.owner;
        delete s.listener;
    }

    bool RunInChild(bool (*fn)(std::string &), std::string &detail) {
        fflush(stdout);
        fflush(stderr);
        int fds[2];
        if (pipe(fds) != 0) {
            detail = "pipe failed";
            return false;
        }
        pid_t pid = fork();
        if (pid < 0) {
            detail = "fork failed";
            return false;
        }
        if (pid == 0) {
            close(fds[0]);
            alarm(20);
            std::string d;
            bool ok = fn(d);
            (void)!write(fds[1], d.data(), d.size());
            close(fds[1]);
            fflush(stdout);
            fflush(stderr);
            _exit(ok ? 0 : 1);
        }
        close(fds[1]);
        char buf[512];
        ssize_t n;
        while ((n = read(fds[0], buf, sizeof(buf))) > 0)
            detail.append(buf, n);
        close(fds[0]);
        int status = 0;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            if (detail.empty())
                detail = "no detail";
            return WEXITSTATUS(status) == 0;
        }
        char s[128];
        if (WIFSIGNALED(status))
            snprintf(s, sizeof(s), "child killed by signal %d (%s)%s", WTERMSIG(status),
                     strsignal(WTERMSIG(status)),
                     WTERMSIG(status) == SIGALRM ? ", deadline" : "");
        else
            snprintf(s, sizeof(s), "child ended abnormally (status 0x%x)", status);
        detail = s + (detail.empty() ? std::string() : "; " + detail);
        return false;
    }

    bool EnterCase(std::string &detail) {
        Scene s = MakeScene("enter");
        s.proxy->Enter();
        bool ok = OwnerChainsProxy(s, detail);
        Teardown(s);
        return ok;
    }
}

int RunProxyChainPhase(GateFn gate) {
    gGate = gate;
    printf("\n=== A9: a proxy RndDir's Enter chains its owner through MsgSource ===\n");

    // Control: the retail call shape, spelled directly.
    {
        Scene s = MakeScene("control");
        MsgSource *src = dynamic_cast<MsgSource *>(s.proxy->Dir());
        if (src)
            s.proxy->ChainSourceSubdir(reinterpret_cast<Hmx::Object *>(src), s.proxy);
        std::string detail;
        bool ok = src && OwnerChainsProxy(s, detail);
        Gate("proxy-chain-control", ok, "ChainSourceSubdir(MsgSource cast of Dir()): %s",
             detail.c_str());
        Teardown(s);
    }

    std::string detail;
    bool ok = RunInChild(EnterCase, detail);
    Gate("proxy-enter-chains-owner-msgsource", ok, "proxy RndDir::Enter: %s", detail.c_str());
    return 0;
}
