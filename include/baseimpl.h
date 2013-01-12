#ifndef _BASE_IMPL_H
#define _BASE_IMPL_H
#include <map>
#include <queue>
#include <list>

using namespace std;

struct APRBasePimpl;

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
private:
    APRBasePimpl *pimpl
};
#endif
