#include "epollevent.h"

class TCPClient : public EpollEvent
{
  public:
  TCPClient() : EpollEvent() {}
  void regHandle(std::function<void(Event &, TCPClient &)> handle)
  {
    this->handle = handle;
  }
  void setFdNonblock(int fd)
  {
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
  }
  int addConn(const char *ip, const int port_number)
  {
    std::lock_guard<std::mutex> lk(event_mutex);
    std::cout << "create new conn..." << std::endl;
    int sockfd = create_socket(ip, port_number);
    setFdNonblock(sockfd);

    Event event;
    event.fd = sockfd;
    event.task = std::bind(handle, std::placeholders::_1, std::ref(*this));
    event.event = EPOLLOUT | EPOLLET | EPOLLONESHOT;
    addEvent(event);
    std::cout << "create new conn end " << sockfd << std::endl;
  }
  int run()
  {
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
        if (event.task)
          thread_pool.enqueue(event.task, event);
          // event.task(event);
      }
    }
  }

  private:
  int create_socket(const char *ip, const int port_number)
  {
    struct sockaddr_in client_addr = {0};
    /* 设置ipv4模式 */
    client_addr.sin_family = AF_INET; /* ipv4 */
    /* 设置端口号 */
    client_addr.sin_port = htons(port_number);
    /* 设置主机地址 */
    if (inet_pton(client_addr.sin_family, ip, &client_addr.sin_addr) == -1)
    {
      // err_exit("inet_pton");
    }
    /* 建立socket */
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
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
    /* 设置被动打开 */
    if (connect(sockfd, (sockaddr *)&client_addr, sizeof(client_addr)) == -1)
    {
      // err_exit("listen");
    }
    return sockfd;
  }
  std::mutex sock_mutex;
  std::function<void(Event &, TCPClient &)> handle;
};