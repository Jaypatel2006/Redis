// stdlib
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
// system
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
// C++
#include <vector>
#include<iostream>
#include <map>


const size_t k_max_msg = 4096;

struct Conn{
    int fd = -1;
    bool want_read = false;
    bool want_write = false;
    bool want_close = false;
    std::vector<uint8_t> incoming;
    std::vector<uint8_t> outgoing;

};

struct Response{
    uint32_t status = 0;
    std::vector<uint8_t> data;

};

enum {
    RES_OK = 0,
    RES_ERR = 1,    // error
    RES_NX = 2,     // key not found
};


static std::map<std::string, std::string> g_data;

static void
buf_append(std::vector<uint8_t> &buf, const uint8_t *data, size_t len) {
    buf.insert(buf.end(), data, data + len);
}
// remove from the front
static void buf_consume(std::vector<uint8_t> &buf, size_t n) {
    buf.erase(buf.begin(), buf.begin() + n);
}

static void fd_set_nb(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags < 0) {
        perror("fcntl F_GETFL");
        exit(1);
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl F_SETFL");
        exit(1);
    }
}

static Conn *handle_accept(int fd) {
    // accept
    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
    if (connfd < 0) {
        return NULL;
    }
    // set the new connection fd to nonblocking mode
    fd_set_nb(connfd);
    // create a `struct Conn`
    Conn *conn = new Conn();
    conn->fd = connfd;
    conn->want_read = true; // read the 1st request
    return conn;
}

bool read_32(uint8_t *&curr,const uint8_t* end,uint32_t& out){
    if(curr + 4 > end){
        return false;
    }
    memcpy(&out,curr,4);
    curr += 4;
    return true;
}


bool read_str(uint8_t *&curr,const uint8_t* end,uint32_t n,std::string &out){
    if(curr+n > end){
        return false;
    }
    out.assign(curr,curr+n);
    curr+=n;
    return true;

} 



static void do_request(std::vector<std::string> &cmd, Response &out) {
    if (cmd.size() == 2 && cmd[0] == "get") {
        auto it = g_data.find(cmd[1]);
        if (it == g_data.end()) {
            out.status = RES_NX;    // not found
            return;
        }
        const std::string &val = it->second;
        out.data.assign(val.begin(), val.end());
    } else if (cmd.size() == 3 && cmd[0] == "set") {
        g_data[cmd[1]].swap(cmd[2]);
    } else if (cmd.size() == 2 && cmd[0] == "del") {
        g_data.erase(cmd[1]);
    } else {
        out.status = RES_ERR;       // unrecognized command
    }
}

static void make_response(const Response &resp, std::vector<uint8_t> &out) {
    uint32_t resp_len = 4 + (uint32_t)resp.data.size();
    buf_append(out, (const uint8_t *)&resp_len, 4);
    buf_append(out, (const uint8_t *)&resp.status, 4);
    buf_append(out, resp.data.data(), resp.data.size());
}

static int32_t parse_req(uint8_t *data,size_t size,std::vector<std::string> &out){
    const uint8_t* end = data + size;
    uint32_t nstr = 0;
    if(!read_32(data,end,nstr)){
        return -1;
    }
    if(nstr > k_max_msg){
        return -1;
    }
    while(out.size() < nstr){
        uint32_t len = 0;
        if(!read_32(data,end,len)){
            return -1;
        }
        out.push_back(std::string());
        if(!read_str(data,end,len,out.back())){
            return -1;
        }
    }
    if(data!=end){
        return -1;
    }
    return 0;

}

static bool try_one_req(Conn* conn){

    if (conn->incoming.size() < 4) {
        return false;   // want read
    }
    uint32_t len = 0;
    memcpy(&len, conn->incoming.data(), 4);
    if (len > k_max_msg) {
        conn->want_close = true;
        return false;   // want close
    }
    // message body
    if (4 + len > conn->incoming.size()) {
        return false;   // want read
    }
    uint8_t *request = &conn->incoming[4];
    

    // 4. Process the parsed message.
    // ...
    // generate the response (echo)
    std::vector<std::string> cmd;
    if(parse_req(request,len,cmd) < 0){
        conn->want_close = true;
        return false;
    }
    Response resp;
    do_request(cmd,resp);
    make_response(resp,conn->outgoing);



    std::cout << "Client says: "
          << std::string((const char*)request, len)
          << "\n";


    buf_append(conn->outgoing, (const uint8_t *)&len, 4);
    buf_append(conn->outgoing, request, len);
    // 5. Remove the message from `Conn::incoming`.
    buf_consume(conn->incoming, 4 + len);
    return true;  


}

static void handle_read(Conn *conn){
    uint8_t buf[64*1024];
    ssize_t rv = read(conn->fd,buf,sizeof(buf));
    
    if (rv < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK)
        return;

        conn->want_close = true;
        return;
    }

    if (rv == 0) {
        conn->want_close = true;
        return;
    }

    buf_append(conn->incoming,buf,(size_t)rv);

    while(try_one_req(conn)){

    }

    if (conn->outgoing.size() > 0) {    // has a response
        conn->want_read = false;
        conn->want_write = true;
    }   // else: want read

}

static void handle_write(Conn *conn) {
    assert(conn->outgoing.size() > 0);
    ssize_t rv = write(conn->fd, conn->outgoing.data(), conn->outgoing.size());

    if (rv < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;

        conn->want_close = true;
        return;
    }

    // remove written data from `outgoing`
    buf_consume(conn->outgoing, (size_t)rv);
    // ...

    if (conn->outgoing.size() == 0) {   // all data written
        conn->want_read = true;
        conn->want_write = false;
    } // else: want write
}

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0)
    {
        perror("socket");
        return 1;
    }

    int val = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                   &val, sizeof(val)) < 0)
    {
        perror("setsockopt");
        return 1;
    }

    sockaddr_in addr{};

    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (const sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        close(fd);
        return 1;
    }

    std::cout << "Bind successful\n";

    if (listen(fd, SOMAXCONN) < 0)
    {
        perror("listen");
        close(fd);
        return 1;
    }

    std::cout << "Listening on port 1234...\n";


    
        

        std::vector<Conn*> fd2conn;
        std::vector<struct pollfd> poll_args;
        while(true){
            poll_args.clear();
            struct pollfd pfd  = {fd,POLLIN,0};
            poll_args.push_back(pfd);

            for(Conn* conn : fd2conn){
                if(!conn){
                    continue;
                }

                struct pollfd pfd = {conn->fd,POLLERR,0};
                if(conn->want_read){
                    pfd.events |= POLLIN;
                }
                if(conn->want_write){
                    pfd.events |= POLLOUT;
                }
                poll_args.push_back(pfd);

            }
            int rv = poll(poll_args.data(),(nfds_t)poll_args.size(),-1);

            if(rv<0 & errno==EINTR){
                continue;
            }
            if(rv<0){
                perror("poll");
            }

            if(poll_args[0].revents){
                if(Conn *conn = handle_accept(fd)){
                    if(fd2conn.size()<= (size_t)conn->fd){
                        fd2conn.resize(conn->fd+1);
                    }
                    fd2conn[conn->fd] = conn;
                }
            }
            
            for(int i =1;i<poll_args.size();i++){
                uint32_t ready = poll_args[i].revents;
                Conn *conn = fd2conn[poll_args[i].fd];
                if(ready & POLLIN){
                    handle_read(conn); //application logic
                }
                if(ready & POLLOUT){
                    handle_write(conn);
                }

                //Close socket on error or if it wants
                if((ready&POLLERR) || conn->want_close){
                    close(conn->fd);
                    fd2conn[conn->fd] = nullptr;
                    delete conn;
                }

            }

        }
    

    return 0;
}