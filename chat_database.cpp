#include "chat_database.h"

DataBase::DataBase()
{

}

DataBase::~DataBase()
{

}

//连接数据库
bool DataBase::database_connect()
{
	//初始化数据库句柄
	mysql = mysql_init(NULL);

	//连接数据库
	mysql = mysql_real_connect(mysql, "localhost", "root", "root", 
			"chat_database", 0, NULL, 0);
	if (NULL == mysql)
	{
		cout << "mysql_real_connect error" << endl;
		return false;
	}

	//设置编码格式
	if (mysql_query(mysql, "set names utf8;") != 0)
	{
		cout << "set names utf8 error" << endl;
		return false;
	}

	return true;
}

//断开数据库
void DataBase::database_disconnect()
{
	mysql_close(mysql);
}

//初始化数据库表
bool DataBase::database_init_table()
{
	database_connect();

	const char *g = "create table if not exists chat_group(groupname varchar(128), groupowner varchar(128),groupmember varchar(4096))charset utf8;";
	if (mysql_query(mysql, g) != 0)
	{
		return false;
	}

	const char *u = "create table if not exists chat_user(username varchar(128), password varchar(128), friendlist varchar(4096), grouplist varchar(4096))charset utf8;";

	if (mysql_query(mysql, u) != 0)
	{
		return false;
	}

	database_disconnect();

	return true;
}

//获取数据库群信息
int DataBase::database_get_group_info(string *g)
{
	if (mysql_query(mysql, "select * from chat_group;") != 0)
	{
		cout << "select error" << endl;
		return -1;
	}

    //保存结果
	MYSQL_RES *res = mysql_store_result(mysql);
	if (NULL == res)
	{
		cout << "store result error" << endl;
		return -1;
	}

	MYSQL_ROW r;
	int idx = 0;
    //fetch_row读取的每一行的结果，r是一个数组,读取的结果放到g中
	while (r = mysql_fetch_row(res))
	{
		g[idx] += r[0];
		g[idx] += '|';
		g[idx] += r[2];
		//std::cout << g[idx] << std::endl;           //测试打印群信息
		idx++;
	}

	mysql_free_result(res);

	return idx;
}

//判断用户是否在数据库中存在，参数：用户的名字
bool DataBase::database_user_is_exist(string u)
{
	char sql[256] = {0};

	sprintf(sql, "select * from chat_user where username = '%s';", u.c_str());

	unique_lock<mutex> lck(_mutex);

	if (mysql_query(mysql, sql) != 0)
	{
		cout << "select error" << endl;
		return true;
	}

	MYSQL_RES *res = mysql_store_result(mysql);
	if (NULL == res)
	{
		cout << "store result error" << endl;
		return true;
	}

	MYSQL_ROW row = mysql_fetch_row(res);
	if (NULL == row)
	{
		return false;
	}
	else
	{
		return true;
	}
}

//将json对象插入到数据库
void DataBase::database_insert_user_info(Json::Value &v)
{
    //1.先解析json对象
	string username = v["username"].asString();
	string password = v["password"].asString();

	char sql[256] = {0};

	sprintf(sql, "insert into chat_user (username, password) values ('%s', '%s');", username.c_str(), password.c_str());

	unique_lock<mutex> lck(_mutex);

	if (mysql_query(mysql, sql) != 0)
	{
		cout << "insert into error" << endl;
	}
}

//登录的时候判断密码和数据库中的密码是否正确
bool DataBase::database_password_correct(Json::Value &v)
{
	string username = v["username"].asString();
	string password = v["password"].asString();

	char sql[256] = {0};
	sprintf(sql, "select password from chat_user where username = '%s';", username.c_str());

	unique_lock<mutex> lck(_mutex);

	if (mysql_query(mysql, sql) != 0) 
	{
		cout << "select password error" << endl;
		return false;
	}
	
	//保存查询结果
	MYSQL_RES *res = mysql_store_result(mysql);
	if(NULL == res)
	{
		cout << "mysql store result error" << endl;
		return false;
	}

	MYSQL_ROW row = mysql_fetch_row(res);
	if (NULL == row)
	{
		cout << "fetch row error" << endl;
		return false;
	}

	//row[0]就是查询到的密码，和输入的密码进行比较
	if (!strcmp(row[0], password.c_str()))
	{
		return true;
	}
	else
	{
		return false;
	}
}

//登录成功后，用于获取好友列表和群列表
bool DataBase::database_get_friend_group(Json::Value &v, string &friList, string &groList)
{
	char sql[1024] = {0};
	sprintf(sql, "select * from chat_user where username = '%s';", v["username"].asString().c_str());

	unique_lock<mutex> lck(_mutex);

	if (mysql_query(mysql, sql) != 0)
	{
		cout << "select * error" << endl;
		return false;
	}

	MYSQL_RES *res = mysql_store_result(mysql);
	if (NULL == res)
	{
		cout << "store result error" << endl;
		return false;
	}

	MYSQL_ROW row = mysql_fetch_row(res);
	if (NULL == row)
	{
		return false;
	}

	if (row[2])
	{
		friList = string(row[2]);
	}
	if (row[3])
	{
		groList = string(row[3]);
	}

	return true;
}

//互相修改数据库中的好友列表
void DataBase::database_add_friend(Json::Value &v)
{
	std::string username = v["username"].asString();
	std::string friendname = v["friend"].asString();

	database_update_friendlist(username, friendname);
	database_update_friendlist(friendname, username);
}

//在用户u的好友列表中添加用户f
void DataBase::database_update_friendlist(string &u, string &f)
{
	char sql[256] = {0};
	string friendlist;
	sprintf(sql, "select friendlist from chat_user where username = '%s';", u.c_str());

	unique_lock<mutex> lck(_mutex);

	if (mysql_query(mysql, sql) != 0)
	{
		cout << "select friendlist error" << endl;
		return;
	}

	MYSQL_RES *res = mysql_store_result(mysql);
	if (NULL == res)
	{
		cout << "store result error" << endl;
		return;
	}

	MYSQL_ROW row = mysql_fetch_row(res);
	if (NULL == row[0])
	{
		friendlist.append(f);
	}
	else 
	{
		friendlist.append(row[0]);
		friendlist.append("|");
		friendlist.append(f);
	}

	memset(sql, 0, sizeof(sql));

	sprintf(sql, "update chat_user set friendlist = '%s' where username = '%s';", friendlist.c_str(), u.c_str());
	//把新的好友列表插入到数据库当中，就是更新数据库
	if (mysql_query(mysql, sql) != 0)
	{
		std::cout << "update chat_user error" << std::endl;
	}
}

//往数据库中插入一个群信息，参数：群名，群主
void DataBase::database_add_new_group(string g, string owner)
{
	char sql[256] = {0};
	string grouplist;

	//修改chat_group表，刚开始只有群成员只有群主
	sprintf(sql, "insert into chat_group values ('%s', '%s', '%s');", g.c_str(), owner.c_str(), owner.c_str());

	unique_lock<mutex> lck(_mutex);

	if (mysql_query(mysql, sql) != 0)
	{
		cout << "insert error" << endl;
		return;
	}

	//修改chat_user表
	memset(sql, 0, sizeof(sql));
	sprintf(sql, "select grouplist from chat_user where username = '%s';",
			owner.c_str());

	if (mysql_query(mysql, sql) != 0)
	{
		cout << "select friendlist error" << endl;
		return;
	}

	MYSQL_RES *res = mysql_store_result(mysql);
	if (NULL == res)
	{
		cout << "store result error" << endl;
		return;
	}

	MYSQL_ROW row = mysql_fetch_row(res);
	if (NULL == row[0])
	{
		grouplist.append(g);
	}
	else 
	{
		grouplist.append(row[0]);
		grouplist.append("|");
		grouplist.append(g);
	}

	memset(sql, 0, sizeof(sql));

	sprintf(sql, "update chat_user set grouplist = '%s' where username = '%s';", grouplist.c_str(), owner.c_str());

	if (mysql_query(mysql, sql) != 0)
	{
		cout << "update chat_user error" << endl;
	}

}

//更新数据库群信息：在群信息表中更新群的成员，在用户表中更新用户的群列表
void DataBase::database_update_group_member(string g, string u)
{
	//先修改chat_group内容
	database_update_info("chat_group", g, u);

	//再修改chat_user内容
	database_update_info("chat_user", g, u);

}

//更新数据库中表的信息，参数：表名，群名，用户名
void DataBase::database_update_info(string table, string groupname, string username)
{
	//先把数据读出来
	char sql[256] = {0};
	string member;

	if (table == "chat_group")
	{	
		//查询群信息表
		sprintf(sql, "select groupmember from chat_group where groupname = '%s';", groupname.c_str());		
	}
	else if (table == "chat_user")
	{	
		//查询用户表
		sprintf(sql, "select grouplist from chat_user where username = '%s';", username.c_str());
	}

	unique_lock<mutex> lck(_mutex);

	if (mysql_query(mysql, sql) != 0)
	{
		cout << "select error" << endl;
		return;
	}

	MYSQL_RES *res = mysql_store_result(mysql);
	if (NULL == res)
	{
		cout << "store result error" << endl;
		return;
	}

	MYSQL_ROW row = mysql_fetch_row(res);
	if (row[0] == NULL)
	{
		if (table == "chat_group")
		{
			member.append(username);
		}
		else if (table == "chat_user")
		{
			member.append(groupname);
		}
	}
	else
	{
		if (table == "chat_group")
		{
			member.append(row[0]);
			member.append("|");
			member.append(username);
		}
		else if (table == "chat_user")
		{
			member.append(row[0]);
			member.append("|");
			member.append(groupname);
		}
	}

	mysql_free_result(res);

	//修改后再更新
	memset(sql, 0, sizeof(sql));

	if (table == "chat_group")
	{
		sprintf(sql, "update chat_group set groupmember = '%s' where groupname = '%s';", member.c_str(), groupname.c_str());
	}
	else if (table == "chat_user")
	{
		sprintf(sql, "update chat_user set grouplist = '%s' where username = '%s';", member.c_str(), username.c_str());
	}

	if (mysql_query(mysql, sql) != 0)
	{
		cout << "update chat_group error" << endl;
	}
}
