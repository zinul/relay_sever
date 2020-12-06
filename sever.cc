#include <iostream>

#include "PackageHead.h"
#include "TCPClient.h"
#include "TCPServer.h"
// ThreadPool thread_pool(8);
// std::mutex v_mutex;
using std::placeholders::_1;

void transmit_read(Event &event, TCPServer &tran, int des_fd,
                   unsigned int remain_bytes);
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
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1)
  {
    // err_exit("setsockopt");
  }
  /* 设置被动打开 */
  if (connect(sockfd, (sockaddr *)&client_addr, sizeof(client_addr)) == -1)
  {
    printf("listen\n");
  }
  std::cout << "create conn " << sockfd << std::endl;
  return sockfd;
}
void close_session(int src_fd, int des_fd)
{
  shutdown(src_fd, SHUT_RDWR);
  close(src_fd);
  shutdown(des_fd, SHUT_RDWR);
  close(des_fd);
}
void transmit_write(Event &event, TCPServer &tran_read, TCPClient &tran_write,
                    int des_fd, unsigned int remain_bytes)
{
  // std::cout << event.fd << "write: " << ((char *)event.buf_ptr->block)[0]
  //           << std::endl;

  int src_fd = event.fd;
  if (event.size <= 0 || write(des_fd, event.buf_ptr->block, event.size) <= 0 ||
      (remain_bytes -= event.size) <= 0)
  {
    tran_read.delEvent(event);
    close_session(src_fd, des_fd);
    return;
  }
  printf("%dwrite: %d\n", event.fd, ((char *)event.buf_ptr->block)[0]);
  // std::cout << "writesize " << event.size << std::endl;

  // remain_bytes -= event.size;

  // auto a=(char *)event.buf_ptr->block;
  // for(int i=0;i<32;i++)
  //   printf("%c",*a++);
  // printf("\n\n");
  event.task = std::bind(transmit_read, _1, std::ref(tran_read),
                         std::ref(tran_write), des_fd, remain_bytes);
  event.event = EPOLLIN | EPOLLET | EPOLLONESHOT;
  tran_read.modEvent(event, event);
}
void transmit_read(Event &event, TCPServer &tran_read, TCPClient &tran_write,
                   int des_fd, unsigned int remain_bytes)
{
  int src_fd = event.fd;

  if (remain_bytes > BUFSIZE)
  {
    event.size = read(src_fd, event.buf_ptr->block, BUFSIZE);
  }
  else
  {
    event.size = read(src_fd, event.buf_ptr->block, remain_bytes);
  }
  // std::cout << event.fd << "recv: " << ((char *)event.buf_ptr->block)[0]
  //           << std::endl;
  printf("%drecv: %d\n", event.fd, ((char *)event.buf_ptr->block)[0]);

  // std::cout << "remain" << remain_bytes << std::endl;
  // std::cout << "readsize " << event.size << std::endl;

  if (event.size > 0)
  {
    event.task = std::bind(transmit_write, _1, std::ref(tran_read),
                           std::ref(tran_write), des_fd, remain_bytes);
    event.event = EPOLLOUT | EPOLLET | EPOLLONESHOT;
    tran_write.modEvent(event, event);
  }
  else if ((event.size < 0 &&
            (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)))
  {
    tran_read.modEvent(event, event);
  }
  else if (event.size <= 0)
  {
    tran_read.delEvent(event);
    close_session(src_fd, des_fd);
  }
}

void handle(Event &event, TCPServer &tran_read, TCPClient &tran_write)
{
  int eventfd = event.fd;
  {
    // std::lock_guard<std::mutex> lk(v_mutex);
    if ((event.size =
             read(eventfd, event.buf_ptr->block, sizeof(PackageHead))) < 0)
    {
      tran_read.delEvent(event);
      shutdown(eventfd, SHUT_RDWR);
      close(eventfd);
      return;
    }
  }
  // std::cout << "head " << event.size << std::endl;

  PackageHead *pack_head = (PackageHead *)event.buf_ptr->block;
  // auto a = pack_head->des_add;
  // for (int i = 0; i < 16; i++)
  //   printf("%c", *a++);
  // printf("\n\n");

  // std::cout << "handle" << pack_head->data_size << std::endl;
  char tmp[16];
  if (inet_pton(AF_INET, pack_head->des_add, tmp) <= 0)
  {
    perror("error ip");
    tran_read.delEvent(event);
    shutdown(eventfd, SHUT_RDWR);
    close(eventfd);
    return;
  }
  // int des_fd = create_socket(pack_head->des_add, tran_read.getPort() + 1);
  tran_write.addConn(pack_head->des_add, tran_read.getPort() + 1);

  Event trans_event;
  trans_event.event = EPOLLIN | EPOLLET | EPOLLONESHOT;
  trans_event.fd = eventfd;
  trans_event.task =
      std::bind(transmit_read, std::placeholders::_1, std::ref(tran_write),
                std::ref(tran_read), des_fd, pack_head->data_size);
  tran_read.modEvent(trans_event, event);

  tran_write.addEvent(trans_event);
  return;
}

int main(int argc, char *argv[])
{
  TCPServer tran_read(atoi(argv[1]));
  tran_read.regHandle(handle);
  std::thread tran_recv(tran_read.run());
  TCPClient tran_write;
  tran_write.regHandle(transmit_write) std::thread tran_send(tran_write.run());
}
