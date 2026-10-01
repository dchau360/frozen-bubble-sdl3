"""A free TCP port for a test's own fb-server.

Each server test used to pick a fixed port, which made `ctest -j` unsafe:
two tests on the same number (15519 and 15521 were each used twice) killed
each other's servers. Asking the OS for an unused port instead lets every
test run alongside every other. There is a small window between closing the
probe socket and fb-server binding the port, which is fine for tests.
"""
import socket


def free_tcp_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]
