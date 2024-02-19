#include "chat_list.h"

ChatInfo::ChatInfo()
{
    //初始化
	online_user = new list<User>;
	group_info = new map<string, list<string> >;
}

//析构
ChatInfo::~ChatInfo()
{
	if (online_user)
		delete online_user;

	if(group_info)
		delete group_info;
}

//将临时数组的内容插入到map中
void ChatInfo::list_update_group(string *g, int size)
{
	int idx, start = 0;
	string groupname, membername;               //群名字，和成员名字
	list<string> l;                             //链表：保存群成员

	for (int i = 0; i < size; i++)
	{
		idx = g[i].find('|');
		groupname = g[i].substr(0, idx);
		//cout << groupname << std::endl;

		start = idx + 1;
		while (1)
		{
			idx = g[i].find('|', idx + 1);
			if (idx == -1)
				break;
			membername = g[i].substr(start, idx - start);
			l.push_back(membername);
			start = idx + 1;
		}

		membername = g[i].substr(start, idx - start);
		l.push_back(membername);

		group_info->insert(pair<string, list<string> >(groupname, l));

		l.clear();
	}
}

//打印map信息，group_info是一个map集合
void ChatInfo::list_print_group()
{
	for (auto it = group_info->begin(); it != group_info->end(); it++)
	{
		cout << it->first << "  ";                                      //群名
		for (auto i = it->second.begin(); i != it->second.end(); i++)
		{
			cout << *i << " ";                                          //群成员
		}
		cout << endl;
	}
}

//更新在线用户列表
bool ChatInfo::list_update_list(Json::Value &v, struct bufferevent *bev)
{
	User u = {v["username"].asString(), bev};

	unique_lock<mutex> lck(list_mutex);

	online_user->push_back(u);

	return true;
}

//返回在线用户链表中n的bev
struct bufferevent * ChatInfo::list_friend_online(string n)
{
	unique_lock<mutex> lck(list_mutex);

	for (auto it = online_user->begin(); it != online_user->end(); it++)
	{
		if(it->name == n)
		{
			return it->bev;
		}
	}

	return NULL;
}

//判断群名是否在map集合中存在
bool ChatInfo::list_group_is_exist(string name)
{
	unique_lock<mutex> lck(map_mutex);

	for (auto it = group_info->begin(); it != group_info->end(); it++)
	{
		if (it->first == name)
		{
			return true;
		}
	}

	return false;
}

//添加一个新的群信息，参数：群名和群用户名字
void ChatInfo::list_add_new_group(string g, string owner)
{
	list<string> l;			//群用户链表
	l.push_back(owner);		//刚开始只有群主一个人

	unique_lock<mutex> lck(map_mutex);

	//新群信息插入到map集合中
	group_info->insert(make_pair(g, l));
}

//判断用户是否在群里，参数：群名和用户名
bool ChatInfo::list_member_is_group(string g, string u)
{
	unique_lock<mutex> lck(map_mutex);

	for (auto it = group_info->begin(); it != group_info->end(); it++)
	{
		if (it->first == g)
		{
			for (auto i = it->second.begin(); i != it->second.end(); i++)
			{
				if (*i == u)
				{
					return true;
				}
			}

			return false;
		}
	}

	return false;
}

//更新map中的群成员链表的信息
void ChatInfo::list_update_group_member(string g, string u)
{
	unique_lock<mutex> lck(map_mutex);

	for (auto it = group_info->begin(); it != group_info->end(); it++)
	{
		if (it->first == g)
		{
			it->second.push_back(u);
		}
	}
}

//得到map集合中一个群的群成员链表
list<string> &ChatInfo::list_get_list(string g)
{
	auto it = group_info->begin();

	unique_lock<mutex> lck(map_mutex);

	for (; it != group_info->end(); it++)
	{
		if (it->first == g)
		{
			break;
		}
	}

	return it->second;
}

//删除在线用户链表中的一个节点
void ChatInfo::list_delete_user(string username)
{
	unique_lock<mutex> lck(list_mutex);

	for (auto it = online_user->begin(); it != online_user->end(); it++)
	{
		if (it->name == username)
		{
			online_user->erase(it);
			return;
		}
	}
}

//从群信息链表中得到一个群的所有群成员
void ChatInfo::list_get_group_member(string g, string &m)
{
	unique_lock<mutex> lck(map_mutex);

	for (auto it = group_info->begin(); it != group_info->end(); it++)
	{
		if (it->first == g)
		{
			for (auto i = it->second.begin(); i != it->second.end(); i++)
			{
				m.append(*i);
				m.append("|");
			}
			break;
		}
	}

	m.erase(m.length() - 1, 1);
}


