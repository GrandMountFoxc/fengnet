#ifndef FENGNET_DAEMON_H
#define FENGNET_DAEMON_H

class FengnetDaemon{
public:
    FengnetDaemon() = default;
    int daemon_init(const char* pidfile);
    int daemon_exit(const char* pidfile);
    int get();
private:
    int check_pid(const char *pidfile);
    int write_pid(const char *pidfile);
    int redirect_fds();
};

#endif