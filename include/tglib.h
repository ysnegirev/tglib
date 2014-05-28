#ifndef _TGLIB_H
#define _TGLIB_H

#include <stddef.h>

class TGLSImpl;
class TGLCImpl;

void TGLib_start();
void TGLib_end();

class TGLPort
{
public:
    TGLPort(const char *host, int port);
    TGLPort();
    ~TGLPort();

    //const char *getHost();
    //void setHost(const char *host);
    //
    //void setPort(int port);
    //int getPort();


//_timeoutMillis_ parameter can have the following values:
// - -1 - block forever
// - 0 - non-blocking mode, not supported in _connect()_
// - any positive value - block for at most _timeoutMillis_
    /**
     * Connect to remote host, using host and port supplied.
     * @return true on success and false on failure
     */
    bool connect(char* host, int port, int timeoutMs);

    //virtual bool isBlocking();
    //virtual void setBLocking(bool blocking);

    bool send(const char *data, size_t *len);
    bool recv(char *data, size_t *len);

    void close();

    bool isClosed();

    //message-driven interface
    /*
     * Simple protocol: each message (e.g. raw data) is
     * preceeded by a 4-byte integer, identifying message length.
     */

    bool sendMess(const char *data, size_t bufSz);

    /**
     * Receive data, sent via primitive message-driven interface.
     * @return >0 when message successfully received.
     * -2 if actual message is larger then buffer. size of actual message written to left.
     * -1 if error occured.
     * 0 if no data has been received
     */
     signed long recvMess(char *data, size_t bufSz, size_t *left);

    void setRecvTimeout(int timeoutMs);

    int getLastError();
public:
    TGLCImpl *pimpl;
};

/**
 * Server port class.
 */


class TGLServerPort
{
public:
    TGLServerPort();
    TGLServerPort(const char *host, int port);
    ~TGLServerPort();

    /**
     * Start listening to connections on a specified port.
     * @return true on successful start and false on failure
     */
    bool bind(const char *port, int host);

    bool accept(TGLPort *port, int timeoutMs);

    int getLastError();

    bool isClosed();

    void close();
private:
    TGLSImpl *pimpl;
};

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
    typedef ssize_t SSIZE_T;
int WSAGetLastError(void)
{
    return errno;
}
#   define WSAEINPROGRESS EINPROGRESS
#   define  WSAEWOULDBLOCK EWOULDBLOCK
#   define closesocket close
#else
#   ifdef WINVER
#       undef WINVER
#   endif
#   define WINVER 0x0510 //I hope noone will use library on windows under 2000...
#   include <winsock2.h>
#   include <Ws2tcpip.h>
#   include <stdint.h>
#endif

class BaseImpl
{
public:
    BaseImpl()
    {
        blocking = true;
        host = NULL;
        port = 0;
        isAddrSet = false;
        memset(&addr, 0, sizeof(addr));
        curTimeoutMs = -1;
        closed = false;
    }

    BaseImpl(const char *host, int port)
    {
        this->host = NULL;
        blocking = true;
        this->port = port;
        copyHost(host);
        isAddrSet = false;
        memset(&addr, 0, sizeof(addr));
        curTimeoutMs = -1;
        closed = false;
    }

    ~BaseImpl()
    {
        if(host)
            free(host);
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

    void setSockBlocking(bool blocking, SOCKET s)
    {
#ifdef OS_UNIX
        int cur_flags = fcntl(s,F_GETFL);
        if (!blocking) {
            assert(fcntl(s, F_SETFL, cur_flags | O_NONBLOCK) == 0);
        }
        else {
            assert(fcntl(s, F_SETFL, cur_flags & (~O_NONBLOCK))==0);
        }
#else
        unsigned long val = !blocking;
        int ret = ioctlsocket(s, FIONBIO, &val);
        if (ret == -1) {
            printf("error on set blocking mode: %d\n", WSAGetLastError());
        }
        assert(ret == 0);
#endif
    }

    bool doCheckClosed(SOCKET s) {
        int error = 0;
        socklen_t len = sizeof (error);
        int retval = getsockopt (s, SOL_SOCKET, SO_ERROR, (char*)&error, &len);
        return retval != 0;
    }

    bool isClosed() {
        if (closed) {
            return true;
        }
        if (doCheckClosed(getSocket())) {
            closed = true;
            return true;
        }
        return false;
    }

    bool isBlocking()
    {
        return blocking;
    }
protected:
    virtual SOCKET getSocket() = 0;

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
                hints.ai_flags |= AI_PASSIVE;
            }

            char *port_str = NULL;
            char *host_to_pass = strdup(host);
            bool found = false;
            //host does not include port. create port_str from port value
            if(!(port_str = strchr(host,':'))) {
                port_str = new char[6];
                assert(sprintf(port_str, "%d",  port) > 0);
            }
            else {
                found = true;
                host_to_pass[port_str-host] = '\0';
                port_str++;
            }
            int ret = 0;
            if ( (ret = getaddrinfo(host_to_pass, port_str, &hints, &res)) != 0) {
               res = NULL;
#              if defined(OS_UNIX) || not defined(_MSC_VER)
               printf(gai_strerror(ret));
#              else
               wprintf(gai_strerror(ret));
#              endif
            }
            if(!found)
                delete [] port_str;
            free(host_to_pass);
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
        setTimeout(1, s);
        //address reusing
#ifdef OS_UNIX
        socklen_t optlen = sizeof(int);
        int optval = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, optlen);
#else
        BOOL reuseAddr = TRUE;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseAddr, sizeof(BOOL));
#endif
    }

    bool bindSocket(SOCKET s)
    {
        bool ret = true;

        int rv = ::bind(s, &addr, sizeof(addr));
        if (rv != 0) {
            ret = false;
            err = WSAGetLastError();
        }
        return ret;
    }

    void setTimeout(int ms, SOCKET s)
    {
        //1 ms blocking timeout
        int e;
        e = setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&ms, sizeof(int));
        if (e != 0) {
            printf("Error on set timeout: %d\n", WSAGetLastError());
        }
        e = setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char*)&ms, sizeof(int));
        if (e != 0) {
            printf("Error on set timeout: %d\n", WSAGetLastError());
        }
        curTimeoutMs = ms;
    }

    inline void msToTv(int ms, struct timeval &timeout)
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
    int err;
    bool blocking;
    bool isAddrSet;
    struct sockaddr addr;
    int curTimeoutMs;
    bool closed;
};

//!!!CLIENT AND ACCEPTED PORT!!!

static inline bool checkConnectionErr(SOCKET s)
{
    int sockerr = 0;
    socklen_t optlen = 0;
#ifdef OS_UNIX
    getsockopt(s, SOL_SOCKET, SO_ERROR, &sockerr, &optlen);
#else
    getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&sockerr, &optlen);
#endif // OS_UNIX
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

    ~TGLCImpl()
    {
        close();
        free(msgBuf.buf);
    }

    bool connect(const char *host, int port, int timeoutMs)
    {
        setup(host, port);
        bool ret = true;
        if(timeoutMs != 0) {
            sock = createSocket();
            createAddr(false);
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
            if(res == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) {
                fd_set  wr;
                FD_ZERO(&wr);
                struct timeval timeout;
                msToTv(timeoutMs, timeout);
                FD_SET(sock, &wr);
                int cnt = select(0, NULL, &wr, NULL, &timeout);
                if (cnt > 0) {
                    ret = checkConnectionErr(sock);
                }
                else {
                    ret = false;
                }
            } else {
                ret = false;
            }
        }
        else {//forever blocking connect
            int res = ::connect(sock, &addr, sizeof(struct sockaddr));
            setBlocking(true);
            configureTimeout(-1);
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
            setBlocking(true);
            setTimeout(timeoutMs, sock);
        }
        else if(timeoutMs < 0) { //blocking mode with infinite (in fact - very large) timeout
            setBlocking(true);
            setTimeout(1000000, sock);
        }
        else { //non-blocking mode
            setBlocking(false);
        }
    }

    bool send(const char *data, size_t *len)
    {
        bool ret = true;

        size_t sent = 0, rem = *len;
        SSIZE_T res;

        while(sent < *len) {
            rem = *len - sent;
#           ifdef OS_UNIX
            res = ::send(sock, (void*)(data + sent), rem, 0);
#           else
            res = ::send(sock, data + sent, rem, 0);
#           endif
            if(res < 0) {
                if (  !isBlocking() &&

                      (WSAGetLastError() == WSAEWOULDBLOCK)
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

    bool receive(char *data, size_t *len)
    {
        bool ret = true;

        size_t received = 0, rem = *len;
        SSIZE_T res;


        while(received < *len) {
            rem = *len - received;
#           ifdef OS_UNIX
            res = recv(sock, (void*)(data + received), rem, 0);
#           else
            res = recv(sock, data + received, rem, 0);
#           endif // OS_UNIX
            if(res < 0) {
                int lastErr = WSAGetLastError();
                if (!isBlocking() && (lastErr == WSAEWOULDBLOCK)) {
                    continue;
                }
                //printf("error on receive: %d\n", lastErr);
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
        if(sock != INVALID_SOCKET)
            ::closesocket(sock);
    }

    bool sendMess(const char *buf, size_t bufSz)
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

        return send(msgBuf.buf, &slen);
    }

    signed long recvMess(char *buf, size_t bufSz, size_t *bytesLeft)
    {
        signed long ret = 1;
        if(msgBuf.filled > 0) {
            //copy as much as we can to buf
            ret = copyReadBytes(buf, bufSz, bytesLeft);
        }
        else {
            int32_t len = readMessageLength();
            if(len < 0)
                return 0;
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
                if(!receive(recv_buf, &rem)) {
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
protected:

    SOCKET getSocket() {
        return sock;
    }

private:
    friend class TGLSImpl;
    friend class TGLServerPort;
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

    int32_t readMessageLength()
    {
        uint32_t len = 0;
        size_t bytesRead = 0, lenLen = 4;
        while(bytesRead < 4) {
            if (receive(((char*)&len) + bytesRead, &lenLen)) {
                bytesRead += lenLen;
                lenLen = 4 - bytesRead;
            } else {
                return -2;
            }
        }
        return len;
    }
};

//implemetation

TGLPort::TGLPort()
{
    pimpl = new TGLCImpl();
    TGLib_start();
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

bool TGLPort::send(const char *data, size_t *len)
{
    return pimpl->send(data, len);
}

bool TGLPort::recv(char *data, size_t *len)
{
    return pimpl->receive(data, len);
}

void TGLPort::close()
{
    pimpl->close();
}

bool TGLPort::sendMess(const char *buf, size_t bufSz)
{
    return pimpl->sendMess(buf, bufSz);
}

signed long TGLPort::recvMess(char *buf, size_t bufSz, size_t *bytesLeft)
{
    return pimpl->recvMess(buf, bufSz, bytesLeft);
}

void TGLPort::setRecvTimeout(int timeoutMs)
{
    pimpl->configureTimeout(timeoutMs);
}

int TGLPort::getLastError()
{
    return pimpl->getLastError();
}

bool TGLPort::isClosed() {
    return pimpl->isClosed();
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
        if (backlog != INVALID_SOCKET)
            ::closesocket(backlog);
    }

    bool bind(const char *host = NULL, int port = 0)
    {
        setup(host, port);

        bool ret = true;

        backlog = createSocket();
        createAddr(true);

        setSockOpts(backlog);
        ret = bindSocket(backlog);
        if (ret) {
            int rv = ::listen(backlog, SOMAXCONN);
            assert(rv == 0);
        }

        return ret;
    }

    bool checkClosed() {
        return doCheckClosed(backlog);
    }

    bool accept(TGLPort *port, int timeoutMs = 0)
    {
        assert(port->pimpl->sock == INVALID_SOCKET);

        return attemptAccept(port, timeoutMs);
    }

    bool attemptAccept(TGLPort *port, int timeoutMs)
    {
        //printf("start accepting, timeout: %d ms\n", timeoutMs);
        bool ret = true;
        socklen_t addrlen = sizeof(struct sockaddr);
        if(timeoutMs <= 0) { //forever or non-blocking accept
            if (timeoutMs == 0) {
                //printf("non-blocing accept\n");
                setBlocking(false);
            } else {
                //printf("blocking forever accept\n");
                setBlocking(true);
            }
            ret = (port->pimpl->sock = ::accept(backlog, &port->pimpl->addr, &addrlen)) > 0;
        }
        else { //blocking accept with timeout
            //printf("blocking accept with timout %d ms\n", timeoutMs);
            fd_set rd;
            FD_ZERO(&rd);
            FD_SET(backlog, &rd);
            struct timeval timeout;
            msToTv(timeoutMs, timeout);
            int cnt = select(0, &rd, NULL, NULL, &timeout);
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
            ::closesocket(backlog);
        backlog = INVALID_SOCKET;
    }

protected:

    SOCKET getSocket() {
        return backlog;
    }

private:
    SOCKET backlog;
};

//implementation

TGLServerPort::TGLServerPort()
{
    pimpl = new TGLSImpl();
    TGLib_start();
}

TGLServerPort::TGLServerPort(const char *host, int port)
{
    pimpl = new TGLSImpl(host, port);
}

TGLServerPort::~TGLServerPort()
{
    delete pimpl;
}

bool TGLServerPort::isClosed() {
    return pimpl->isClosed();
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
        pimpl->setBlocking(false);
        ret = pimpl->accept(port);
    }
    else if(timeoutMs > 0) { //blocking with timeout
        ret = pimpl->accept(port, timeoutMs);
    }
    else { //forever blocking
        pimpl->setBlocking(true);
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
#ifndef OS_UNIX
    static bool inited = false;
    if(!inited) {
        WORD ver = MAKEWORD(1,1);
        WSADATA data;
        WSAStartup(ver, &data);
        inited = true;
    }
#endif
}

void TGLib_end()
{

#ifndef OS_UNIX
    WSACleanup();
#endif
}
#endif
