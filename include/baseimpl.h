#ifndef _BASE_IMPL_H
#define _BASE_IMPL_H
#include <map>
#include <queue>
#include <list>
#include <apr_thread_proc.h>

using namespace std;

class TGLBaseImpl
{
public:
    TGLBaseImpl();
    ~TGLBaseImpl();
    
    bool sendStr(const char *str);
    const char *recvStr();

    void setPort(int port);
    int getPort();

    void setHost(const char *host);
    const char *getHost();

    bool isBlocking();
    void setBlocking(bool blocking);

    virtual const char *getLastError();
protected:
    apr_pool_t *pool;
    apt_thread_t *worker;
    apr_socket_t *socket;
    
    apr_mutex_t *send_lock;
    apr_mutex_t *recv_lock;
    apr_mutex_t *spare_lock;
    apr_mutex_t *size_lock;
    
    map<char*,size_t> size_map;
    
    queue<char*, list> send_queue;
    queue<char*, list> recv_queue;

    queue<char*,list> spare_queue;

    bool stopping;
    bool opened;
    size_t longest_str;
    
    bool blocking;
    
    char *host;
    int port;

    char *buf;
    size_t capacity;
    size_t filled;

    void appendToSpare();
    bool getStringSize(const char *ptr, size_t *res);
    bool resizeString(char **ptr, size_t str_len);
};


void *thread_func(apr_thread_t *thread, void *param);
#endif
