#ifndef _BASE_IMPL_H
#define _BASE_IMPL_H
#include <map>
#include <queue>
#include <stack>

//#include <apr_thread_proc.h>
//#include <apr_thread_mutex.h>
//#include <apr_network_io.h>

using namespace std;

void *thread_func(apr_thread_t *thread, void *param);
void *recv_thread_func(apr_thread_t *thread, void *param);
void *send_thread_func(apr_thread_t *thread, void *param);

class TGLBaseImpl
{
public:
    TGLBaseImpl();
    ~TGLBaseImpl();
    
    bool sendStr(const char *str);
    const char *recvStr();

    /**
     * Put the string, returned from "::"<recvStr> or ::<getLastError>, back into internal object's storage
     */
    void refundStr(char *str);

    void setPort(int port) { this->port = port; }
    int getPort() { return port; }

    void setHost(const char *host);
    const char *getHost() { return host; }

    bool isBlocking() { return blocking; }
    void setBlocking(bool blocking) { this->blocking = blocking; }

    virtual const char *getLastError();

    friend void *recv_thread_func(apr_thread_t *thread, void *param);
    friend void *send_thread_func(apr_thread_t *thread, void *param);
protected:
    apr_pool_t *pool;
    apr_thread_t *receiver;
    apr_thread_t *sender;
    apr_socket_t *socket;
    
    apr_thread_mutex_t *send_lock;
    apr_thread_mutex_t *recv_lock;
    apr_thread_mutex_t *spare_lock;
    apr_thread_mutex_t *size_lock;
    apr_thread_mutex_t *err_lock;

    map<char*,size_t> size_map;
    
    queue<char*> send_queue;
    queue<char*> recv_queue;

    queue<char*> spare_queue;

    stack<char*> errStack;

    bool stopping;
    bool opened;
    size_t longest_str;
    
    bool blocking;
    
    char *host;
    int port;

    char *buf;
    size_t capacity;
    size_t filled;

    char *errBuf;
    size_t errBufSz;

    void appendToSpare();
    bool getStringSize(char *ptr, size_t *res);
    bool resizeString(char **ptr, size_t str_len);
    void putErr(apr_status_t rv);
    bool createThreads();
private:
    bool createSpecificThread(apr_thread_t **worker, apr_thread_start_t func);
};
#endif
