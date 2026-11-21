// Entry point for the simppeliTCU host build: runs the firmware's setup()/loop() as a Linux program.
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>

#include "Arduino.h"
#include "host_config.h"

void setup();
void loop();

static void usage(const char* prog) {
    fprintf(stderr,
            "Usage: %s [options]\n"
            "  --port <n>         HTTP port for the web UI (default 8080)\n"
            "  --bind <addr>      Address to listen on (default 127.0.0.1, use 0.0.0.0 for LAN)\n"
            "  --prefs <file>     Settings file replacing NVS (default ./simppeliTCU_host_prefs.txt)\n"
            "  --can <iface>      Use a Linux SocketCAN interface, e.g. vcan0 or can0\n"
            "  --replay <file>    Replay received CAN frames from a `candump -l` log\n"
            "  --loop             Repeat the replay log forever\n"
            "\n"
            "The firmware's serial CLI reads from stdin, e.g. type: set mqtt_server localhost\n",
            prog);
}

int main(int argc, char** argv) {
    hostSetArgv(argc, argv);
    signal(SIGPIPE, SIG_IGN);
    randomSeed((unsigned long)time(nullptr) ^ (unsigned long)getpid());

    const char* bind = "127.0.0.1";
    int port = 8080;
    const char* canIface = nullptr;
    const char* replayFile = nullptr;
    bool replayLoop = false;

    for (int i = 1; i < argc; i++) {
        auto next = [&](const char* opt) -> const char* {
            if (i + 1 >= argc) {
                fprintf(stderr, "Missing value for %s\n", opt);
                exit(2);
            }
            return argv[++i];
        };
        if (!strcmp(argv[i], "--port")) port = atoi(next("--port"));
        else if (!strcmp(argv[i], "--bind")) bind = next("--bind");
        else if (!strcmp(argv[i], "--prefs")) hostPreferencesSetPath(next("--prefs"));
        else if (!strcmp(argv[i], "--can")) canIface = next("--can");
        else if (!strcmp(argv[i], "--replay")) replayFile = next("--replay");
        else if (!strcmp(argv[i], "--loop")) replayLoop = true;
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
        else { usage(argv[0]); return 2; }
    }

    if (canIface && replayFile) {
        fprintf(stderr, "Use either --can or --replay, not both\n");
        return 2;
    }
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid --port\n");
        return 2;
    }

    hostWebServerConfigure(bind, port);
    if (canIface) hostCanUseSocketCan(canIface);
    if (replayFile && !hostCanUseReplay(replayFile, replayLoop)) return 1;
    hostSeedPreferences();

    setup();
    for (;;) {
        loop();
        usleep(1000);  // ~1 kHz loop, keeps CPU usage low
    }
}
