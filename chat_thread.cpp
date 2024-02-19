#include "chat_thread.h"

ChatThread::ChatThread()
{
	_thread = new thread(worker, this);
	_id = _thread->get_id();

	base = event_base_new();
}

ChatThread::~ChatThread()
{
	if (_thread)
		delete _thread;
}

//将chatserver的数据库对象和数据结构对象传递给每个线程
void ChatThread::start(ChatInfo *i, DataBase *d)
{
	this->info = i;
	this->db = d;
}

//线程函数：
void ChatThread::worker(ChatThread *t)
{
    t->run();               //静态成员函数可以通过类对象调用普通成员函数，过渡
}

void ChatThread::run()
{
	//集合中放入一个定时器事件； 如何集合没有事件，event_base_dispatch(base)，将会退出
	struct event timeout;           //定时器事件
	struct timeval tv;              //定时器事件执行时间

    //定时器事件绑定集合, this：线程池对象
	event_assign(&timeout, base, -1, EV_PERSIST, timeout_cb, this);

	evutil_timerclear(&tv);
	tv.tv_sec = 3;                          //频次：3秒一次
	event_add(&timeout, &tv);               //时间加入到事件中

	cout << "---thread " << _id << " start working ..." << endl;

	event_base_dispatch(base);

	event_base_free(base);
}

void ChatThread::timeout_cb(evutil_socket_t fd, short event, void *arg)
{
	ChatThread *t = (ChatThread *)arg;              //arg参数：就是线程池对象
	//cout << "--- thread " << t->thread_get_id();
	//cout << " is listening ..." << endl;
}

//得到线程的id
thread::id ChatThread::thread_get_id()
{
	return _id;
}

//获取线程的事件集合
struct event_base *ChatThread::thread_get_base()
{
	return base;
}

void ChatThread::thread_readcb(struct bufferevent *bev, void *arg)
{
    ChatThread *t = (ChatThread *)arg;

    char buf[1024] = {0};
    if (!t->thread_read_data(bev, buf))
	{	
		cout << "thread read data error" << endl;
		return;
	}
    cout << "---thread " << t->thread_get_id() << " receive data :" << buf << endl;

    Json::Reader reader;           //json解析对象
	Json::Value val;                //保存json对象的位置
	//判断 buf 是不是 json 格式
	if (!reader.parse(buf, val))
	{
		cout << "ERROR : data is not json" << endl;
		return;
	}

    if (val["cmd"] == "register")
	{
        t->thread_register(bev, val);
	}
	else if (val["cmd"] == "login")
	{
		t->thread_login(bev, val);
	}
	else if (val["cmd"] == "addfriend")
	{
		t->thread_add_friend(bev, val);
	}
	else if (val["cmd"] == "private")
	{
		t->thread_private_chat(bev, val);
	}
	else if (val["cmd"] == "creategroup")
	{
		t->thread_create_group(bev, val);
	}
	else if (val["cmd"] == "joingroup")
	{
		t->thread_join_group(bev, val);
	}
	else if (val["cmd"] == "groupchat")
	{
		t->thread_group_chat(bev, val);
	}
	else if (val["cmd"] == "file")
	{
		t->thread_transfer_file(bev, val);
	}
	else if (val["cmd"] == "offline")
	{
		t->thread_client_offline(bev, val);
	}
	else if (val["cmd"] == "groupmember")
	{
		t->thread_get_group_member(bev, val);
	}

}

//客户端异常断开执行此函数，eventcb：表示在事件处理器发生错误或状态变化时调用的回调函数。
void ChatThread::thread_eventcb(struct bufferevent *b, short s, void *a)
{
	if (s & BEV_EVENT_EOF)
	{
		cout << "[disconnect] client offline" << endl;
		bufferevent_free(b);
	}
	else
	{
		cout << "UNKOWN ERROR" << endl;
	}
}

//服务器线程读取数据，包格式：长度+json字符串，
bool ChatThread::thread_read_data(struct bufferevent *bev, char *s)
{
	int size;
	size_t count = 0;

    //先读4个字节，是 libevent 库中的一个函数，用于读取套接字缓冲区事件处理器中的数据。
	if (bufferevent_read(bev, &size, 4) != 4)
	{
		return false;
	}

	char buf[1024] = {0};
	while (1)
	{
		count += bufferevent_read(bev, buf, 1024);
		strcat(s, buf);
		memset(buf, 0, 1024);
		if (count >= size)
		{
			break;
		}
	}

	return true;
}

//实现注册功能函数
void ChatThread::thread_register(struct bufferevent *bev, Json::Value &v)
{
	db->database_connect();

	//判断用户是否存在
	if (db->database_user_is_exist(v["username"].asString()))
	{
		Json::Value val;
		val["cmd"] = "register_reply";
		val["result"] = "user_exist";

		thread_write_data(bev, val);
	}
	else     //用户不存在
	{
		db->database_insert_user_info(v);

		Json::Value val;
		val["cmd"] = "register_reply";
		val["result"] = "success";

		thread_write_data(bev, val);
	}

	db->database_disconnect();
}

//服务器线程返回数据
void ChatThread::thread_write_data(struct bufferevent *bev, Json::Value &v)
{
	string s = Json::FastWriter().write(v);
	int len = s.size();
	char buf[1024] = {0};
	memcpy(buf, &len, 4);
	memcpy(buf + 4, s.c_str(), len);
	if (bufferevent_write(bev, buf, len + 4) == -1)
	{
		cout << "bufferevent_write error" << endl;
	}
}

//实现登录功能
void ChatThread::thread_login(struct bufferevent *bev, Json::Value &v)
{
	//判断用户是否已经登录
	struct bufferevent *b = info->list_friend_online(v["username"].asString());
	if (b)
	{
		//已经在线
		Json::Value val;
		val["cmd"] = "login_reply";
		val["result"] = "already_online";

		thread_write_data(bev, val);

		return;
	}

	db->database_connect();

	if (!db->database_user_is_exist(v["username"].asString()))
	{
		//用户不存在
		Json::Value val;
		val["cmd"] = "login_reply";
		val["result"] = "not_exist";

		thread_write_data(bev, val);

		db->database_disconnect();

		return;
	}

	//用户存在 判断密码是否正确
	if (!db->database_password_correct(v))
	{
		//密码不正确
		Json::Value val;
		val["cmd"] = "login_reply";
		val["result"] = "password_error";

		thread_write_data(bev, val);

		db->database_disconnect();

		return;
	}

	//获取好友列表以及群列表
	string friendlist, grouplist;
	if (!db->database_get_friend_group(v, friendlist, grouplist))
	{
		cout << "get friend list error" << endl;
		return;
	}

	db->database_disconnect();
	//cout << "登陆成功" << endl;
	//回复客户端登录成功
	Json::Value val;
	val["cmd"] = "login_reply";
	val["result"] = "success";
	val["friendlist"] = friendlist;
	val["grouplist"] = grouplist;

	thread_write_data(bev, val);

	//更新在线用户链表
	info->list_update_list(v, bev);

	//通知所有好友
	if (friendlist.empty())
	{
		return;
	}

	int idx = friendlist.find('|');
	int start = 0;
	while (idx != -1)
	{
		string name = friendlist.substr(start, idx - start);
		//给好友发送数据
		struct bufferevent *b = info->list_friend_online(name);
		if(NULL != b)
		{
			Json::Value val;
			val["cmd"] = "online";
			val["username"] = v["username"];
			thread_write_data(b, val);
		}

		start = idx + 1;
		idx = friendlist.find('|', idx + 1);
	}

	//最后一个好友同样的操作
	string name = friendlist.substr(start, idx - start);
	b = info->list_friend_online(name);
	if(NULL != b)
	{
		val.clear();
		val["cmd"] = "online";
		val["username"] = v["username"];

		thread_write_data(b, val);
	}
}

//实现添加好友功能
void ChatThread::thread_add_friend(struct bufferevent *bev, Json::Value &v)
{
	//判断是不是自己
	if (v["friend"] == v["username"])
	{
		return;
	}

	db->database_connect();

	//判断好友是否存在
	if (!db->database_user_is_exist(v["friend"].asString()))	
	{
		//好友不存在
		Json::Value val;
		val["cmd"] = "addfriend_reply";
		val["result"] = "not_exist";

		thread_write_data(bev, val);

		db->database_disconnect();

		return;
	}

	//判断是不是好友关系
	string friendlist, grouplist;
	if (db->database_get_friend_group(v, friendlist, grouplist))
	{
		string str[1024];
		//得到好友的个数
		int num = thread_parse_string(friendlist, str);
		for (int i = 0; i < num; i++)
		{
			if (str[i] == v["friend"].asString())
			{
				Json::Value val;
				val["cmd"] = "addfriend_reply";
				val["result"] = "already_friend";

				thread_write_data(bev, val);

				db->database_disconnect();

				return;
			}
		}
	}

	//修改数据库
	db->database_add_friend(v);

	db->database_disconnect();

	//回复好友
	Json::Value val;
	val["cmd"] = "be_addfriend";
	val["friend"] = v["username"];
	
	struct bufferevent * b;
	//去好友在线列表中去寻找好友的bev
	b = info->list_friend_online(v["friend"].asString());
	if (NULL != bev)
	{
		thread_write_data(b, val);	
	}

	//回复自己
	val.clear();
	val["cmd"] = "addfriend_reply";
	val["result"] = "success";
	val["friend"] = v["friend"];

	thread_write_data(bev, val);
}

//解析好友列表字符串，将好友名字保存在字符串数组中
int ChatThread::thread_parse_string(std::string &f, std::string *s)
{
	int count = 0, start = 0;
	int idx = f.find('|');

	while (idx != -1)
	{
		s[count++] = f.substr(start, idx - start); 
		start = idx + 1;
		idx = f.find('|', start);
	}
	s[count++] = f.substr(start);

	return count;
}

//实现私聊功能
void ChatThread::thread_private_chat(struct bufferevent *bev, Json::Value &v)
{
	//判断对方在不在线
	string name = v["tofriend"].asString();
	struct bufferevent *b = info->list_friend_online(name);	
	if (NULL == b)
	{
		//对方不在线
		Json::Value val;
		val["cmd"] = "private_reply";
		val["result"] = "offline";

		//对方不在线时，服务器回复自己
		thread_write_data(bev, val);

		return;
	}
	//转发数据
	Json::Value val;
	val["cmd"] = "private";
	val["fromfriend"] = v["username"];
	val["text"] = v["text"];
	//服务器转发聊天数据给好友
	thread_write_data(b, val);

}

//实现建群功能
void ChatThread::thread_create_group(struct bufferevent *bev, Json::Value &v)
{
	//判断群是否存在
	string groupname = v["groupname"].asString();
	if (info->list_group_is_exist(groupname))
	{
		Json::Value val;
		val["cmd"] = "creategroup_reply";
		val["result"] = "exist";

		thread_write_data(bev, val);

		return;
	}

	//不存在：修改数据库
	db->database_connect();

	db->database_add_new_group(groupname, v["owner"].asString());

	db->database_disconnect();

	//修改map
	info->list_add_new_group(groupname, v["owner"].asString());

	//返回客户端
	Json::Value val;
	val["cmd"] = "creategroup_reply";
	val["result"] = "success";
	val["groupname"] = v["groupname"];

	thread_write_data(bev, val);
}

//实现加群功能
void ChatThread::thread_join_group(struct bufferevent *bev, Json::Value &v)
{
	//判断群是否存在
	string groupname = v["groupname"].asString();
	string username = v["username"].asString();
	if (!info->list_group_is_exist(groupname))
	{
		Json::Value val;
		val["cmd"] = "joingroup_reply";
		val["result"] = "not_exist";

		thread_write_data(bev, val);

		return;
	}

	//判断是不是已经在群里
	if (info->list_member_is_group(groupname, username))
	{
		Json::Value val;
		val["cmd"] = "joingroup_reply";
		val["result"] = "already";

		thread_write_data(bev, val);

		return;
	}

	//修改数据库
	db->database_connect();

	db->database_update_group_member(groupname, username);

	db->database_disconnect();

	//修改map
	info->list_update_group_member(groupname, username);

	//通知群的所有成员
	struct bufferevent *b;
	string member;				//除自己以外的群成员，返回给自己
	//获得的群成员链表
	list<string> l = info->list_get_list(groupname);
	for (auto it = l.begin(); it != l.end(); it++)
	{
		if (*it == username)
		{
			continue;
		}

		member += *it;
		member += "|";

		//得到在线群成员的bev
		b = info->list_friend_online(*it);
		if (b != NULL)
		{
			Json::Value val;
			val["cmd"] = "new_member_join";
			val["groupname"] = groupname;
			val["username"] = username;

			thread_write_data(b, val);
		}
	}

	cout << member << endl;
	member.erase(member.size() - 1);		//删除群成员字符串中最后的一个'|'
	
	//通知本人
	Json::Value val;
	val["cmd"] = "joingroup_reply";
	val["result"] = "success";
	val["member"] = member;
	val["groupname"] = v["groupname"];

	thread_write_data(bev, val);
}

//实现群聊功能
void ChatThread::thread_group_chat(struct bufferevent *bev, Json::Value &v)
{
	//获取群成员
	string groupname = v["groupname"].asString();
	list<string> l = info->list_get_list(groupname);

	//转发数据
	struct bufferevent *b;

	for (auto it = l.begin(); it != l.end(); it++)
	{
		//忽略自己
		if (*it == v["username"].asString())
		{
			continue;
		}

		//判断是否在线
		b = info->list_friend_online(*it);
		if (NULL == b)
		{
			continue;
		}

		Json::Value val;
		val["cmd"] = "groupchat_reply";
		val["from"] = v["username"];
		val["groupname"] = groupname;
		val["text"] = v["text"];

		thread_write_data(b, val);
	}


}

//实现传输文件功能
void ChatThread::thread_transfer_file(struct bufferevent *bev, Json::Value &v)
{
	Json::Value val;
	string friendname = v["friendname"].asString();
	//1.看对方是否在线
	struct bufferevent *b = info->list_friend_online(friendname);
	if (!b)
	{
		val["cmd"] = "file_reply";
		val["result"] = "offline";

		thread_write_data(bev, val);

		return;
	}

	val["cmd"] = "file_reply";
	val["result"] = "online";

	thread_write_data(bev, val);

	if (v["step"] == "1")
	{
		//转发文件属性
		val["cmd"] = "file_name";
		val["filename"] = v["filename"];
		val["filelength"] = v["filelength"];
		val["fromuser"] = v["username"];
	}
	else if (v["step"] == "2")
	{
		//转发文件内容
		val["cmd"] = "file_transfer";
		val["text"] = v["text"];	
	}
	else if (v["step"] == "3")
	{
		//文件传输结束
		val["cmd"] = "file_end";

		bufferevent_free(bev);
	}

	thread_write_data(b, val);
}

//实现客户端下线功能
void ChatThread::thread_client_offline(struct bufferevent *bev, Json::Value &v)
{
	string username = v["username"].asString();

	//删除在线用户链表结点
	info->list_delete_user(username);

	//释放bufferevent
	bufferevent_free(bev);

	//通知好友
	string friendlist, grouplist;

	db->database_connect();

	db->database_get_friend_group(v, friendlist, grouplist);

	db->database_disconnect();

	if (friendlist.empty())
	{
		return;
	}

	string str[1024];
	int num = thread_parse_string(friendlist, str);

	for (int i = 0; i < num; i++)
	{
		Json::Value val;
		val["cmd"] = "friend_offline";
		val["username"] = v["username"];
		//看好友在不在线，在线就回复
		struct bufferevent *b = info->list_friend_online(str[i]);
		if (b)
		{
			thread_write_data(b, val);
		}

	}
	cout << "[disconnet] client offline" << endl;
}

//得到群的所有群成员
void ChatThread::thread_get_group_member(struct bufferevent *bev, Json::Value &v)
{
	string groupname = v["groupname"].asString();
	string member;

	info->list_get_group_member(groupname, member);

	Json::Value val;
	val["cmd"] = "groupmember_reply";
	val["member"] = member;

	thread_write_data(bev, val);
}

