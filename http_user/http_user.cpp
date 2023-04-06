
#include "http_user.h"

//初始化为无效
int Http_user::epo_fd = -1;
int Http_user::cur_num = 0;

//初始化 不包括fd与sockaddr
void Http_user::init()  //初始化在构造时与写完时或者移除时
{
    cur_read_len = 0;
    cur_write_len = 0;
    add_write_len = 0;
    file_addr = nullptr;
    send_addr = nullptr;
    send_addr_len = 1;

    bzero(rbuf, max_read_len);
    bzero(wbuf, max_write_len);
    bzero(&file_info, sizeof(file_info));

    req_header.clear();

    m_check_state = CHECK_STATE_REQUESTLINE;
    m_line_state = LINE_OK;
    m_method = GET;
    m_url = "";
    m_req_data = "";
    cur_line = "";
    read_buf = "";
}

Http_user::Http_user()
{
    fd = -1;
    bzero(&addr, sizeof(addr));
    last_time = time(nullptr); 
    m_link = false;
    // req_header["key"] = "234";
    init();
}

Http_user::~Http_user()
{

    if(fd!=-1)
    {
        epo_del(epo_fd, fd);
        close(fd);
        delete[] send_addr;
    }
}

bool Http_user::read_()
{
    if(this->cur_read_len>=max_read_len) 
    {
        cout<<"读缓冲区已满!\n"<<endl;
        return false;
    }

    int len=0;
    while(1) //非阻塞读
    { 
        len = read(fd, rbuf + this->cur_read_len, max_read_len - this->cur_read_len);
        if(len==-1)
        {
            if(errno!=EAGAIN && errno!=EWOULDBLOCK)
                perror("read");
            else
            {
                // cout<<"读取完毕！\n"<<endl;
                break;
            }
        }
        else if(len == 0) 
        {   
            cout<<"客户端已关闭...\n"<<endl;
            return false;
        }
        this->cur_read_len += len;
    }

    // cout<<rbuf<<endl;
    return true;
}

//分散写
bool Http_user::write_()
{
    if(add_write_len==0)
    {
        epo_mod(epo_fd, fd, EPOLLIN);  //若没有可写的数据则启动读

        //释放内存映射
        if(file_addr)
        {
            int res = munmap(file_addr, file_info.st_size);
            if(res==-1){perror("munmap"); exit(-1);}
        }

        init();  
        return true;
    }
    
    cur_write_len = 0; 
    while(1)
    {
        int len = writev(fd, send_addr, send_addr_len);
        if(len == -1) 
        {
            if(errno==EAGAIN||errno==EWOULDBLOCK)  //写缓冲已满才会报这个
            {
                // cout<<"1写完"<<cur_write_len<<endl;

                continue;  //TODO: 这里用continue还是return(return需要重新注册事件)
            }
            perror("write"); 
            return false;
        }
        cur_write_len += len;

        //每次没写完再写的话需要改变写的起始位置
        if(cur_read_len >= send_addr[0].iov_len)   
        {
            send_addr[1].iov_base += cur_read_len - send_addr[0].iov_len;
            send_addr[1].iov_len -= cur_read_len - send_addr[0].iov_len;
            send_addr[0].iov_len = 0;
        }
        else
        {
            send_addr[0].iov_base += cur_read_len;
            send_addr[0].iov_len -= cur_read_len;
        }

        if(cur_write_len>=add_write_len) 
        {
            epo_mod(epo_fd, fd, EPOLLIN); //写完则启动读
            //释放内存映射
            int res = munmap(file_addr, file_info.st_size);
            if(res==-1){perror("munmap"); exit(-1);}
            // 写完则初始化一下
            init();  
            return m_link;
        }
        
    }   

    return true;
}

   
void Http_user::setfd(int fd, sockaddr_in addr)
{
    //设置成员属性
    cur_num++;
    this->fd = fd;
    this->addr = addr;
    this->last_time = time(nullptr);

    //设为非阻塞的
    int flag = fcntl(this->fd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(this->fd, F_SETFL, flag);

    //加入epoll监听
    epo_add(epo_fd, fd);
}

int Http_user::getfd()
{
    return fd;
}

void Http_user::rmfd()
{
    if(this->fd!=-1)
    {
        cur_num--;
        this->last_time = -1;
        m_link = false;

        //关闭套接字 移除监听 描述符置为无效
        epo_del(epo_fd, this->fd);
        close(this->fd);

        delete[] send_addr; //释放堆区数据

        fd = -1;
        bzero(&addr, sizeof(addr));
        init();
    }
}

void Http_user::setefd(int efd)
{
    epo_fd = efd;
}


void Http_user::process()
{
    // printf("接受长度: %ld ----处理中，请稍后\n", strlen(rbuf));
    
    //解析
    HTTP_CODE ret = parse(); 
    if(ret==BAD_REQUEST) 
    {
        cout<<"解析出错!"<<endl;
        epo_mod(epo_fd, fd, EPOLLIN);
        return ;
    }

    //生成响应  不管什么请求都发送回报
    int res = response();
    if(!res) 
    {
        cout<<"生成响应出错!"<<endl;
    } //TODO:  
    epo_mod(epo_fd, fd, EPOLLOUT); //当写完数据则再次启动写事件
}


Http_user::LINE_STATUS Http_user::parse_line()
{
    //查找\r\n  删除多余\r\n
    int pos = read_buf.find("\r\n");
    if(pos==-1) 
        return LINE_BAD;

    cur_line = read_buf.substr(0, pos);
    read_buf = read_buf.substr(pos + 2); //去除当前/r/n

    if(read_buf.find("\r\n")==0)  //请求头解析完毕
    {
        read_buf = read_buf.substr(2); //去除尾上的/r/n
        m_check_state = CHECK_STATE_CONTENT;
    }
    
    return LINE_OK;
}

Http_user::HTTP_CODE Http_user::parse_req_line()
{
    //请求方法
    int pos = cur_line.find(" ");
    if(pos==-1) return BAD_REQUEST;

    string method = cur_line.substr(0, pos);
    if(method == "GET" )
    {
        m_method = GET;
    } 
    else if(method == "POST")
    {
        m_method = POST;
    } 
    else
    {
        cout<<"暂未支持该种请求"<<endl;
        return BAD_REQUEST;
    }

    for(;pos<cur_line.size();++pos)     //去除多余空格
        if(cur_line[pos]!=' ') break;
    cur_line = cur_line.substr(pos);

    //请求url  TODO: 为判断url合法性什么的
    pos = cur_line.find(" ");
    if(pos==-1) return BAD_REQUEST;
    m_url = cur_line.substr(0, pos);

    for(;pos<cur_line.size();++pos)     //去除多余空格
        if(cur_line[pos]!=' ') break;
    cur_line = cur_line.substr(pos);

    //请求协议 ......


    //状态转移 下一步检查头
    m_check_state = CHECK_STATE_HEADER;

    return NO_REQUEST;
}

Http_user::HTTP_CODE Http_user::parse_req_head() //其实请求严格按照格式的话也不用这么复杂...
{
    int pos = 0;
    //去除开头\r\n与空格
    for(;pos<cur_line.size();++pos)  
        if(cur_line[pos]!='\r'&&cur_line[pos]!='\n'&&cur_line[pos]!=' ')
            break;
    cur_line = cur_line.substr(pos);

    //查找key
    pos = cur_line.find(":");  //找到第一个:
    if(pos==-1) return BAD_REQUEST;
    string key = cur_line.substr(0,pos);
    cur_line = cur_line.substr(pos+1);

    //去除多余空格 求得value
    pos = 0;
    for(;pos<cur_line.size();++pos)  
        if(cur_line[pos]!='\r'&&cur_line[pos]!='\n'&&cur_line[pos]!=' ')
            break;
    string value = cur_line.substr(pos);

    req_header[key] = value;

    return NO_REQUEST;
}

Http_user::HTTP_CODE Http_user::parse_req_content()
{
    
    if(read_buf.size()==0) 
    {
        return NO_REQUEST;  //没有请求数据
    }

    m_req_data = read_buf;
    return NO_REQUEST;
}

Http_user::HTTP_CODE Http_user::parse()  //TODO: 目前都是没有请求 直接读取url的数据
{
    read_buf = string(rbuf);

    while( m_check_state==CHECK_STATE_CONTENT || (m_line_state = parse_line())==LINE_OK)
    {
        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
        {
            HTTP_CODE res = parse_req_line();
            if(res==BAD_REQUEST) 
            {
                cout<<"parse_req_line"<<endl;
                return BAD_REQUEST;
            }
            break;
        }
        case CHECK_STATE_HEADER:
        {
            HTTP_CODE res = parse_req_head();
            if(res==BAD_REQUEST) 
            {
                cout<<"parse_req_head"<<endl;
                return BAD_REQUEST;
            }
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            HTTP_CODE res = parse_req_content();
            if(res==BAD_REQUEST) 
            {
                cout<<"parse_req_content"<<endl;
                return BAD_REQUEST;
            }
            return NO_REQUEST;  //最后应该会从这里出去
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }

    cout<<"LINE_BAD"<<endl;
    return BAD_REQUEST;
}


int Http_user::response()
{
    cout<<"url:"<<m_url<<endl;

    //获取当前工作目录
    char filename[200];  //最大路径字符长200
    getcwd(filename, sizeof(filename));
    string file_name(filename);
    int file_pos = file_name.find("webserver");
    string root_name = file_name.substr(0,file_pos+9).append("/resource");  //所有资源存在resource下
    file_name = root_name + m_url;

    sprintf(filename, "%s", file_name.c_str());

    //获取文件信息
    int res = stat(filename, &file_info);

    if(file_name.back()=='/'||res==-1)  //没有文件请求||找不到文件
    {
        // cout<<"stat:未检测到该文件"<<endl;
        file_name = root_name + "/1.html"; // 默认界面
        sprintf(filename, "%s", file_name.c_str());
        res = stat(filename, &file_info);
    }
    

/*添加响应行*/
    
    string s1 = "HTTP/1.1 200 ok\r\n";
    // string s1 = "HTTP/1.0 200 ok\r\n";

/*添加响应头*/

    if(req_header["Connection"]=="keep-alive")  m_link  = true;
    s1 = s1.append("Connection: ").append(m_link?"keep-alive":"close").append("\r\n");



    s1 = s1.append("Content-Length: ").append(to_string(file_info.st_size)).append("\r\n");

    int pos = file_name.find(".");
    string suffix = file_name.substr(pos+1);
    string type;
    if(suffix=="html") type="text/html";
    else type = "text/plain";

    if(suffix!="css")  //css文件放行
        s1 = s1.append("Content-Type: ").append(type).append(";charset=utf-8\r\n");  //设置中文编码


    s1 = s1.append("\r\n");

    //第一个地址空间
    sprintf(wbuf, "%s", s1.c_str());
    add_write_len = s1.size();  //TODO: 需不需要+1  好像不需要

/*添加响应体*/

    //打开待发送文件
    int html_fd = open(filename, O_RDONLY);
    if(html_fd==-1) {perror("open mmap"); exit(-1);}

    //以读的方式创建内存映射 第二个地址空间
    file_addr = (char*)mmap(nullptr, file_info.st_size, PROT_READ, MAP_PRIVATE, html_fd, 0);
    if(!file_addr) 
    {
        perror("mmap");
        exit(-1);
    }
    close(html_fd);

    add_write_len += file_info.st_size;

/*确定发送数据*/

    //创建地址段 TODO: 默认为两个地址段
    send_addr_len = 2; //待发送地址段
    send_addr = new iovec[2];
    send_addr[0].iov_base = wbuf;
    send_addr[0].iov_len = strlen(wbuf);
    send_addr[1].iov_base = file_addr;
    send_addr[1].iov_len = file_info.st_size;
 
    return 1;
}

/////////////////////epoll//////////////////////////

void epo_add(int efd, int fd, bool one_shot, bool is_et)
{
    epoll_event eve;
    eve.data.fd = fd;
    eve.events = EPOLLIN| EPOLLRDHUP;  // 加入时便是监听输入状态  不用加入写事件 不然开始就会写
    if(one_shot) eve.events |= EPOLLONESHOT;
    if(is_et) eve.events |= EPOLLET;

    int res = epoll_ctl(efd, EPOLL_CTL_ADD, fd, &eve);
    if(res == -1) {perror("epoll_add"); exit(-1);}
}

//删除
void epo_del(int efd, int fd)
{
    int res = epoll_ctl(efd, EPOLL_CTL_DEL, fd, nullptr);
    if(res == -1) {perror("epoll_del"); exit(-1);}
}

//改变事件
void epo_mod(int efd, int fd, int eve, bool et)
{
    epoll_event event;
    event.data.fd = fd;
    //为什么没有et和in?? 这两个不是默认的,利用参数传进来
    event.events = eve  | EPOLLONESHOT | EPOLLRDHUP; 
    if(et) event.events |= EPOLLET;

    int res = epoll_ctl(efd, EPOLL_CTL_MOD, fd, &event);
    if(res == -1) {perror("epoll_mod"); exit(-1);}
}
