#ifndef _EPOLL_H_
#define _EPOLL_H_
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <memory>

#include "mempool.h"
#include "threadpool.h"
const int MAX_EPOLL_EVENTS = 500;
const size_t BUFSIZE = 4096;

struct Event
{
  Event(size_t buf_size = 4096) : size(0), buf_ptr(nullptr) {}
  size_t size;
  int fd;
  uint32_t event;
  std::function<void(Event &)> task = NULL;
  std::shared_ptr<Buf> buf_ptr;

  int free_buf(std::shared_ptr<Buf> buf, MemPool &mem_pool)
  {
    if (buf_ptr)
    {
      mem_pool.deallocate(buf);
      return 0;
    }
    return -1;
  }
  int alloc_buf(MemPool &mem_pool)
  {
    if (!buf_ptr)
    {
      buf_ptr = mem_pool.allocate();
      return 0;
    }
    return -1;
  }
};
class EpollEvent
{
  public:
  EpollEvent() : mem_pool(), thread_pool(4)
  {
    int epollFd = epoll_create(10);
    if (epollFd <= 0)
    {
      perror("create_create error:");
    }
    this->epollFd = epollFd;
  }

  int addEvent(Event &event)
  {
    /* add event to this->events */
    event.alloc_buf(mem_pool);
    {
      // std::lock_guard<std::mutex> lk(event_mutex);
      this->events[event.fd] = event;
    }

    struct epoll_event epollEvent;

    epollEvent.data.fd = event.fd;
    epollEvent.events = event.event;
    int retCode =
        epoll_ctl(this->epollFd, EPOLL_CTL_ADD, event.fd, &epollEvent);
    if (retCode < 0)
    {
      perror("epoll_ctl error:");
      return retCode;
    }

    return 0;
  }
  int delEvent(Event &event)
  {
    int retCode = epoll_ctl(this->epollFd, EPOLL_CTL_DEL, event.fd, nullptr);

    std::cout<<"free fd:"<<event.fd<<std::endl;
    if (retCode < 0)
    {
      perror("epoll_ctl error:");
      return retCode;
    }

    {
      std::lock_guard<std::mutex> lk(event_mutex);
      event.free_buf(event.buf_ptr, mem_pool);
      this->events.erase(event.fd);
    }
    return 0;
  }
  int modEvent(Event &event, Event &old_event)
  {
    event.buf_ptr=old_event.buf_ptr;
    {
      std::lock_guard<std::mutex> lk(event_mutex);
      this->events[event.fd] = event;
    }
    // this->events[event.fd] = event;

    struct epoll_event epollEvent;

    epollEvent.data.fd = event.fd;
    epollEvent.events = event.event;
    int retCode =
        epoll_ctl(this->epollFd, EPOLL_CTL_MOD, event.fd, &epollEvent);
    if (retCode < 0)
    {
      perror("epoll_ctl error:");
      // std::cout << event.fd << std::endl;
      return retCode;
    }
    return 0;
  }
  int dispatcher()
  {
    struct epoll_event epollEvents[MAX_EPOLL_EVENTS];

    // cout << "epoll_wait before" << endl;
    int nEvents = epoll_wait(epollFd, epollEvents, MAX_EPOLL_EVENTS, -1);
    if (nEvents <= 0)
    {
      perror("epoll_wait error:");
      return -1;
    }
    // cout << "epoll_wait after nEvent" << endl;
    for (int i = 0; i < nEvents; i++)
    {
      int fd = epollEvents[i].data.fd;
      Event event = this->events[fd];
      if (event.task)
      {
        thread_pool.enqueue(event.task, event);
        // event.task(event);
      }
    }
    return 0;
  }

  protected:
  int epollFd;
  MemPool mem_pool;

  ThreadPool thread_pool;
  std::mutex event_mutex;
  // Event event;
  std::map<int, Event> events;
};
#endif
