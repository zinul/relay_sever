#include "PackageHead.h"
#include "TCPServer.h"
#include "TcpClient.h"

// const int SERV_PORT = 40001;
MemPool mem_pool;
char str[BUFSIZE];
using std::placeholders::_1;
using std::placeholders::_2;
void sendPack(Event &event, TCPClient &client, int pack_num, int des_fd)
{
  if (pack_num <= 0)
  {
    client.delEvent(event);
    shutdown(des_fd, SHUT_RDWR);
    close(des_fd);
    return;
  }
  // memset(str, pack_num, sizeof(str));
  int ret = write(des_fd, str, sizeof(str));
  printf("%dwrite: %d %d\n", event.fd, pack_num, ret);
  // std::cout << event.fd << "write: " << (char)(pack_num + '0') << std::endl;
  // if (ret == 0)
  // {
  //   client.delEvent(event);
  //   shutdown(des_fd, SHUT_RDWR);
  //   close(des_fd);
  //   return;
  // }
  if (ret < 0 && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN))
  {
    client.modEvent(event, event);
  }
  else if (ret > 0)
  {
    event.task =
        std::bind(sendPack, _1, std::ref(client), pack_num - 1, des_fd);
    client.modEvent(event, event);
  }
  else
  {
    client.delEvent(event);
    shutdown(des_fd, SHUT_RDWR);
    close(des_fd);
    return;
  }
}
void sendPackHead(Event &event, TCPClient &client, int pack_num,
                  const char *des_ip)
{
  int connectfd = event.fd;
  PackageHead pack_head(des_ip, (unsigned int)4096 * pack_num);
  // std::cout<<pack_head.des_add;
  event.size = write(connectfd, (char *)&pack_head, sizeof(pack_head));

  // memcpy(event.buf_ptr.get(), str, sizeof(str));
  event.task = std::bind(sendPack, _1, std::ref(client), pack_num, connectfd);
  client.modEvent(event, event);
  // char tmp[write_buf->size];
  // memcpy(tmp,read_buf,write_buf->size);
  // std::cout << tmp << std::endl;
}

void recvPack(Event &event, TCPServer &recv_client)
{
  int ret = read(event.fd, event.buf_ptr->block, BUFSIZE);
  if (ret < 0 && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN) ||
      ret > 0)
  {
    // event.event = EPOLLIN | EPOLLET | EPOLLONESHOT;
    recv_client.modEvent(event, event);
  }
  else
  {
    recv_client.delEvent(event);
    shutdown(event.fd, SHUT_RDWR);
    close(event.fd);
    return;
  }
  // std::cout << event.fd << "recv: " << ((char *)event.buf_ptr->block)[0]
  //           << std::endl;
  printf("%drecv: %d\n",event.fd,((char *)event.buf_ptr->block)[0]);

  return;
  // recv_client.modEvent(event, event);
}

int main(int argc, char *argv[])
{
  pid_t pid=fork();

  if (pid == 0)
  {
    sleep(1);
    TCPClient send_client;
    send_client.regHandle(
        std::bind(sendPackHead, _1, _2, atoi(argv[5]), argv[3]));
    for (int i = 0; i < atoi(argv[4]); i++)
    {
      send_client.addConn(argv[1], atoi(argv[2]));
    }
    send_client.run();
  }
  else
  {
    TCPServer recv_client(atoi(argv[2]) + 1);
    recv_client.regHandle(recvPack);
    recv_client.run();
  }

  // std::cout<<"port"<<sockfd<<std::endl;
  // thread_pool.enqueue(deal, sockfd, atoi(argv[3]));
}