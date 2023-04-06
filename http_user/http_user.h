#ifndef HTTP_USER_H
#define HTTP_USER_H

#include <sys/epoll.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <cstring>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <sys/time.h>

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
using namespace std;


//包装套接字
class Http_user{

private:
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    enum HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN       //TODO: 暂时没用到
    };


public:
   
    Http_user();      // 初始化
    ~Http_user();           // 关闭套接字 从epoll中退出
    bool read_();           // 非阻塞 一次性读出所有数据
    bool write_();          // 分散写
    void process();         // 解析http请求 生成响应
    void setfd(int, sockaddr_in);    // 加入fd，设为非阻塞的，加入epoll监听
    int getfd();            
    void rmfd();            // 移除用户 关闭连接
    static void setefd(int);   // 设置epoll实例

public:

    static int cur_num;     // 当前总用户数
    sockaddr_in addr;       // 当前用户的ip与port  默认为ipv4
    
    time_t last_time;  // 记录本用户上一次io操作的时间 


private:

    void init();     //返回原始状态

    ////解析http与生成响应 现在默认为get////
    LINE_STATUS parse_line();           //解析出一行
    HTTP_CODE parse_req_line();         //解析请求行
    HTTP_CODE parse_req_head();         //解析请求头 键值对
    HTTP_CODE parse_req_content();      //解析请求体
    HTTP_CODE parse();            //状态机实现
    int response();

private:

    static int epo_fd;      // 所有用户由同一个epol实例监听
    int fd;                 // 套接字描述符

    static const int max_read_len = 2048;
    static const int max_write_len = 1024; //最长 响应行+头
    int cur_read_len;           // 当前读进rbuf的长度
    int cur_write_len;          // 当前wbuf写出长度
    int add_write_len;          // 写进wbuf的长度


    char rbuf[max_read_len];                // 读缓冲区
    char wbuf[max_write_len];         // 写缓冲区


    char* file_addr;            // 待发送的内存映射的首地址
    struct stat file_info;      // 待发送内存映射空间的信息

    struct iovec* send_addr;    // 待发送首地址地址 写响应时确定长度
    int send_addr_len;          // 待发送的不连续地址数目

    //解析相关---------------------------------------
    CHECK_STATE m_check_state;  // 下一步解析状态
    LINE_STATUS m_line_state;   // 行状态

    METHOD m_method;    // 当前请求方法 
    string m_url;
    string m_req_data;  // 请求数据
    unordered_map<string,string> req_header;    // 解析请求头
    bool m_link;        //是否保持连接

    string read_buf;    // 未解析的字符串
    string cur_line;    // 当前待解析的一行数据

};
 
void epo_add(int efd, int fd, bool one_shot = true, bool is_et = true);
void epo_del(int efd, int fd);
void epo_mod(int efd, int fd, int eve, bool et = true);  //修改文件描述符 主要是重置epolloneshot



#endif