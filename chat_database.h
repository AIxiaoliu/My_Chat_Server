#ifndef CHAT_DATABASE_H
#define CHAT_DATABASE_H

#include <mysql/mysql.h>
#include <mutex>
#include <iostream>
#include <stdio.h>
#include <jsoncpp/json/json.h>
#include <cstring>

using namespace std;

class DataBase
{
private:
	MYSQL *mysql;
	mutex _mutex;
public:
	DataBase();
	~DataBase();
    bool database_connect();
	void database_disconnect();
    bool database_init_table();
    bool database_user_is_exist(string);
    void database_insert_user_info(Json::Value &);
    int database_get_group_info(string *);
    bool database_password_correct(Json::Value &);
    bool database_get_friend_group(Json::Value &, string &, string &);
    void database_add_friend(Json::Value &);
    void database_update_friendlist(string &, string &);
    void database_add_new_group(string, string);
    void database_update_group_member(string, string);
    void database_update_info(string, string, string);
};

#endif

