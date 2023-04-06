

#include <arpa/inet.h>
#include <errno.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>

#include <iostream>

#include "thread_pool.hpp"
#include "thread_synchr.h"
#include "http_user.h"
#include "timer_stop.h"

using namespace std;

const int max_fd = 65535;       // 最大连接数目
const int max_event = 10000;    // epoll监听最大事件数
const int time_out = 10;         // 定时清理时间
const int time_detect = 5;         // 定时检测时间

int s_pipe[2];    //管道通信


void addsig(int sig, void (*handler)(int) )
{
    struct sigaction sigact;
    sigact.sa_flags = 0;
    sigact.sa_handler = handler;
    sigfillset(&sigact.sa_mask); // TODO: tws是fill的，表示不接受其他信号??  测试sigrmpty也可以
    int res = sigaction(sig, &sigact, nullptr);
    if(res == -1) {perror("sigaction");exit(-1);}
}

void alarm_handler(int sig)
{
    int res = write(s_pipe[1], (char*)&sig, 1); //传出信号
    if(res==-1) {perror("write pipe");exit(-1);}
}


int main(int arg, char* argv[] )
{

    if(arg==1)
    {
        cout<<"请输入: "<<argv[0]<<" + 端口号."<<endl;
        exit(-1);
    }

    //创建线程池
    Thread_pool<Http_user>* thd_pol = new Thread_pool<Http_user>();


    //创建用户类
    Http_user* user = new Http_user[max_fd];  //设置最大连接用户数


    //创建监听套接字
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if(lfd == -1) {perror("socket"); exit(-1);}


    //设置端口复用
    int use = 1;
    int opt_res = setsockopt(lfd, SOL_SOCKET, SO_REUSEPORT, &use, sizeof(use));
    if(opt_res == -1) {perror("setsockopt"); exit(-1);}

    //绑定ip端口
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = 0;
    addr.sin_port = htons( stoi(argv[1]) );
    opt_res = bind(lfd, (sockaddr*)&addr, sizeof(addr));
    if(opt_res == -1) {perror("bind"); exit(-1);}

    //监听
    opt_res = listen(lfd, 5);
    if(opt_res == -1) {perror("listen"); exit(-1);}


    //创建管道 捕捉定时信号 创建定时清理类 
    opt_res = pipe(s_pipe);
    if(opt_res == -1) {perror("pipe"); exit(-1);}

    addsig(SIGALRM, alarm_handler);

    Timer_stop_list user_time_list(time_out);

    itimerval ite; //定时器参数
    ite.it_interval.tv_sec = time_detect;
    ite.it_interval.tv_usec = 0;
    ite.it_value.tv_sec = time_detect;
    ite.it_value.tv_usec = 0;
    opt_res = setitimer(ITIMER_REAL, &ite, nullptr);
    if(opt_res == -1) {perror("setitimer"); exit(-1);}


    //创建epoll对象
    epoll_event* events = new epoll_event[max_event];
    int efd = epoll_create(100);
    if(efd == -1) {perror("epoll_create"); exit(-1);}

    //管道加入epoll
    epo_add(efd, s_pipe[0], false, false);

    //将监听套接字加入epoll实例 TODO: lfd默认为lt模式
    epo_add(efd, lfd, false, false); 
    
    //设置用户的epoll实例
    Http_user::setefd(efd);

    cout<<"正在监听中....\n"<<endl;

    while(1)
    {
        bool timeout = false;   //当前是否到达定时时间

        // cout<<"等待连接..."<<endl;
        int len = epoll_wait(efd, events, max_event, -1);  // 阻塞  首先会被alarm中断，再返回到这
        // cout<<len<<endl;
        if(len == -1 && errno!=EINTR ) {perror("epoll_wait"); break;} //不是被信号中断

        for(int i=0;i<len;++i)
        {
            int sockfd = events[i].data.fd;
            if(sockfd == lfd)  //有新连接请求
            {
                sockaddr_in client_addr;
                socklen_t len_client_addr = sizeof(client_addr);
                int cfd = accept(lfd, (sockaddr*)&client_addr, &len_client_addr);
                if( cfd < 0 ) 
                {
                    perror("accept");
                    exit(-1);  // TODO: 直接退出程序的 以后可以考虑改一改
                } 
                if( Http_user::cur_num >= max_fd )
                {
                    cout<<"当前用户数已满，请稍后再试\n"<<endl;
                    close(cfd);
                    continue;
                }
                /*假设开始创建数组，则当前是setfd，但是空间占用很大；
                假设开始使用vector 后面push_back，可能会慢一点，主要是关闭连接时怎么
                删除该用户类
                */
                //加入到用户 epoll监听 
                user[cfd].setfd(cfd, client_addr);
                user_time_list.add(&user[cfd]);
                // cout<<"新用户！----------------------------"<<endl;
            }
            else if(sockfd==s_pipe[0])
            {
                if(events[i].events & ( EPOLLERR | EPOLLHUP | EPOLLRDHUP ))
                {
                    cout<<"pipe报错!"<<endl;
                    exit(-1);
                }
                if(events[i].events & EPOLLIN)  //TODO:
                {
                    char tmp[1024] ;
                    read(sockfd, tmp, sizeof(tmp));  //直接丢弃 
                    timeout = true;
                }
            }
            else if(events[i].events & ( EPOLLERR | EPOLLHUP | EPOLLRDHUP ))
            {
                //客户端关闭连接
                // user[sockfd].rmfd();
                user_time_list.del(&user[sockfd]); 
            }
            else if(events[i].events & EPOLLIN)
            {
                if(user[sockfd].read_())
                {
                    user_time_list.adjust(&user[sockfd]); //不管有没有成功都更新时间
                    thd_pol->append(&user[sockfd]);
                }
                else 
                    user_time_list.del(&user[sockfd]); 
                
            }
            else if(events[i].events & EPOLLOUT)
            {
                if(!user[sockfd].write_())
                    user_time_list.del(&user[sockfd]); 
                else 
                    user_time_list.adjust(&user[sockfd]);  //不管有没有成功都更新时间
            }
        }

        if(timeout)
        {
            user_time_list.clean();
            timeout = false;
        }
    }


    //释放资源
    close(s_pipe[0]);
    close(s_pipe[1]);
    close(efd);
    delete[] events;
    delete[] user;
    delete[] thd_pol;
    close(lfd);
    return 0;
}