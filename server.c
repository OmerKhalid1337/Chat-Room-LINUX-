#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<pthread.h>
#include<fcntl.h>
#include<sys/shm.h>
#include<sys/mman.h>
#include<string.h>
#include<signal.h>


int file;
typedef struct {
    char usernames[100][32];
    int userCount;
    pthread_mutex_t unameArr;
    pthread_mutex_t fileWr;
    pthread_mutex_t userIndex;
    int userExitIndex;
    int msgCount;
    off_t msgOffset; 
} shared;

shared* data;
int clientCount = 0;

void handleConnect(int signum){
  pthread_mutex_lock(&data->fileWr);

  lseek(file, data->msgOffset, SEEK_SET);
  int written = dprintf(file, "\n------User \"%s\" has joined the chat room!------\n", data->usernames[clientCount]);
  data->msgOffset += written;
  clientCount++;
  data->msgCount++;

  pthread_mutex_unlock(&data->fileWr);
}

void handleClientExit(int sig) {
  pthread_mutex_lock(&data->fileWr);

  lseek(file, data->msgOffset, SEEK_SET);
  int written = dprintf(file, "\n------User \"%s\" has left the chat room!------\n", data->usernames[data->userExitIndex]);
  data->msgOffset += written;
  data->msgCount++;

  pthread_mutex_unlock(&data->fileWr);


  pthread_mutex_lock(&data->unameArr);

  for(int i=data->userExitIndex;i<data->userCount - 1;i++){
    strcpy(data->usernames[i],data->usernames[i+1]);
  }
  data->userCount--;
  clientCount--;

  pthread_mutex_unlock(&data->unameArr);
  pthread_mutex_unlock(&data->userIndex);
}

void handle_server_exit(int sig)
{
  pthread_mutex_lock(&data->fileWr);
  lseek(file, data->msgOffset, SEEK_SET);

  int written = dprintf(file, "\n LOST CONNECTION TO CHAT ROOM \n");
  
  data->msgOffset += written;
  data->msgCount++;
  pthread_mutex_unlock(&data->fileWr);
  
  sleep(2);
  
  FILE *fp = popen("pidof ./client","r");
  int pid;
  while (fscanf(fp, "%d", &pid) == 1) {
      // printf("%d\n",pid);
      kill(pid,9);
    }
  pclose(fp);
  exit(0);
}

int main(){

  FILE* fp = popen("pidof ./server", "r");
    int pid;

    char buffer[1024];
    fgets(buffer, sizeof(buffer), fp);
    
    int count = 0;
    char* token = strtok(buffer, " \n");
    while (token) {
        count++;
        token = strtok(NULL, " \n");
    }

    if(count > 1){
        printf("\n  A SERVER IS ALREADY RUNNING \n");
        fflush(stdout);
        exit(0);
    }
  pclose(fp);

	printf("Server is running...\n");
  if(signal(SIGUSR1,handleConnect) == SIG_ERR){
    printf("error");
  }
  if(signal(SIGUSR2,handleClientExit) == SIG_ERR){
    printf("error");
  }
  if(signal(SIGTSTP,handle_server_exit) == SIG_ERR){
    printf("error");
  }
  if(signal(SIGINT,handle_server_exit) == SIG_ERR){
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
  pthread_mutex_init(&data->userIndex, &attr);

  //file = open("chat.txt", O_WRONLY | O_TRUNC | O_CREAT, 0644);
//   if (file < 0) {
//       perror("open for truncate");
//       exit(1);
//   }
//  // close(file);  // Truncation is done

  file = open("chat.txt", O_RDWR | O_CREAT, 0644);
  if (file < 0) {
  perror("open for write");
  exit(1);
}
pthread_mutex_lock(&data->fileWr);
off_t offset = lseek(file, 0, SEEK_END);
data->msgOffset = offset;
pthread_mutex_unlock(&data->fileWr);
  while(1){
      //waits
      sleep(1);
  }



  pthread_mutexattr_destroy(&attr);
  close(file);
}

