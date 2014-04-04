#include <tglib.h>
#include <cstdio>
#include <cstring>
#include <cassert>

using namespace std;

//preved-medved protocol implementation for a single client

int main(int argc, char **argv)
{
    TGLib_start();
    
    TGLServerPort sp("0.0.0.0", 1234);
    TGLPort client;
    assert(sp.bind());
    printf("waiting for incoming connection\n");
    assert(sp.accept(&client, -1));
    printf("someone connected\n");

    char buf[100];
    memset(buf, 0, sizeof(buf));
    size_t left = 0;
    client.setMessRecvTimeout(-1);

    assert(client.recvMess(buf, 100, &left) > 0);
    printf("received: %s\n", buf);

    assert(client.sendMess("medved", strlen("medved") +1, -1));
    printf("sent medved\n");
    
    TGLib_end();
    return 0;
}
