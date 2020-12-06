#include "epollevent.h"

class TCPServer : public EpollEvent
{
  public:
  TCPServer(const int port_num) : EpollEvent(), serv_port(port_num)
  {
    int epollFd = epoll_create(10);
    if (epollFd <= 0)
    {
      perror("create_create error:");
    }
    this->epollFd = epollFd;
    this->sockfd = create_socket(port_num);
    std::cout << "create socket " << sockfd << std::endl;
  }
  void regHandle(std::function<void(Event &, TCPServer &)> handle)
  {
    this->handle = handle;
  }
  int &getPort() { return serv_port; }
  int run()
  {
    Event listen;
    listen.fd = sockfd;
    listen.event = EPOLLIN | EPOLLET;
    listen.task = std::bind(&TCPServer::create_conn, this);
    addEvent(listen);
    struct epoll_event epoll_events[MAX_EPOLL_EVENTS];

    // cout << "epoll_wait before" << endl;
    while (1)
    {
      int nEvents = epoll_wait(epollFd, epoll_events, MAX_EPOLL_EVENTS, -1);
      if (nEvents <= 0)
      {
        // std::cout<<"00000"<<std::endl;
        perror("epoll_wait error:");
        // return -1;
      }
      // cout << "epoll_wait after nEvent" << endl;
      for (int i = 0; i < nEvents; i++)
      {
        int fd = epoll_events[i].data.fd;
        Event event = this->events[fd];
        if (event.fd == sockfd)
        {
          event.task(event);
        }
        else
        {
          thread_pool.enqueue(event.task, event);
          // event.task(event);
        }
      }
    }
  }
  private:
  int create_socket(const int port_number)
  {
    struct sockaddr_in server_addr = {0};
    /* 设置ipv4模式 */
    server_addr.sin_family = AF_INET; /* ipv4 */
    /* 设置端口号 */
    server_addr.sin_port = htons(port_number);
    /* 设置主机地址 */
    if (inet_pton(server_addr.sin_family, "localhost", &server_addr.sin_addr) ==
        -1)
    {
      // err_exit("inet_pton");
    }
    /* 建立socket */
    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
      // err_exit("socket");
    }
    /* 设置复用模式 */
    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) ==
        -1)
    {
      // err_exit("setsockopt");
    }
    /* 绑定端口 */
    if (bind(sockfd, (sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
      // err_exit("bind");
    }
    /* 设置被动打开 */
    if (listen(sockfd, 5) == -1)
    {
      // err_exit("listen");
    }
    return sockfd;
  }
  void setFdNonblock(int fd)
  {
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
  }
  void create_conn()
  {
    std::cout << "accept new client...\n";
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int connfd =
        accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
    setFdNonblock(connfd);
    // std::cout << "size:" << buf_pool.size() << std::endl;

    Event event;

    event.fd = connfd;
    event.event = EPOLLIN | EPOLLET | EPOLLONESHOT;
    event.task = std::bind(handle, std::placeholders::_1, std::ref(*this));
    addEvent(event);
    // std::cout << event.buf_ptr << std::endl;

    // handle_read(event);
    std::cout << "accept new client finish\n";
    std::cout << connfd << "\n";
  }
  int serv_port;
  int sockfd;
  std::function<void(Event &, TCPServer &)> handle;
};
