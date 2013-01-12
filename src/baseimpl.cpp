#include <baseimpl.h>
#include <apt_thread_mutex.h>
#include <apr_network_io.h>
#include <assert>
#include <cstring>
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
        delete ptr;
    }
}

void TGLBaseImpl::appendToSpare()
{
    char *appnd = new char[longest_str + 1];
    assert(appnd);

    assert(size_lock && spare_lock);

    apr_mutex_lock(size_lock);
    size_map[appnd] = longest_str + 1;
    apr_mutex_unlock(size_lock);

    apr_mutex_lock(spare_lock);
    spare_queue.push(appnd);
    apr_mutex_unlock(spare_lock);
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

void *thread_func(apr_thread_t *thread, void *param)
{
    APRBasePimpl *pimpl = (APRBasePimpl*) param;
    while(!stopping) {
        //TODO: wait for data in queues

        //send data
        apr_mutex_lock(send_lock);
        if(!send_queue.empty()) {
            char *snd = send_queue.back();
            send_queue.pop();
            size_t len = strlen(snd) + 1; //beware of null-terminator!
            size_t sent = 0, already_sent = 0;
            while(sent < len) {
                apr_socket_send(socket, snd, &already_sent);
                if(sent + already_sent < len) {
                    snd += already_sent;
                    sent += already_sent;
                }
            }
        }
        apr_mutex_unlock(send_lock);

        //receive data
        size_t real_cap = capacity - filled;
        if(real_cap < 10) {
            char *tmp = new char[capacity * 2];
            assert(tmp);
            memcpy(tmp, buf, filled);
            delete buf;
            buf = tmp;
            capacity *= 2;
            real_cap = capacity - filled;
        }
        char *buf_start = buf + filled;
        size_t want = real_cap, real_recv = real_cap;
        apr_socket_recv(socket, buf_start, real_recv);
        size_t term_idx = capacity + 1;
        int i;
        for(i = 0; i < real_recv; i++) {
            if(buf_start[i] == '\0') {
                //we have got a complete string. aquire a lock and
                //copy it to the recv_queue
                size_t our_size = filled + i;
                apr_mutex_lock(spare_lock);
                char *our_str = NULL;
                if(!spare_queue.empty()) {
                    our_str = spare_queue.back();
                    spare_queue.pop();
                    size_t curr_sz = 0;
                    assert(getStringSize(our_str, curr_sz));
                    if(curr < our_size) {
                        resizeString(our_str, curr_sz - 1);
                    }
                }
                else {
                    if (longest_str + 1 < our_size)
                        longest_str = our_size - 1;
                    our_str = new char[our_size];
                    apr_mutex_lock(size_lock);
                    size_map[our_str] = our_size;
                    apr_mutex_unlock(size_lock);
                }
                apr_mutex_unlock(spare_lock);

                if(term_idx == capacity + 1)
                    memcpy(our_str, buf, our_size);
                else {
                    our_size = i - term_idx - 1;
                    memcpy(our_str, buf_start + term_idx + 1, our_size);
                }
                
                apr_mutex_lock(recv_lock);
                recv_queue.push(our_str);
                apr_mutex_unlock(recv_lock);
                term_idx = i;
            }
        }
        if (term_idx != real_recv -1) {
            filled += real_recv;
        }
    }
    return NULL;
}
