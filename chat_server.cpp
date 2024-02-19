#include "chat_server.h"

ChatServer::ChatServer(){
    //初始化事件集合
    this->base = event_base_new();

    //初始化数据库对象
    db = new DataBase();

    //初始化数据库表（chat_user   chat_group）
	if (!db->database_init_table())
	{
		cout << "init table failure" << endl;
		exit(1);
	}

    //初始化数据结构对象
    info = new ChatInfo();

    //初始化群信息：把群信息从数据库读出来，放入map中
    server_update_group_info();

    //初始化线程池
    thread_num = 3;
	cur_thread = 0;
	pool = new ChatThread[thread_num];

	for (int i = 0; i < thread_num; i++)
	{
		pool[i].start(info, db);                //所有线程共用数据库对象和数据结构对象，所有需要传过去
	}
    

}

void ChatServer::server_update_group_info()
{
    //1.先连接数据库
	if (!db->database_connect())
	{
		exit(1);
	}

	string groupinfo[1024];       //临时变量， 最多1024个群
	int num = db->database_get_group_info(groupinfo);               //查出来的数据放入到了groupinfo

	cout << "--- group num :" << num << endl;

	db->database_disconnect();                  //断开数据库

	info->list_update_group(groupinfo, num);                        //将查出来的数据放入到map中

	//info->list_print_group();                 //用于测试
}

ChatServer::~ChatServer(){

    //释放数据库对象
    if (db)
    {
        delete db;
    }

    //释放数据结构对象
    if (info)
    {
        delete info;
    }
    
    

}

//创建监听对象
void ChatServer::listen(const char* ip, int port){

    //服务器信息
    struct sockaddr_in server_info;
    memset(&server_info, 0, sizeof(server_info));
    server_info.sin_family = AF_INET;
    server_info.sin_addr.s_addr = inet_addr(ip);
    server_info.sin_port = htons(port);
    //创建一个监听事件，this是把ChatServer这个对象作为参数传递给回调函数使用
    struct evconnlistener* listener = evconnlistener_new_bind(base, listener_cb, this, LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
                                                        5, (struct sockaddr*)&server_info, sizeof(server_info));
    if (NULL == listener)
    {
        cout << "evconnlistener_new_bind error" << endl;
        exit(1);
    }
    
    //事件循环，开始监听
    event_base_dispatch(base);          //死循环，如何集合没有事件，退出

    //释放监听事件
    evconnlistener_free(listener);
    
    //释放集合
    event_base_free(base);
    
}

//回调函数：有客户端发起连接，会触发该函数
void ChatServer::listener_cb(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *c, int socklen, void *arg){
    ChatServer *ser = (ChatServer *)arg;
    //需要强转,c：表示新连接的客户端地址
    struct sockaddr_in* client_info = (struct sockaddr_in*)c;
    cout << "[connection]" << endl;
    cout << "client ip :" << inet_ntoa(client_info->sin_addr) << endl;
    cout << " client port :" << client_info->sin_port << endl;

    //创建事件：放入线程池中
    ser->server_alloc_event(fd);

}

//给线程池中的线程分配事件
void ChatServer::server_alloc_event(int fd)
{
    //获取线程池中当前线程的事件集合
	struct event_base *t_base = pool[cur_thread].thread_get_base();
    //返回一个指向新创建的套接字缓冲区事件处理器的指针。应用程序可以使用返回的指针来操作缓冲区事件处理器，例如注册事件回调函数、发送和接收数据等。
	struct bufferevent *bev = bufferevent_socket_new(t_base, fd, BEV_OPT_CLOSE_ON_FREE);
	if (NULL == bev)
	{
		cout << "bufferevent_socket_new error" << endl;
		return;
	}
    //是 libevent 库中用于设置套接字缓冲区事件处理器回调函数的函数
	bufferevent_setcb(bev, ChatThread::thread_readcb, NULL, ChatThread::thread_eventcb, &pool[cur_thread]);
	bufferevent_enable(bev, EV_READ);

	cout << " to thread " << pool[cur_thread].thread_get_id() << endl;

	cur_thread = (cur_thread + 1) % thread_num;
}


