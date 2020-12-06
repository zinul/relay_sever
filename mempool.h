#include <memory>
#include <mutex>
struct Buf
{
  Buf(size_t size):block(new char[size]){}
  Buf(void *p):block(p){}
  void *block;
};
class MemPool
{
  public:
  MemPool(size_t buf_size = 4096) : buf_size(buf_size), free_block_head(nullptr)
  {
  }
  std::shared_ptr<Buf> allocate()
  {
    std::lock_guard<std::mutex> lk(mtx);

    if (!free_block_head)
    {
      std::shared_ptr<FreeBlock> new_block =
          std::make_shared<FreeBlock>(buf_size);
      new_block->next = nullptr;

      free_block_head = new_block;
    }
    std::shared_ptr<Buf> object_block = free_block_head->buf;
    free_block_head = free_block_head->next;
    return object_block;
  }
  void deallocate(std::shared_ptr<Buf> p)
  {
    std::lock_guard<std::mutex> lk(mtx);

    if (!free_block_head)
    {
      free_block_head = std::make_shared<FreeBlock>(buf_size);
      free_block_head->buf = p;
    }
    else
    {
      auto temp = std::make_shared<FreeBlock>(buf_size);
      temp->buf = p;
      temp->next = free_block_head;
      free_block_head = temp;
    }
  }

  private:
  struct FreeBlock
  {
    FreeBlock(size_t buf_size) : buf(std::make_shared<Buf>(buf_size)) {}
    // FreeBlock(Buf *)
    std::shared_ptr<Buf> buf;
    std::shared_ptr<FreeBlock> next;
  };

  std::mutex mtx;
  size_t buf_size;
  std::shared_ptr<FreeBlock> free_block_head;
};