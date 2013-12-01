#include <tglib.h>
#include <cstdio>
#include <cstring>
#include <cassert>

using namespace std;

//preved-medved protocol implementation for a single client

int main(int argc, char **argv)
{
    //TGLib_start();
    
    TGLPort client;

    assert(client.connect("127.0.0.1", 1234, -1));
    printf("connected\n");

    char buf[100];
    memset(buf, 0, sizeof(buf));
    size_t left = 0;
    client.setMessRecvTimeout(-1);

    assert(client.sendMess("preved", strlen("preved") +1, -1));
    printf("sent preved\n");
    
    printf("receiving\n");
    memset(buf, 0, sizeof(buf));
    assert(client.recvMess(buf, 100, &left) > 0);
    printf("received: %s\n", buf);
    
    //TGLib_end();
    return 0;
}
