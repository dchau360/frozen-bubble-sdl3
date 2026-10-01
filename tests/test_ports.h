// A free TCP port for a test's own fb-server (POSIX only, like every test
// that starts one). Fixed ports made `ctest -j` unsafe: two tests on the same
// number killed each other's servers. See tests/testports.py for the Python
// tests' twin.
#ifndef FB_TEST_PORTS_H
#define FB_TEST_PORTS_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

inline int FreeTcpPort() {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    int port = -1;
    socklen_t len = sizeof(addr);
    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0 &&
        getsockname(s, reinterpret_cast<sockaddr*>(&addr), &len) == 0)
        port = ntohs(addr.sin_port);
    close(s);
    return port;
}

#endif
