#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int
main(int argc, char *argv[])
{
  int fd;
  char buf[100];
  
  // Get initial read count
  uint64 initial_count = getreadcount();
  printf("Initial read count: %ld\n", initial_count);
  
  // Open a file for reading
  if((fd = open("README", O_RDONLY)) < 0){
    printf("Error: cannot open README file\n");
    exit(1);
  }
  
  // Read 100 bytes from the file
  int bytes_read = read(fd, buf, 100);
  printf("Read %d bytes from file\n", bytes_read);
  
  // Get updated read count
  uint64 updated_count = getreadcount();
  printf("Updated read count: %ld\n", updated_count);
  
  // Verify the count increased by the number of bytes read
  printf("Difference: %ld\n", updated_count - initial_count);
  
  close(fd);
  exit(0);
}
