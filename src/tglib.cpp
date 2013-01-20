#include <tglib.h>
#include <cassert>
#include <cstdlib>
#include <cstring>

#include <apr_poll.h>
#include <apr_network_io.h>

class BaseImpl
{
public:
    BaseImpl()
    {
        host = NULL;
        port = 0;
        createPool();
    }

    BaseImpl(const char *host, int port)
    {
        this->port = port;
        copyHost(host);
        createPool();
    }
    
    ~BaseImpl()
    {
        if(host)
            free(host);

        apr_status_t rv = apr_pool_destroy(pool);
        /*
         * The following assert is commented out because not so many
         * people give a damn about this error in destructor
         */
        //TGLS_APR_ASSERT(rv);
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
protected:
    char *host;
    int port;
    apr_status_t err;
    apr_pool_t *pool;
    
    void createPool()
    {
        apr_status_t rv = apr_pool_create(pool, NULL); //root pool
        TGLS_APR_ASSERT(rv);
    }

    void copyHost(const char *host)
    {
        size_t len = strlen(host);
        if(this->host) {
            size_t currLen = strlen(this->host);
            if (currLen < len) {
                this->host = realloc( (len+1) * sizeof(char));
            }
        }
        else
            this->host = calloc(len + 1, sizeof(char));
        assert(this->host);
        strcpy(this->host, host);
    }

    apr_sockaddr_t *createSocket(apr_socket_t **s)
    {
        
        apr_status_t rv;
        apr_sockaddr_t *sa;
        
        rv = apr_sockaddr_info_get(&sa, host, APR_INET, port, 0, pool);
        TGLS_APR_ASSERT(rv);
        
        rv = apr_socket_create(&s, sa->family, SOCK_STREAM, APR_PROTO_TCP, pool);
        TGLS_APR_ASSERT(rv);

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
        apt_status_t rv;

        rv = apr_socket_bind(s, sa);
        if (rv != APR_SUCCESS) {
            ret = false;
            err = rv;
        }
        return ret;
    }
};

//!!!CLIENT AND ACCEPTED PORT!!!

//private implementation
class TGLSimpl;
class TGLCimpl : public BaseImpl
{
public:
    TGLCimpl() : BaseImpl()
    {
        blocking = true;
        socket = NULL;
    };
    
    TGLCimpl(const char *host, int port)
    {
        blocking = true;
        socket = NULL;
    };

    //5 extra lines of code...
    ~TGLCimpl()
    {
        if(socket)
            apr_socket_close(socket);
    }

    bool connect(const char *host, int port, int timeoutMs)
    {
        setup();
        bool ret = true;
        apr_sockaddr_t *sa= createSocket(&socket);
        
        //blocking mode
        apr_socket_opt_set(socket, APR_SO_NONBLOCK, 0);

        //default timeout is 1 second
        if(timeoutMs == 0)
            timeoutMs = 1000;

        apr_socket_timeout_set(socket, timeoutMs);

        apr_status_t rv = apr_socket_connect(socket, sa);
        if(rv != APR_SUCCESS) {
            ret = false;
            err = rv;
        }
        return ret;
    }

    bool send(const char *data, size_t len)
    {

    }

private:
    friend class TGLSimpl;
};

//implemetation

TGLPort::TGLPort()
{
    pimpl = new TGLCimpl();
}

TGLPort::TGLPort(const char *host, int port)
{
    pimpl = new TGLCimpl(host, port);
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

    pimpl->setBlocking(true);
    ret = pimpl->connect(host, port, timeoutMs);

    return ret;
}

bool TGLPort::connect(int timeoutMs)
{
    return connect(NULL, 0, timeoutMs);
}

bool TGLPort::send(const char *data, size_t len)
{
    return pimpl->send(data, len);
}

bool TGLPort::recv(char *data, size_t len, int timeoutMs)
{
    return pimpl->recv(data, len, timeoutMs);
}

void TGLPort::close()
{
    pimpl->close();
}

int TGLPort::getLastError()
{
    return pimpl->getLastError();
}

//!!!SERVER PORT!!!

#define TGLS_APR_ASSERT(rv)\
    assert(rv == APR_SUCCESS)
//private implementation
class TGLSimpl : public BaseImpl
{
public:
    TGLSimpl() : BaseImpl()
    {
        blocking = true;;
        backlog = NULL;
    }

    TGLSimpl(const char *host, int port) : BaseImpl(host, port);
    {
        blocking = true;
        backlog = NULL;
    }
    
    ~TGLSimpl()
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

        return ret;
    }

    bool accept(TGLPort *port, int timeout = 0)
    {
        assert(!port->pimpl->socket);
        assert(port->pimpl->pool);

        bool ret = true;
        if(timeout) {
            apr_socket_timeout_set(backlog, timeout);
        }
        apr_status_t rv = apr_socket_accept(&port->socket, 
                                            backlog,
                                            port->pool);
        if (rv != APR_SUCCESS) {
            ret = false;
            err = rv;
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
    bool blocking;
};

//implementation

TGLServerPort::TGLServerPort()
{
    pimpl = new TGLSimpl();
}

TGLServerPort::TGlServerPort(const char *host, int port)
{
    pimpl = new TGLSimpl(host, port);
}

TGLServerPort::~TGLServerPort()
{
    delete pimpl;
}

bool TGLServerPort::bind()
{
    return pimpl->bind();
}

bool TGLServerPort::bind(const char *port, int host)
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

