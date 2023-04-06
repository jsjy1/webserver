
#include "thread_synchr.h"

//locker
Locker::Locker()
{
    if(pthread_mutex_init(&m_mutex, nullptr)!=0)
    {
        throw exception();
    }
}

Locker::~Locker()
{
    pthread_mutex_destroy(&m_mutex);
}

bool Locker::lock()
{
    return pthread_mutex_lock(&m_mutex) == 0;
}

bool Locker::unlock()
{
    return pthread_mutex_unlock(&m_mutex) == 0;
}

pthread_mutex_t& Locker::getlock()
{
    return m_mutex;
}

//cond
Cond::Cond()
{
    if(pthread_cond_init(&m_cond, nullptr)!=0)
        throw exception();
}

Cond::~Cond()
{
    pthread_cond_destroy(&m_cond);
}

bool Cond::wait(Locker& locker)
{
    return pthread_cond_wait(&m_cond, &locker.getlock()) == 0;
}

bool Cond::timewait(Locker& locker, const timespec& time)
{
    return pthread_cond_timedwait(&m_cond, &locker.getlock(), &time);
}

bool Cond::signal()
{
    return pthread_cond_signal(&m_cond) == 0;
}

bool Cond::broadcast()
{
    return pthread_cond_broadcast(&m_cond) == 0;
}

pthread_cond_t& Cond::getcond()
{
    return m_cond;
}

//sema
Sema::Sema()
{
    if(sem_init(&m_sem, 0, 0)!=0)
        throw exception();
}

Sema::Sema(int value)
{
    if(sem_init(&m_sem, 0, value)!=0)
        throw exception();
}

Sema::~Sema()
{
    sem_destroy(&m_sem);
}

bool Sema::wait()
{
    return sem_wait(&m_sem) == 0;
}

bool Sema::post()
{
    return sem_post(&m_sem) == 0;
}