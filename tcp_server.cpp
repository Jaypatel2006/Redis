#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <assert.h>

static void do_something(int connfd)
{
    char rbuf[64] = {};
    ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1);
    if (n < 0)
    {
        std::cout << "Error while reading ... " << "\n";
        return;
    }
    std::cout << "Client Says  : " << rbuf << "\n";
    char wbuf[] = "Welcome";
    write(connfd, wbuf, sizeof(wbuf));
}

static int32_t read_full(int fd, char *buf, size_t n)
{
    while (n > 0)
    {
        int rv = read(fd, buf, n);
        if (rv <= 0)
        {
            return -1;
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}


static int32_t write_all(int fd, const char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0) {
            return -1;  // error
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rv)
    {
        std::cout << "Error While Binding" << "\n";
    }
    else
    {
        std::cout << "Bind Successfully.." << "\n";
    }

    rv = listen(fd, SOMAXCONN);
    if (rv)
    {
        std::cout << "Error while listening.." << "\n";
    }
    else
    {
        std::cout << "Listening on port : " << ntohs(addr.sin_port) << "\n";
    }

    while (true)
    {
        struct sockaddr_in client_addr = {};
        socklen_t addrlen = sizeof(client_addr);
        int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
        if (connfd < 0)
        {
            continue;
        }

        while (true)
        {
            int32_t err = one_req(connfd);
            if (err)
            {
                break;
            }
        }
        close(connfd);
    }

    return 0;
}