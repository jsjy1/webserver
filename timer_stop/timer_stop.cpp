
#include "timer_stop.h"


Timer_stop_list::Timer_stop_list(time_t timeout)
{
    m_timeout = timeout;
    head = new Node();
    head->next = new Node(head, nullptr, nullptr);
    tail = head->next;  //初始 头->尾

    quick_pos.clear();
}

Timer_stop_list::~Timer_stop_list()
{
    if(head && tail) //Node里并没有开辟空间
    {
        while(head->next!=tail) // 从前往后释放
        {
            Node* tmp = head;
            head = head->next;
            delete tmp;
        }
        delete head;
        delete tail;
        head = nullptr;
        tail = nullptr;
    }
    quick_pos.clear();
}

//没有user用户的adjust
void Timer_stop_list::add(Http_user* user)
{
    adjust(user);
}

void Timer_stop_list::del(Http_user* user)
{
    Node* user_node = quick_pos[user->getfd()];

    if(user_node)
    {
        //释放结点
        user_node->last->next = user_node->next;
        user_node->next->last = user_node->last;
        delete user_node;

        quick_pos.erase(user->getfd());
    }
    
    user->rmfd();
}

void Timer_stop_list::del(Node* user_node)
{
    if(user_node->last==nullptr || user_node->next==nullptr)
    {
        cout<<"del-待移除的结点没有 last or next"<<endl;
        throw exception();
        return ;
    }

    quick_pos.erase(user_node->cur->getfd());
    user_node->cur->rmfd();

    //释放结点
    user_node->last->next = user_node->next;
    user_node->next->last = user_node->last;
    delete user_node;

}

void Timer_stop_list::adjust(Http_user* user)
{
    if(user->getfd()==-1)
    {
        cout<<"adjust传入错误用户"<<endl;
        throw exception();
        return ;
    } 
    
    if(user->getfd()==0)
    {
        cout<<"fd有问题！！！！"<<endl;
    }

    //调整用户时间
    user->last_time = time(nullptr);

    Node* user_node = quick_pos[user->getfd()];
    if(user_node)
    {
        //释放结点
        user_node->last->next = user_node->next;
        user_node->next->last = user_node->last;
        delete user_node;
    }

    // 头插
    Node* tmp = head->next;
    head->next = new Node(head, tmp, user);
    tmp->last = head->next;
    quick_pos[user->getfd()] = head->next;

    if(head->next->cur->getfd()==0||tail->last->cur->getfd()==0)
    {
        cout<<"有问题！"<<endl;
    }

    return ; 
}

void Timer_stop_list::clean()
{
    time_t cur_time = time(nullptr);
    while( head->next!=tail && cur_time - tail->last->cur->last_time > m_timeout)
    {
        del(tail->last); // 删除本结点一切记录
    }

}


