#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <jsoncpp/json/json.h>

void *xx(void *arg)
{
	int sockfd = *(int *)arg;
	char buf[1024] = {0};
	int len;

	while (1)
	{
		if (recv(sockfd, &len, 4, 0) == 0)
		{
			break;
		}
		printf("收到长度 %d ", len);
		if (recv(sockfd, buf, len, 0) == 0)
			break;
		printf("收到数据 %s\n", buf);
		memset(buf, 0, 1024);
	}

	return NULL;
}

int main(){

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    struct sockaddr_in server_info;
    //memset(&server_info, 0, sizeof(server_info));
    server_info.sin_family = AF_INET;
    server_info.sin_addr.s_addr = inet_addr("172.23.171.44");
    server_info.sin_port = htons(8888);
    int size = sizeof(server_info);

    if (connect(sockfd, (struct sockaddr* )&server_info, size) == -1)
    {
        perror("connect");
        exit(1);
    }

    pthread_t tid;
	pthread_create(&tid, NULL, xx, &sockfd);

    Json::Value val;

	val["cmd"] = "creategroup";
	val["groupname"] = "干饭群";
	//val["password"] = "11111";
	val["owner"] = "小王";
	//val["text"] = "11111";

    //先将json对象转换成string
    std::string s = Json::FastWriter().write(val);
	char buf[128] = {0};
	int len = s.size();
	memcpy(buf, &len, 4);
	memcpy(buf + 4, s.c_str(), len);

	send(sockfd, buf, len + 4, 0);

    while (1)
    {
        
    };
    
    
    return 0;

}


