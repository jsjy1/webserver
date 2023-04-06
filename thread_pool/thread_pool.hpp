#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <exception>
#include <list>
#include "thread_synchr.h"
using namespace std;

//线程池类  处理T的任务
template<class T>
class Thread_pool{

public:
    
    Thread_pool(int pol =8, int req = 1000); // 线程池数目与请求队列最大长度


    ~Thread_pool();


    bool append(T*); //加入任务到线程池 

private:

    static void* work(void*); //子线程工作函数 从请求队列取任务并且处理

    pthread_t* pool;    //线程池数组
    int len_pol;

    list<T*> request;   // 请求队列
    int len_req;

    Locker m_lok;       //线程同步机制
    // Cond m_cond;     //暂时不用来处理任务 因为每次wait都需要唤醒 当任务一多时
    Sema m_sem;         //用来判断是否有任务需要处理
    
    bool m_stop;        //是否结束线程

};


template<class T>
Thread_pool<T>::Thread_pool(int pol, int req):
len_req(req),len_pol(pol),m_stop(false),pool(nullptr)
{
    if(pol<=0||req<=0) 
    {
        cout<<"输入参数有误, 线程池创建失败!\n"<<endl;
        throw exception();
    }

    pool = new pthread_t[pol];
    if(!pool)
    {
        throw exception();
    }
    
    for (int i = 0; i < len_pol; i++)
    {
        //创建子线程
        if(pthread_create(pool+i, nullptr, work, this)!=0)
        {
            delete[] pool;
            cout<<"线程池创建失败！\n"<<endl;
            throw exception();
        }
        
        //设置线程分离
        if(pthread_detach(pool[i])!=0)
        {
            delete[] pool;
            cout<<"线程分离失败！\n"<<endl;
            throw exception();
        }
    }

}

template<class T>
Thread_pool<T>::~Thread_pool()
{
    delete[] pool; 
    m_stop = true;
}

template<class T>
bool Thread_pool<T>::append(T* worker)
{
    this->m_lok.lock();

    if(this->request.size()>=len_req) 
    {
        this->m_lok.unlock(); 
        return false;
    }
    this->request.push_back(worker);

    this->m_lok.unlock(); 
    this->m_sem.post(); // 有任务 唤醒线程
    return true;
}

template<class T>
void* Thread_pool<T>::work(void* tmp)
{
    Thread_pool* thd_pol = (Thread_pool*) tmp;
    while(!thd_pol->m_stop)
    {
        //从请求队列取出任务
        thd_pol->m_sem.wait();
        thd_pol->m_lok.lock();
        
        if(thd_pol->request.size()==0) continue; // 等待任务唤醒 TODO: 虽然感觉前面已经用wait等待到了任务 但是还是必须加 不然debug会出现问题 
        
        T* worker= thd_pol->request.front();
        thd_pol->request.pop_front();

        thd_pol->m_lok.unlock();

        //工作
        worker->process();
    }
}



#endif