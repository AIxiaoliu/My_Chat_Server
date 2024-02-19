#include <iostream>
#include "chat_server.h"

using namespace std;

int main()
{
    //创建服务器对象
    ChatServer s;
    //设置监听的IP  和  PORT
    s.listen(IP, PORT);

    return 0;
}








