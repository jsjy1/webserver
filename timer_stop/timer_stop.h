#ifndef TIMER_STOP_H
#define TIMER_STOP_H


#include "http_user.h"
#include "thread_synchr.h"

#include <iostream>
#include <unordered_map>
#include <exception>
using namespace std;

//结点
struct Node{
    Node* last;
    Node* next;
    Http_user* cur;
    Node(Node* l=nullptr, Node* n=nullptr,Http_user* c=nullptr)
    {last=l;next=n;cur=c;}
};


//TODO: 内部再维护一个哈希表 用于查找时快速定位  可以手动根据fd实现
//时延链表 按时延时间递增存放(实际上从head到tailtime从大到小，即最后一次更新时间)
class Timer_stop_list{

public:

    Timer_stop_list(time_t timeout = 5);
    ~Timer_stop_list();

    void add(Http_user*);
    void del(Http_user*);       // 移除某个用户
    void adjust(Http_user*);    // 当前用户刚触发事件，调整改用户
    
    void clean();               // 清理超时的结点


    Node* head;         //头->尾 表示空
    Node* tail;     
    time_t m_timeout;   //超时时间

private:

    void del(Node*);            // 移除某个结点

    unordered_map<int, Node*> quick_pos;    //描述符 对应结点


};



#endif