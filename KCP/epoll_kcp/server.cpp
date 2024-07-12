#include <sys/epoll.h>
#include <unistd.h>
#include <iostream>
#include <fcntl.h>

#include "ikcp.h"
#include "kcpdef.h"

// 编译:  g++ -o server server.cpp ikcp.c -lpthread

/**
 * setnonblocking – 设置句柄为非阻塞方式
**/
int setnonblocking(int sockfd)
{
   if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0)|O_NONBLOCK) == -1)
   {
     return -1;
   }

   return 0;
}
 
// void msg_process(int fd)
// {
// 	char buffer[1024];
//     struct sockaddr_in client_addr;
//     socklen_t addr_size = sizeof(client_addr);
//     ssize_t ret = recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &addr_size);
//     if (ret > 0) {
//         // 处理接收到的数据
//         std::cout << "Received: " << std::string(buffer, ret) << std::endl;
//     }
// }

int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
	printf("1111111服务器回调e开始:len = %d\n", len);
	kcpObj *send = (kcpObj *)user;

	//发送信息
	int n = sendto(send->sockfd, buf, len, 0, (struct sockaddr *)&send->ClientAddr, sizeof(struct sockaddr_in));
	if (n >= 0)
	{
		//会重复发送，因此牺牲带宽
		printf("服务器回调send: %d bytes, t:%ld\n", n, iclock64()); //24字节的KCP头部
		return n;
	}
	else
	{
		printf("服务器回调error: %d bytes send, error\n", n);
		return -1;
	}
}

//const int port = 12345;
// 初始化KCP客户端
void initKCPClient(kcpObj* kcpObj, const char* server, int port) {

	kcpObj->port = port;
	kcpObj->pkcp = NULL;

	bzero(kcpObj->buff, sizeof(kcpObj->buff));
	char Msg[] = "Server:Hello!"; //与客户机后续交互
	memcpy(kcpObj->buff, Msg, sizeof(Msg));

    ikcpcb *kcp = ikcp_create(0x1, (void *)&kcp); //创建kcp对象把kcp传给kcp的user变量
	ikcp_setmtu(kcp, 1400);
	kcp->output = udp_output;		//设置kcp对象的回调函数
	ikcp_nodelay(kcp, 0, 10, 0, 0); //1, 10, 2, 1
	ikcp_wndsize(kcp, 128, 128);

	kcpObj->pkcp = kcp;

    kcpObj->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	/*设置socket属性，端口可以重用*/
    int opt=SO_REUSEADDR;
    setsockopt(kcpObj->sockfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
 
    setnonblocking(kcpObj->sockfd);

	if (kcpObj->sockfd < 0)
	{
		perror("socket error!");
		exit(1);
	}

	bzero(&kcpObj->addr, sizeof(kcpObj->addr));

	kcpObj->addr.sin_family = AF_INET;
	kcpObj->addr.sin_addr.s_addr = htonl(INADDR_ANY); //INADDR_ANY
	kcpObj->addr.sin_port = htons(kcpObj->port);

	printf("服务器socket: %d  port:%d\n", kcpObj->sockfd, kcpObj->port);

	if (kcpObj->sockfd < 0)
	{
		perror("socket error!");
		exit(1);
	}

	if (bind(kcpObj->sockfd, (struct sockaddr *)&(kcpObj->addr), sizeof(struct sockaddr_in)) < 0)
	{
		perror("bind");
		exit(1);
	}
}

int ikcp_get_fd(kcpObj* kcpObj)
{
    if (kcpObj == nullptr)
    {
        return -1;
    }
    
    return kcpObj->sockfd;
}

// 主函数
int main() {
    kcpObj kcpObj;
    initKCPClient(&kcpObj, "127.0.0.1", 12345);

    // 创建epoll实例
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        std::cerr << "Failed to create epoll instance" << std::endl;
        return -1;
    }

    // 添加kcp到epoll监视列表
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = ikcp_get_fd(&kcpObj);
    if (ev.data.fd <= 0)
    {
        std::cerr << "Failed to ikcp_get_fd" << std::endl;
        return -1;
    }
    
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, ikcp_get_fd(&kcpObj), &ev) == -1) {
        std::cerr << "Failed to add kcp to epoll" << std::endl;
        return -1;
    }

   struct epoll_event events[1024];
    while (1) 
    {
        /* 等待有事件发生 */
        // int nfds = epoll_wait(epoll_fd, events, 1024, -1);
        // if (nfds == -1)
        // {
        //     std::cerr << "Epoll wait failed" << std::endl;
        //     break;
        // }
    
        // /* 处理所有事件 */
        // for (int n = 0; n < nfds; ++n)
        // {
        //     std::cout << "nfds = " << nfds << std::endl;
        //      if (ev.events & EPOLLIN) {
        //         // 处理接收到的数据
        //         char buffer[1024];
        //         int ret = ikcp_input(kcpObj.pkcp, buffer, sizeof(buffer));
        //         if (ret < 0)
        //         {
        //             printf("ikcp_input 11error\n");
        //             continue;
        //         }
        //         int bytes_received = ikcp_recv(kcpObj.pkcp, buffer, sizeof(buffer));
        //         if (bytes_received > 0) {
        //             std::cout << "Received: " << std::string(buffer, bytes_received) << std::endl;
        //         }
        //     }
        // }

        int nfds = epoll_wait(epoll_fd, events, 1024, -1);  
        if (nfds == -1) {  
            std::cerr << "Epoll wait failed" << std::endl;  
            break;  
        }  
        
        /* 处理所有事件 */  
        for (int n = 0; n < nfds; ++n) {  
            //std::cout << "nfds = " << nfds << std::endl;  
            if (events[n].events & EPOLLIN) {  
                // 假设 fd 是已经关联到 epoll 的套接字文件描述符  
                int fd = events[n].data.fd;  
                char buffer[1024];  
                unsigned int len = sizeof(struct sockaddr_in);
                while (1)
                {
                    ssize_t bytesRead = recvfrom(fd, buffer, sizeof(buffer), MSG_DONTWAIT, (struct sockaddr *)&kcpObj.ClientAddr, &len);
                    if (bytesRead > 0) {  
                        int ret = ikcp_input(kcpObj.pkcp, buffer, bytesRead);  
                        if (ret < 0) {  
                            printf("ikcp_input error\n");  
                            break;
                        } 
                        std::cout << "******收到client消息" << kcpObj.ClientAddr.sin_addr.s_addr << ":" << ntohs(kcpObj.ClientAddr.sin_port) << std::endl;
            
                        char recvBuffer[1024];  
                        int bytes_received = ikcp_recv(kcpObj.pkcp, recvBuffer, sizeof(recvBuffer));  
                        if (bytes_received > 0) {  
                            std::cout << "Received: " << std::string(recvBuffer, bytes_received) << std::endl;  
                        }
                        //std::cout << "Received: " << received_message << std::endl;

                        // 发送ACK
                        // const char* ack_message = "ACK";
                        // ikcp_send(kcpObj.pkcp, ack_message, strlen(ack_message));
                        // ikcp_update(kcpObj.pkcp,iclock());
                    } else if (bytesRead == 0) {  
                        // 套接字已关闭  
                        // 处理关闭逻辑
                        break;
                    } else {  
                        // recv 出错  
                        // 处理错误
                        break;
                    }  
                }
                
            }  
        }

    }

    // 清理资源
    //ikcp_destroy(kcp);
    close(epoll_fd);
    ikcp_release(kcpObj.pkcp);
    return 0;
}