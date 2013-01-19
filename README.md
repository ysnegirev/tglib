tglib
==

Tupoy Gruzchick (Dumb Porter) library. Helps children study network programming in C++.
At the moment, limited to IPv4 and single client-server connection


Interface Specificaton
==

TGLPort

 - bool connect(char* host, int port);
 - void send(char* data, int n);
 - int receive(char* data, int bufferSize);
 - close();
 - int getLastErrorCode();

TGLServerPort

 - bool bind(char* host, int port);
 - bool accept(TGLPort* port, int timeoutMillis);
 - close();




 
