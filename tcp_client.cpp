#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <assert.h>
#include <cstdint>

const size_t k_max_msg = 4096;


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


static int32_t query(int fd,char *text){
    uint32_t len = (u_int32_t)strlen(text);
    if(len > k_max_msg){
        return -1;
    }
    char wbuf[4 + len];
    memcpy(wbuf,&len,4);
    memcpy(&wbuf[4],text,len);
    int32_t err = write_all(fd,wbuf,4+len);
    if(err){
        return err;
    }


    char rbuf[4+k_max_msg];
    err = read_full(fd,rbuf,4);
    if(err){
        std::cout<<"Error while reading the response";
    }
    memcpy(&len,rbuf,4);
    if(len > k_max_msg){
        std::cout<<"Message Too long...";
    }
    err = read_full(fd,&rbuf[4],len);
    if(err){
        std::cout<<"Error while reading the response payload...";
    }
    std::cout<<&rbuf[4]<<"\n";
    if(err){
        return -1;
    }
    return 0;

}

int main(){
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        std::cout<<"Error in Socket Creation .. "<<"\n";
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // 127.0.0.1
    if (connect(fd, (const sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("connect");
    close(fd);
    return 1;
}

    int32_t err = query(fd, "hello1");
    err = query(fd,"hello2");


    close(fd);
    return 0;
}