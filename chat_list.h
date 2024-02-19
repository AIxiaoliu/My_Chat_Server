#ifndef CHAT_LIST_H
#define CHAT_LIST_H

#include <iostream>
#include <mutex>
#include <list>
#include <map>
#include <event.h>
#include <jsoncpp/json/json.h>

using namespace std;

struct User
{
	string name;                    //账号（用户名）
	struct bufferevent *bev;      //客户端对应的事件
};

class ChatInfo
{
private:
	//保存在线用户信息
	list<User> *online_user;
	//保存所有群的信息   key：value（群名：群用户链表）
	map<string, list<string> > *group_info;
	//访问在线用户的锁
	mutex list_mutex;
	//访问群信息的锁
	mutex map_mutex;

public:
	ChatInfo();
	~ChatInfo();
    void list_update_group(string *, int);
    void list_print_group();
	bool list_update_list(Json::Value &, struct bufferevent *);
	struct bufferevent *list_friend_online(string);
	bool list_group_is_exist(string);
	void list_add_new_group(string, string);
	bool list_member_is_group(string, string);
	void list_update_group_member(string, string);
	void list_delete_user(string);
	void list_get_group_member(string, string &);
	list<string> &list_get_list(string);

};


#endif