tglib
==
Tupoy Gruzchick (Dumb Porter) library. Helps children study network programming in C++.
At the moment, limited to IPv4 and single client-server connection



Interface Specificaton:
==

TGLPort
--

 - bool connect(char* host, int port, int timeoutMillis);
 - void send(char* data, int n);
 - int receive(char* data, int bufferSize);
 - void sendMess(char* data, int n);
 - int receiveMess(char* data, int bufferSize, int* bytesLeft);
 - void setReceiveTimeout(int timeoutMillis);
 - void close();
 - int getLastErrorCode();

TGLServerPort
--

 - bool bind(char* host, int port);
 - bool accept(TGLPort* port, int timeoutMillis);
 - void close();

Notes
==

_timeoutMillis_ parameter can have the following values:
 - -1 - block forever
 - 0 - non-blocking mode, not supported in _connect()_
 - any positive value - block for at most _timeoutMillis_


 
