#include <cstring>
struct PackageHead
{
  PackageHead(const char *ip,unsigned int data_size):data_size(data_size){
    memcpy(des_add,ip,sizeof(des_add));
    des_add[strlen(ip)]=0;
  }
  char des_add[16];
  unsigned int data_size;
};