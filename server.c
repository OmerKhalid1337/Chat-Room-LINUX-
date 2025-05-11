server.c
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<pthread.h>
#include<fcntl.h>
#include<sys/shm.h>
#include<sys/mman.h>
#include<string.h>
#include<signal.h>


typedef struct {
    char usernames[100][32];
    int userCount;
    pthread_mutex_t unameArr;
    int file;
    pthread_mutex_t fileWr;
    int msgCount;
    off_t msgOffset; 
} shared;

shared* data;
int clientCount = 0;
void sendConnectMsg(){
  pthread_mutex_lock(&data->fileWr);

  lseek(data->file, data->msgOffset, SEEK_SET);
  int written = dprintf(data->file, "\n------User \"%s\" has joined the chat room!------\n", data->usernames[clientCount]);
  data->msgOffset += written;

   clientCount++;
    data->msgCount++;
  pthread_mutex_unlock(&data->fileWr);
}

void handleConnect(int signum){
    sendConnectMsg();
}

int main(){
	printf("Server is running...\n");
  if(signal(SIGUSR1,handleConnect) == SIG_ERR){
    printf("error");
  }

  int fd = shm_open("chatRoom", O_CREAT | O_RDWR, 0666);
  if (fd == -1) {
      perror("shm_open");
      exit(1);
  }
  ftruncate(fd,sizeof(shared));

  data = mmap(NULL, sizeof(shared),PROT_READ | PROT_WRITE,MAP_SHARED, fd, 0);
  if (data == MAP_FAILED) {
      perror("mmap");
      exit(1);
  }

  data->msgCount = 0;
  data->userCount = 0;

  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
  pthread_mutex_init(&data->unameArr, &attr);
  pthread_mutex_init(&data->fileWr, &attr);

  //data->file = open("chat.txt", O_WRONLY | O_TRUNC | O_CREAT, 0644);
//   if (data->file < 0) {
//       perror("open for truncate");
//       exit(1);
//   }
//  // close(data->file);  // Truncation is done

  data->file = open("chat.txt", O_RDWR | O_CREAT, 0644);
  if (data->file < 0) {
  perror("open for write");
  exit(1);
}
off_t offset = lseek(data->file, 0, SEEK_END);
data->msgOffset = offset;

  while(1){
      //waits
      sleep(1);
  }



  pthread_mutexattr_destroy(&attr);
  close(data->file);
}