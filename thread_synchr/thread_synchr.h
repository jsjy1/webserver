#ifndef THREAD_SYNCHR_H
#define THREAD_SYNCHR_H

#include <pthread.h>
#include <semaphore.h>
#include <exception>
using namespace std;

//互斥锁类
class Locker{
public:
    Locker();
    ~Locker();
    bool lock();   //尝试锁上
    bool unlock();  //尝试解锁
    pthread_mutex_t& getlock();  //得到互斥锁
    
private:
    pthread_mutex_t m_mutex;

};


//条件变量类
class Cond{
public:
    Cond();
    ~Cond();
    bool wait(Locker& );  //等待唤醒
    bool timewait(Locker&, const timespec&);  //等待一段时间唤醒
    bool signal();  //唤醒一个或多个
    bool broadcast();  //唤醒所有
    pthread_cond_t& getcond(); //得到条件变量

private:
    pthread_cond_t m_cond;

};


//信号量类
class Sema{
public:
    Sema();
    Sema(int value);
    ~Sema();
    bool wait(); //减少信号量
    bool post(); //增加信号量
 
private:
    sem_t m_sem;

};

#endif