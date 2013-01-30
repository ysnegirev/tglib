#include <tglib.h>
#include <cassert>
#include <cstdlib>
#include <cstring>

#include <apr_poll.h>
#include <apr_network_io.h>

#include <arpa/inet.h>

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
        createPool();
    }

    BaseImpl(const char *host, int port)
    {
        blocking = true;
        this->port = port;
        copyHost(host);
        createPool();
    }
    
    ~BaseImpl()
    {
        if(host)
            free(host);

        apr_pool_destroy(pool);
    }
    
    int getLastError()
    {
        int ret = (int)err;
        err = (apr_status_t)0;
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

    void setSockBlocking(bool blocking, apr_socket_t *s)
    {

        apr_socket_opt_set(s, APR_SO_NONBLOCK, (int)(!blocking));
        this->blocking = blocking;
    }

    bool isBlocking()
    {
        return blocking;
    }
protected:
    char *host;
    int port;
    apr_status_t err;
    apr_pool_t *pool;
    bool blocking;
    
    void createPool()
    {
        apr_status_t rv = apr_pool_create(&pool, NULL); //root pool
        TGLB_APR_ASSERT(rv);
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

    apr_sockaddr_t *createSocket(apr_socket_t **s)
    {
        
        apr_status_t rv;
        apr_sockaddr_t *sa;
        
        rv = apr_sockaddr_info_get(&sa, host, APR_INET, port, 0, pool);
        TGLB_APR_ASSERT(rv);
        
        rv = apr_socket_create(s, sa->family, SOCK_STREAM, APR_PROTO_TCP, pool);
        TGLB_APR_ASSERT(rv);

        return sa;
    }

    void setSockOpts(apr_socket_t *s)
    {
        //blocking mode
        apr_socket_opt_set(s, APR_SO_NONBLOCK, 0);
        
        //1 ms blocking timeout
        apr_socket_timeout_set(s, -1);

        //addres reusing
        apr_socket_opt_set(s, APR_SO_REUSEADDR, 1);
    }
    
    bool bindSocket(apr_socket_t *s, apr_sockaddr_t *addr)
    {
        bool ret = true;
        apr_status_t rv;

        rv = apr_socket_bind(s, addr);
        if (rv != APR_SUCCESS) {
            ret = false;
            err = rv;
        }
        return ret;
    }

    void setTimeout(int ms, apr_socket_t *s)
    {
        apr_socket_timeout_set(s, ms);
    }
};

//!!!CLIENT AND ACCEPTED PORT!!!

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
        sock = NULL;
        
        msgBuf.capacity = 100;
        msgBuf.filled = 0;
        msgBuf.pos = 0;
        msgBuf.buf = (char*)calloc(msgBuf.capacity, sizeof(char));
        assert(msgBuf.buf);
    };
    
    TGLCImpl(const char *host, int port)
    {
        blocking = true;
        sock = NULL;
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
        apr_sockaddr_t *sa= createSocket(&sock);
        
        //blocking mode
        apr_socket_opt_set(sock, APR_SO_NONBLOCK, 0);

        //default timeout is 1 second
        if(timeoutMs == 0)
            timeoutMs = 1000;

        apr_socket_timeout_set(sock, timeoutMs);

        apr_status_t rv = apr_socket_connect(sock, sa);
        if(rv != APR_SUCCESS) {
            ret = false;
            err = rv;
        }
        return ret;
    }

    void configureTimeout(int timeoutMs)
    {
        if(timeoutMs != 0) { //blocking mode
            setSockBlocking(true, sock);
        }
        else {
            setSockBlocking(false, sock);
        }
        setTimeout(timeoutMs, sock);
    }

    bool send(const char *data, size_t *len, int timeoutMs)
    {
        bool ret = true;

        configureTimeout(timeoutMs);

        size_t sent = 0, rem = *len;

        apr_status_t rv;
        while(sent < *len) {
            rem = *len - sent;
            rv = apr_socket_send(sock, data + sent, &rem);
            if(rv != APR_SUCCESS) {
                err = rv;
                ret = false;
                break;
            }
            sent += rem;
        }

        *len = sent;
        return ret;
    }

    bool recv(char *data, size_t *len, int timeoutMs)
    {
        bool ret = true;

        configureTimeout(timeoutMs);

        size_t received = 0, rem = *len;

        apr_status_t rv;
        while(received < *len) {
            rem = *len - received;
            rv = apr_socket_recv(sock, data + received, &rem);
            if(rv != APR_SUCCESS) {
                err = rv;
                ret = false;
                break;
            }
            received += rem;
        }

        *len = received;

        return ret;
    }

    void close()
    {
        if(sock)
            apr_socket_close(sock);
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
            size_t inBuf = msgBuf.filled - msgBuf.pos;
            size_t can = inBuf > bufSz ? bufSz : inBuf;
            memcpy((void*)buf, (const void*)(msgBuf.buf + msgBuf.pos), can);
            msgBuf.pos += can;
            size_t left = msgBuf.filled - msgBuf.pos;
            *bytesLeft = left;
            if(left > 0)
                ret = 0;
            else {
                msgBuf.filled = msgBuf.pos = 0;
                ret = can;
            }
        }
        else {
            uint32_t len = 0;
            apr_status_t rv;
            size_t bytesRead = 0, lenLen = 4;
            
            //read length
            while(bytesRead < 4) {
                rv = apr_socket_recv(sock, (char*)(&len), &lenLen);
                if (rv != APR_SUCCESS && bytesRead < 4) {
                    err =rv;
                    return -2;
                }
                bytesRead += lenLen;
                lenLen = 4 - bytesRead;
            }

            len = ntohl(len);

            //now we must read len bytes
            char *recv_buf = NULL;
            size_t rem = len;
            bytesRead = 0;
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

            while(bytesRead < len) {
                rem = len - bytesRead;
                rv = apr_socket_recv(sock, recv_buf, &rem);
                if( rv != APR_SUCCESS && bytesRead < len) {
                    err = rv;
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
    apr_socket_t *sock;
    struct MessBuffer
    {
        char *buf;
        size_t capacity;
        size_t filled;
        size_t pos;
    } msgBuf;
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
    return pimpl->recv(data, len, timeoutMs);
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
        backlog = NULL;
    }

    TGLSImpl(const char *host, int port) : BaseImpl(host, port)
    {
        blocking = true;
        backlog = NULL;
    }
    
    ~TGLSImpl()
    {
        if (backlog)
            apr_socket_close(backlog);
    }

    bool bind(const char *host = NULL, int port = 0)
    {
        setup(host, port);

        bool ret = true;
        
        apr_sockaddr_t *addr = createSocket(&backlog);

        setSockOpts(backlog);
        ret = bindSocket(backlog, addr);

        apr_status_t rv = apr_socket_listen(backlog, SOMAXCONN);
        TGLB_APR_ASSERT(rv);

        return ret;
    }

    bool accept(TGLPort *port, int timeout = 0)
    {
        assert(!port->pimpl->sock);
        assert(port->pimpl->pool);

        bool ret = true;
        if(timeout) {
            apr_socket_timeout_set(backlog, timeout);
        }
        apr_status_t rv = apr_socket_accept(&port->pimpl->sock, 
                                            backlog,
                                            port->pimpl->pool);
        if (rv != APR_SUCCESS) {
            ret = false;
            err = rv;
            char buf[80];
            char *ret = apr_strerror(rv, buf, 80);
            printf("accept fucked up: %s\n", buf);
        }
        return ret;
    }

    void close()
    {
        apr_status_t rv;
        if(backlog)
            rv = apr_socket_close(backlog);
        backlog = NULL;
    }
private:
    apr_socket_t *backlog;
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
        pimpl->setBlocking(false);
        ret = pimpl->accept(port);
    }
    else if(timeoutMs > 0) { //blocking with timeout
        pimpl->setBlocking(true);
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
    apr_initialize();
}

void TGLib_end()
{
    apr_terminate();
}
