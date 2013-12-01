#include <tglib.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdio>

#ifdef OS_UNIX
#   include <arpa/inet.h>
#   include <netinet/in.h>
#   include <netinet/tcp.h>
#   include <signal.h>
#   include <netdb.h>
#   include <errno.h>
#   include <arpa/inet.h>
#   include <fcntl.h>
#   include <unistd.h>
    typedef  int SOCKET;
#   define INVALID_SOCKET -1
#else
#endif

#define TGLB_APR_ASSERT(rv)\
    assert(rv == APR_SUCCESS)

class BaseImpl
{
public:
    BaseImpl()
    {
        blocking = true;
        host = NULL;
        port = 0;
        //createPool();
        isAddrSet = false;
        //createAddr(false);
        memset(&addr, 0, sizeof(addr));
    }

    BaseImpl(const char *host, int port)
    {
        blocking = true;
        this->port = port;
        copyHost(host);
        //createPool();
        //createAddr(false);
        memset(&addr, 0, sizeof(addr));
    }
    
    ~BaseImpl()
    {
        if(host)
            free(host);

        //apr_pool_destroy(pool);
    }
    
    int getLastError()
    {
        int ret = err;
        err = 0;
        return ret;
    }

    void setup(const char *host, int port)
    {
        if (host) {
            copyHost(host);
        }

        if(!port) {
            if(!this->port)
                abort();
        }
        else
            this->port = port;
    }

    //void setSockBlocking(bool blocking, apr_socket_t *s)
    //{

    //    apr_socket_opt_set(s, APR_SO_NONBLOCK, (int)(!blocking));
    //    this->blocking = blocking;
    //}


    void setSockBlocking(bool blocking, SOCKET s)
    {

        //apr_socket_opt_set(s, APR_SO_NONBLOCK, (int)(!blocking));
        //this->blocking = blocking;
#ifdef OS_UNIX
        int cur_flags = fcntl(s,F_GETFL);
        if (!blocking) {
            assert(fcntl(s, F_SETFL, cur_flags | O_NONBLOCK) == 0);
        }
        else {
            assert(fcntl(s, F_SETFL, cur_flags & (~O_NONBLOCK))==0);
        }

#endif
    }

    bool isBlocking()
    {
        return blocking;
    }
protected:
    void createPool()
    {
        //apr_status_t rv = apr_pool_create(&pool, NULL); //root pool
        //TGLB_APR_ASSERT(rv);
    }

    void copyHost(const char *host)
    {
        size_t len = strlen(host);
        if(this->host) {
            size_t currLen = strlen(this->host);
            if (currLen < len) {
                this->host =(char*) realloc((void*)this->host, (len+1) * sizeof(char));
            }
        }
        else
            this->host =(char*) calloc(len + 1, sizeof(char));
        assert(this->host);
        strcpy(this->host, host);
    }

    void createAddr(bool serverPort)
    {
        if(host && !isAddrSet) {
            struct addrinfo hints, *res;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family=AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            if(serverPort) {
                printf("getting server port address\n");
                hints.ai_flags |= AI_PASSIVE;
            }
            else {
                printf("SUDDENLY, getting client port address\n");
            }

            char *port_str = NULL;
            char *host_to_pass = strdup(host);
            bool found = false;
            //host does not include port. create port_str from port value
            if(!(port_str = strchr(host,':'))) {
                port_str = new char[6];
                assert(snprintf(port_str, 5, "%d",  port) > 0);
            }
            else {
                found = true;
                host_to_pass[port_str-host] = '\0';
                port_str++;
            }
            printf("port_str: %s; host: %s\n", port_str, host);
            //size_t all_len = 
            int ret = 0;
            if ( (ret = getaddrinfo(host_to_pass, port_str, &hints, &res)) != 0) {
               res = NULL;
               printf(gai_strerror(ret));
            }
            if(!found)
                delete [] port_str;
            free(host_to_pass);
            //addr = *res->ai_addr;
            memcpy((void*)&addr, (void*)res->ai_addr, sizeof(struct sockaddr));
            isAddrSet = true;
            freeaddrinfo(res);
        }
        else {
            memset(&addr, 0, sizeof(addr));
        }
    }

    SOCKET createSocket()
    {
        SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        return s;
    }

    void setSockOpts(SOCKET s)
    {
        //blocking mode
        setSockBlocking(true, s);
#ifdef OS_UNIX
        setTimeout(1, s);
        //address reusing
        socklen_t optlen = sizeof(int);
        int optval = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, optlen);
#endif
    }
    
    bool bindSocket(SOCKET s)
    {
        bool ret = true;

        int rv = ::bind(s, &addr, sizeof(addr));
        if (rv != 0) {
            ret = false;
            err = errno;
            perror("bind");
        }
        return ret;
    }

    void setTimeout(int ms, SOCKET s)
    {
#ifdef OS_UNIX
        struct timeval timeout;
        msToTv(ms, timeout);
        //1 ms blocking timeout
        socklen_t toSize = sizeof(timeout);
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, toSize);
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, toSize);
#endif
    }

    inline void msToTv(int ms, struct timeval timeout)
    {
        int s = 0;
        if (ms > 1000) {
            s = ms / 1000;
            ms = ms % 1000;
        }
        timeout.tv_sec = s;
        timeout.tv_usec = ms;
    }
    char *host;
    int port;
    //apr_status_t err;
    int err;
    //apr_pool_t *pool;
    bool blocking;
    bool isAddrSet;
    struct sockaddr addr;
};

//!!!CLIENT AND ACCEPTED PORT!!!

static inline bool checkConnectionErr(SOCKET s)
{
    int sockerr = 0;
    socklen_t optlen = 0;
    getsockopt(s, SOL_SOCKET, SO_ERROR, &sockerr, &optlen);
    if(sockerr != 0) {
        fprintf(stderr, "Connection error: %s\n", strerror(sockerr));
    }
    return sockerr == 0;
}

//private implementation
class TGLCImpl : public BaseImpl
{
public:
    void setBlocking(bool blocking)
    {
        setSockBlocking(blocking, sock);
    }

    TGLCImpl() : BaseImpl()
    {
        blocking = true;
        sock = INVALID_SOCKET;
        
        msgBuf.capacity = 100;
        msgBuf.filled = 0;
        msgBuf.pos = 0;
        msgBuf.buf = (char*)calloc(msgBuf.capacity, sizeof(char));
        assert(msgBuf.buf);
    };
    
    TGLCImpl(const char *host, int port) : BaseImpl(host, port)
    {
        blocking = true;
        sock = INVALID_SOCKET;
    };

    //5 extra lines of code...
    ~TGLCImpl()
    {
        close();
        
        free(msgBuf.buf);
    }

    bool connect(const char *host, int port, int timeoutMs)
    {
        setup(host, port);
        bool ret = true;
        
        if(timeoutMs != 0 ) {
            sock = createSocket();
            createAddr(false);
            if (timeoutMs < 0) { //block forever
                setBlocking(true);
                configureTimeout(-1);
            }
            ret = attemptConnection(timeoutMs);
        }
        else {
            fprintf(stderr, "Non-blocking connect is not supported\n");
            ret = false;
        }
        return ret;
    }

    bool attemptConnection(int timeoutMs)
    {
        bool ret = true;
        if(timeoutMs > 0) {
            /*
             * BSD socket API does not have timeout settings for
             * connect and accept. So, we must switch to non-blocking mode
             * and wait for appropriate event in select() call.
             */
            setBlocking(false);
            int res = ::connect(sock, &addr, sizeof(struct sockaddr));
            if(res != 0 && errno == EINPROGRESS) { 
                fd_set  wr;
                FD_ZERO(&wr);
                struct timeval timeout;
                msToTv(timeoutMs, timeout);
                FD_SET(sock, &wr);

                int cnt = select(sock + 1, NULL, &wr, NULL, &timeout);
                if(cnt > 0) {
                    ret = checkConnectionErr(sock);
                }
                else if(cnt == 0) {//noone did anything or timeout
                    checkConnectionErr(sock);
                }
                else {
                    perror("select_connect");
                }
            }
            else {
                perror("connect_nonblocking");
            }
        }
        else {//forever blocking connect
            int res = ::connect(sock, &addr, sizeof(struct sockaddr));
            if(res != 0) {
                ret = false;
                perror("connect_blocking");
            }
        }
        return ret;
    }


    void configureTimeout(int timeoutMs)
    {
        if(timeoutMs > 0) { //blocking with finite timeout mode
            setSockBlocking(true, sock);
            setTimeout(timeoutMs, sock);
        }
        else if(timeoutMs < 0) { //blocking mode with infinite (in fact - very large) timeout
            setBlocking(true);
            setTimeout(1000000, sock);
        }
        else { //non-blocking mode
            setSockBlocking(false, sock);
        }
    }

    bool send(const char *data, size_t *len, int timeoutMs)
    {
        bool ret = true;

        configureTimeout(timeoutMs);

        size_t sent = 0, rem = *len;
        ssize_t res;

        while(sent < *len) {
            rem = *len - sent;
            res = ::send(sock, (void*)(data + sent), rem, 0);
            if(res < 0) {
                if (  !isBlocking() && 
                      (errno == EAGAIN || errno == EWOULDBLOCK)
                )
                    continue;
                
                ret = false;
                break;
            }
            else {
                sent += res;
            }
        }

        *len = sent;
        return ret;
    }

    bool receive(char *data, size_t *len, int timeoutMs)
    {
        bool ret = true;

        configureTimeout(timeoutMs);

        size_t received = 0, rem = *len;
        ssize_t res = 0;

        while(received < *len) {
            rem = *len - received;
            res = recv(sock, (void*)(data + received), rem, 0);
            if(res < 0) {
                if (  !isBlocking() && 
                      (errno == EAGAIN || errno == EWOULDBLOCK)
                )
                    continue;
                
                ret = false;
                break;
            }
            else {
                received += res;
            }
        }

        *len = received;

        return ret;
    }

    void close()
    {
        //if(sock)
        //    apr_socket_close(sock);
        if(sock != INVALID_SOCKET)
            ::close(sock);
    }

    bool sendMess(const char *buf, size_t bufSz, int timeoutMs)
    {
        uint32_t len = (uint32_t)bufSz;

        uint32_t lenToSend = htonl(len);

        if(msgBuf.capacity < len + 4) {
            msgBuf.buf = (char*)realloc(msgBuf.buf, (len + 4) * sizeof(char));
            assert(msgBuf.buf);
            msgBuf.capacity = (len + 4);
        }

        len += 4;

        //filling send buffer
        memcpy(msgBuf.buf, (const void*)(&lenToSend), sizeof(uint32_t));
        memcpy((void*)(msgBuf.buf + 4), (const void*)buf, bufSz);

        size_t slen = len;

        return send(msgBuf.buf, &slen, timeoutMs);
    }

    signed long recvMess(char *buf, size_t bufSz, size_t *bytesLeft)
    {
        signed long ret = 1;
        if(msgBuf.filled > 0) {
            //copy as much as we can to buf
            ret = copyReadBytes(buf, bufSz, bytesLeft);
        }
        else {
            uint32_t len = readMessageLength();
            len = ntohl(len);

            //now we must read len bytes
            
            //prepairing receive buffer
            char *recv_buf = NULL;
            size_t rem = len;
            size_t bytesRead = 0;
            if(len <= bufSz)
                recv_buf = buf;
            else {
                recv_buf = msgBuf.buf;
                if(msgBuf.capacity < len) {
                    msgBuf.buf = (char*)realloc(msgBuf.buf, (len) * sizeof(char));
                    assert(msgBuf.buf);
                    msgBuf.capacity = len;
                }
            }

            int retries = 0;
            while(bytesRead < len && retries++ < 10) {
                rem = len - bytesRead;
                if(!receive(recv_buf, &rem, 0)) {
                    err = errno;
                    *bytesLeft = len - bytesRead;
                    return -1;
                }
                bytesRead += rem;
            }

            if(recv_buf != buf) {
                memcpy((void*)buf, (const void*)recv_buf, bufSz);
                msgBuf.filled = len;
                msgBuf.pos = bufSz;
                ret = 0;
            }
            else {
                ret = len;
            }
        }
        return ret;
    }

private:
    friend class TGLSImpl;
    friend class TGLServerPort;
    //apr_socket_t *sock;
    SOCKET sock;
    struct MessBuffer
    {
        char *buf;
        size_t capacity;
        size_t filled;
        size_t pos;
    } msgBuf;
    
    signed long copyReadBytes(char *buf, size_t bufSz, size_t *bytesLeft)
    {
        signed long ret = 0;
        size_t inBuf = msgBuf.filled - msgBuf.pos;
        size_t can = inBuf > bufSz ? bufSz : inBuf;
        memcpy((void*)buf, (const void*)(msgBuf.buf + msgBuf.pos), can);
        msgBuf.pos += can;
        size_t left = msgBuf.filled - msgBuf.pos;
        *bytesLeft = left;
        if(left < 0) {
            msgBuf.filled = msgBuf.pos = 0;
            ret = can;
        }
        return ret;
    }

    uint32_t readMessageLength()
    {
        uint32_t len = 0;
        size_t bytesRead = 0, lenLen = 4;
        while(bytesRead < 4) {
            if(!receive((char*)&len, &lenLen, -1) && lenLen < 4) {
                return -2;
            }
            bytesRead += lenLen;
            lenLen = 4 - bytesRead;
        }
        return len;
    }
};

//implemetation

TGLPort::TGLPort()
{
    pimpl = new TGLCImpl();
}

TGLPort::TGLPort(const char *host, int port)
{
    pimpl = new TGLCImpl(host, port);
}

TGLPort::~TGLPort()
{
    delete pimpl;
}

bool TGLPort::connect(char *host, int port, int timeoutMs)
{
    assert(host);
    assert(port);

    bool ret = true;

    ret = pimpl->connect(host, port, timeoutMs);

    return ret;
}

bool TGLPort::connect(int timeoutMs)
{
    return connect(NULL, 0, timeoutMs);
}

bool TGLPort::send(const char *data, size_t *len, int timeoutMs)
{
    return pimpl->send(data, len, timeoutMs);
}

bool TGLPort::recv(char *data, size_t *len, int timeoutMs)
{
    return pimpl->receive(data, len, timeoutMs);
}

void TGLPort::close()
{
    pimpl->close();
}

bool TGLPort::sendMess(const char *buf, size_t bufSz, int timeoutMs)
{
    return pimpl->sendMess(buf, bufSz, timeoutMs);
}

signed long TGLPort::recvMess(char *buf, size_t bufSz, size_t *bytesLeft)
{
    return pimpl->recvMess(buf, bufSz, bytesLeft);
}

void TGLPort::setMessRecvTimeout(int timeoutMs)
{
    pimpl->configureTimeout(timeoutMs);
}

int TGLPort::getLastError()
{
    return pimpl->getLastError();
}

//!!!SERVER PORT!!!

//private implementation
class TGLSImpl : public BaseImpl
{
public:
    void setBlocking(bool blocking)
    {
        setSockBlocking(blocking, backlog);
    }
    TGLSImpl() : BaseImpl()
    {
        blocking = true;;
        backlog = INVALID_SOCKET;
    }

    TGLSImpl(const char *host, int port) : BaseImpl(host, port)
    {
        blocking = true;
        backlog = INVALID_SOCKET;
    }
    
    ~TGLSImpl()
    {
        //if (backlog)
        //    apr_socket_close(backlog);
        if (backlog != INVALID_SOCKET)
            ::close(backlog);
    }

    bool bind(const char *host = NULL, int port = 0)
    {
        setup(host, port);

        bool ret = true;
        
        backlog = createSocket();
        createAddr(true);

        setSockOpts(backlog);
        char currHost[10], currPort[10];
        int nameinfoRet = getnameinfo(&addr, sizeof(addr),
                    currHost, sizeof(currHost),
                    currPort, sizeof(currPort),
                    0);
        if(nameinfoRet != 0) {
            switch(nameinfoRet) {
                case EAI_AGAIN:
                    printf("should try again later\n");
                    break;
                case EAI_BADFLAGS:
                    printf("flags are shitty\n");
                    break;
                case EAI_FAIL:
                    printf("epic fail\n");
                    break;
                case EAI_FAMILY:
                    printf("bad addres family\n");
                    break;
                case EAI_MEMORY:
                    printf("out of memory\n");
                    break;
                case EAI_NONAME:
                    printf("name cannot be resolved\n");
                    break;
                case EAI_OVERFLOW:
                    printf("too small buffers\n");
                    break;
                case EAI_SYSTEM:
                    printf("bad shit happened...\n");
                    perror("getnameinfo");
                    break;
            }
        }
        else {
            printf("got host: %s, port: %s\n", currHost, currPort);
        }
        
        ret = bindSocket(backlog);
        if (ret) {
            //apr_status_t rv = apr_socket_listen(backlog, SOMAXCONN);
            int rv = ::listen(backlog, SOMAXCONN);
            assert(rv == 0);
            printf("listening on backlog\n");
        }

        return ret;
    }

    bool accept(TGLPort *port, int timeoutMs = 0)
    {
        //assert(!port->pimpl->sock);
        //assert(port->pimpl->pool);

        assert(port->pimpl->sock == INVALID_SOCKET);

        return attemptAccept(port, timeoutMs);
    }
    
    bool attemptAccept(TGLPort *port, int timeoutMs)
    {
        bool ret = true;
        socklen_t addrlen = sizeof(struct sockaddr);
        if(timeoutMs <= 0) { //forever or non-blocking accept
            printf("forever or non-blocking accept\n");
            ret = (port->pimpl->sock = ::accept(backlog, &port->pimpl->addr, &addrlen)) > 0;
            printf("accepted socket: %d\n", port->pimpl->sock);
        }
        else { //blocking accept with timeout
            fd_set rd;
            FD_ZERO(&rd);
            FD_SET(backlog, &rd);
            struct timeval timeout;
            msToTv(timeoutMs, timeout);
            int cnt = select(backlog +1, &rd, NULL, NULL, &timeout);
            if(cnt > 0) {
                ret = (port->pimpl->sock = ::accept(backlog, &port->pimpl->addr, &addrlen)) > 0;
            }
            else if (cnt < 0) {
                perror("select_accept");
                ret = false;
            }
            else {
                ret = false;
            }
        }
        return ret;
    }

    void close()
    {
        if(backlog != INVALID_SOCKET)
            ::close(backlog);
        backlog = INVALID_SOCKET;
    }
private:
    //apr_socket_t *backlog;
    SOCKET backlog;
};

//implementation

TGLServerPort::TGLServerPort()
{
    pimpl = new TGLSImpl();
}

TGLServerPort::TGLServerPort(const char *host, int port)
{
    pimpl = new TGLSImpl(host, port);
}

TGLServerPort::~TGLServerPort()
{
    delete pimpl;
}

bool TGLServerPort::bind()
{
    return pimpl->bind();
}

bool TGLServerPort::bind(const char *host, int port)
{
    assert(port);
    assert(host);
    return pimpl->bind(host, port);
}

bool TGLServerPort::accept(TGLPort *port, int timeoutMs)
{
    assert(port);
    bool ret = true;
    
    if (timeoutMs == 0) { //nonblocking mode
        ret = pimpl->accept(port);
    }
    else if(timeoutMs > 0) { //blocking with timeout
        ret = pimpl->accept(port, timeoutMs);
    }
    else { //forever blocking
        ret = pimpl->accept(port);
    }

    return ret;
}

int TGLServerPort::getLastError()
{
    int ret = 0;
    ret = pimpl->getLastError();
    return ret;
}

void TGLServerPort::close()
{
    pimpl->close();
}

void TGLib_start()
{
    //apr_initialize();
}

void TGLib_end()
{
    //apr_terminate();
}
