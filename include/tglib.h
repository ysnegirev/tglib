#ifndef _TGLIB_H
#define _TGLIB_H
class TGLSImpl;
class TGLCImpl;

void TGLib_start();
void TGLib_end();
/**
 * Port interface.
 */
class ITGLPort
{
public:
    virtual bool isBlocking()=0;
    virtual void setBLocking(bool blocking)=0;
    virtual void setPort(int port);
    virtual int getPort();
    
    /** Fetch last aquired null-terminated string. 
     * @return Last aquired string null-terminated string. Warning!!! This 
     * string is allocated from the internal buffer, so the user must obtain
     * its copy in order to modify it!
     */
    virtual const char *getLastString()=0;
    
    /** 
     * Send NULL-terminated string to remote hos
     */
    virtual bool sendString(const char *str)=0;

    virtual const char *getLastError()=0;
};

/** 
 * Server port class.
 * Binds server port to all (e.g. 0.0.0.0) interfaces and starts listening to 
 * the incoming connections. Current implementation allows only a single 
 * client to be served.
 */
class TGLibServerPort : public ITGLPort
{
public:
    TGLibServerPort();
    TGLibServerPort(int port);
    ~TGLibServerPort();

    /**
     * Start listening to connections on a specified port.
     * @return true on successful start and false on failure
     */
    bool start();

    virtual bool isBlocking();
    virtual void setBLocking(bool blocking);
    virtual void setPort(int port);
    virtual int getPort();

    virtual const char *getLastString();
    virtual bool sendString(const char *str);

    virtual const char *getLastError();
private:
    TGLSImpl *pimpl;
};

class TGLibClientPort : public ITGLPort
{
public:
    TGLibClientPort(const char *host = NULL);
    TGLibClientPort();
    ~TGLibClientPort();

    const char *getHost();
    vois setHost(const char *host);

    /**
     * Connect to remote host, using host and port supplied.
     * @return true on success and false on failure
     */
    bool connect();

    virtual bool isBlocking();
    virtual void setBLocking(bool blocking);
    virtual void setPort(int port);
    virtual int getPort();

    virtual const char *getLastString();
    virtual bool sendString(const char *str);

    virtual const char *getLastError();
private:
    TGLCImpl *pimpl;
};
#endif
