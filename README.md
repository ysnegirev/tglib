tglib
==
Tupoy Gruzchick (Dumb Porter) library. Helps children study network programming in C++.
At the moment, limited to IPv4.


Interface Specificaton
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

Windows notices
==
Using library in Windows requires linking against Ws2_32 library (e.g. "Windows Sockets 2"). For example, in Code::Blocks, one must perform the following actions in order to link against this library:
 - click "Settings" menu item, select "Compiler";
 - select "Linker settings" tab;
 - add libws2_32.a library. On my computer, this sits at the following path: C:\Program Files (x86)\CodeBlocks\MinGW\lib\libws2_32.a;
 - click 'Ok'.
 
In Visual Studio 2012:
 - fire up context menu for your project (right mouse button click on your project in Solution explorer) and select 'Properties' item;
 - open 'Linker' node, select 'Input' and edit 'Additional dependencies';
 - Enter Ws2_32.lib into input box and click 'Ok'