#include <apt_thread_mutex.h>
#include <apr_network_io.h>
#include <assert>
#include <cstring>
#include <baseimpl.h>

void *thread_func(apr_thread_t *thread, void *param)
{
    TGLBaseImpl *pimpl = (TGLBaseImpl*) param;
    bool sendEmpty = false, recvEmty = false;
    apr_status_t rv = APR_SUCCESS;
    while(!stopping) {
        //TODO: use socket polling

        //send data
        apr_mutex_lock(pimpl->send_lock);
        if(!pimpl->send_queue.empty()) {
            char *snd = pimpl->send_queue.back();
            pimpl->send_queue.pop();
            char *orig = snd;
            size_t len = strlen(snd) + 1; //beware of null-terminator!
            size_t sent = 0, already_sent = 0;
            while(sent < len) {
                rv = apr_socket_send(pimpl->socket, snd, &already_sent);

                if(rv != APR_SUCCESS) {
                    pimpl->putErr(rv);
                }

                if(sent + already_sent < len) {
                    snd += already_sent;
                    sent += already_sent;
                }
            }
            pimpl->refundStr(orig);
        }
        
        if(pimpl->send_queue.empty())
            sendEmpty = true;
        apr_mutex_unlock(send_lock);

        //receive data
        size_t real_cap = pimpl->capacity - pimpl->filled;
        if(real_cap < 10) {
            char *tmp = new char[pimpl->capacity * 2];
            assert(tmp);
            memcpy(tmp, pimpl->buf, pimpl->filled);
            delete pimpl->buf;
            pimpl->buf = tmp;
            capacity *= 2;
            real_cap = pimpl->capacity - filled;
        }
        char *buf_start = pimpl->buf + pimpl->filled;
        size_t want = real_cap, real_recv = real_cap;
        rv = apr_socket_recv(pimpl->socket, buf_start, real_recv);

        if(rv != APR_SUCCESS) {
            pimpl->putErr(rv);
        }

        size_t term_idx = pimpl->capacity + 1;
        int i;
        for(i = 0; i < real_recv; i++) {
            if(buf_start[i] == '\0') {
                //we have got a complete string. aquire a lock and
                //copy it to the recv_queue
                size_t our_size = pimpl->filled + i;
                apr_mutex_lock(pimpl->spare_lock);
                char *our_str = NULL;
                if(!pimpl->spare_queue.empty()) {
                    our_str = pimpl->spare_queue.back();
                    pimpl->spare_queue.pop();
                    size_t curr_sz = 0;
                    assert(pimpl->getStringSize(our_str, curr_sz));
                    if(curr < our_size) {
                        pimpl->resizeString(our_str, curr_sz - 1);
                    }
                }
                else {
                    if (pimpl->longest_str + 1 < our_size)
                        pimpl->longest_str = our_size - 1;
                    our_str = new char[our_size];
                    apr_mutex_lock(pimpl->size_lock);
                    pimpl->size_map[our_str] = our_size;
                    apr_mutex_unlock(pimpl->size_lock);
                }
                apr_mutex_unlock(pimpl->spare_lock);

                if(term_idx == capacity + 1)
                    memcpy(our_str, pimpl->buf, our_size);
                else {
                    our_size = i - term_idx - 1;
                    memcpy(our_str, buf_start + term_idx + 1, our_size);
                }
                
                apr_mutex_lock(pimpl->recv_lock);
                recv_queue.push(pimpl->our_str);
                apr_mutex_unlock(pimpl->recv_lock);
                term_idx = i;
            }
        }
        if (term_idx != real_recv -1) {
            pimpl->filled += real_recv;
        }
    }
    return NULL;
}

TGLBaseImpl::TGLBaseImpl()
{
    apr_status_t res;
    res = apr_pool_create(&pool, NULL);
    assert(res == APR_SUCCESS);

    res = apr_mutex_create(&send_lock, APR_TREAD_MUTEX_NESTED, pool);
    assert(res == APR_SUCCESS);

    res = apr_mutex_create(&recv_lock, APR_TREAD_MUTEX_NESTED, pool);
    assert(res == APR_SUCCESS);

    res = apr_mutex_create(&spare_lock, APR_TREAD_MUTEX_NESTED, pool);
    assert(res == APR_SUCCESS);
    
    res = apr_mutex_create(&size_lock, APR_TREAD_MUTEX_NESTED, pool);
    assert(res == APR_SUCCESS);

    res = apr_mutex_create(&err_lock, APR_TREAD_MUTEX_NESTED, pool);
    assert(res == APR_SUCCESS);


    stopping = false;
    opened = false;

    host = NULL;
    port = 0;

    socket = NULL;

    worker = NULL;
    longest_str = 80;

    capacity = 80;
    filled = 0;
    buf = new char[capacity];

    errBufSz = 80;
    errBuf = new char[errBufSz];

    //create a couple of spare strings
    appendToSpare();
    appendToSpare();
}


TGLBaseImpl::~TGLBaseImpl()
{
    stopping = false;
    if(worker)
        apr_thread_exit(worker, 0);

    apr_pool_clear(pool);

    //draining queues
    while (!send_queue.empty())
        send_queue.pop();

    while (!recv_queue.empty())
        recv_queue.pop();

    for(map<char*,size_t>::iterator = size_map.begin(); it != size_map.end(); it++) {
        char *ptr = it->first;
        delete [] ptr;
    }

    delete [] buf;
    delete [] errBuf;
}


bool TGLBaseImpl::sendStr(const char *str)
{
    bool ret = true;

    char *myStr = NULL;
    size_t mySz = 0;

    apr_mutex_lock(spare_lock);
    if(!spare_queue.empty()) {
        myStr = spare_queue.back();
        spare_queue.pop();
        assert(getStringSize(myStr, mySz));
    }
    else {
        mySz = strlen(str) + 1;
        myStr = new char[mySz];
        assert(myStr);

        apr_mutex_lock(size_lock);
        size_map[myStr] = mySz;
        apr_mutex_unlock(size_lock);
    }

    strncpy(myStr, str, mySz);

    apr_mutex_lock(send_lock);
    send_queue.push(myStr);
    apr_mutex_unlock(send_lock);

    return ret;
}

const char* TGLBaseImpl::recvStr()
{
    char *ret = NULL;
    apr_mutex_lock(recv_lock);
    if(!recv_queue.empty()) {
        ret = recv_queue.back();
        recv_queue.pop();
    }
    apr_mutex_unlock(recv_lock);
    return ret;
}

void TGLBaseImpl::refundStr(const char *str)
{
    apr_mutex_lock(spare_lock);
    spare_queue.push(str);
    apr_mutex_unlock(spare_lock);
}

void TGLBaseImpl::setHost(const char *host)
{
    size_t len = strlen(host);
    this->host = new char[len+1];
    strncpy(this->host, host, len+1);
}

const char* TGLBaseImpl::getLastError()
{
    char *ret = NULL;
    apr_mutex_lock(err_lock);
    if(!errStack.empty()) {
        ret = errStack.top();
        errStack.pop();
    }
    apr_mutex_unlock(err_lock);
    return ret;
}

void TGLBaseImpl::appendToSpare()
{
    char *appnd = new char[longest_str + 1];
    assert(appnd);

    assert(size_lock && spare_lock);

    apr_mutex_lock(size_lock);
    size_map[appnd] = longest_str + 1;
    apr_mutex_unlock(size_lock);

    refundStr(appnd);

}

bool TGLBaseImpl::getStringSize(const char *ptr, size_t *res)
{
    bool ret = false;
    assert(ptr && res);
    
    apr_mutex_lock(size_lock);
    map<char*,size_t>::iterator it = size_map.find(ptr);
    if(it != size_map.end()) {
        ret = true;
        *res = it->second();
    }
    apr_mutex_unlock(size_lock);

    return ret;
}

bool TGLBaseImpl::resizeString(char **ptr, size_t str_len)
{
    size_t curr_sz = 0;
    bool ret = true;
    if(!getStringSize(*ptr, &curr_sz))
        ret = false;
    else if (str_len > curr_sz - 1) {
        apr_mutex_lock(size_lock);
        size_map.erase(*ptr);
        
        delete *ptr;
        
        *ptr = new char[str_len + 1];
        assert(*ptr);

        size_map[*ptr] = str_len + 1;

        apr_mutex_unlock(size_lock);
    }
    return ret;
}

void TGLBaseImpl::putErr(apr_status_t rv)
{
    apr_strerror(rv, errBuf, errBufSz);
    char *errStr = NULL;
    
    apr_mutex_lock(spare_lock);
    if(!spare_queue.empty()) {
        errStr = spare_queue.back();
        spare_queue().pop();
    }
    else {
        errStr = new char [errBufSz];
        apr_mutex_lock(size_lock);
        size_map[errStr] = errBufSz;
        apr_mutex_unlock(size_lock);
    }
    apr_mutex_unlock(spare_lock);

    apr_mutex_lock(err_lock);
    errStack.push(errStr);
    apr_mutex_unlock(err_lock);
}

#define TGL_APR_ASSERT\
    if(rv != APR_SUCCESS) {\
        putErr(rv);\
        ret = false;\
        goto end;
    }
bool TGLBaseImpl::createThread()
{
    bool ret = true;
    apr_threadattr_t *attr = NULL;
    apr_status_t rv = APR_SUCCESS;
    
    rv = apr_threadattr_create(&attr, pool);
    TGL_APR_ASSERT;

    rv = apr_threadattr_detach_set(attr, 1);
    TGL_APR_ASSERT;

    rv = apr_thread_create(&worker, attr, (apr_thread_start_t)thread_func, this, pool);
    TGL_APR_ASSERT;

end:
    return true;
}
