client.c
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<pthread.h>
#include<fcntl.h>
#include<sys/shm.h>
#include<sys/mman.h>
#include<string.h>
#include<signal.h>

int local_msgCount = 0;
int readFile;
char user[32];
pthread_t reader,writer;

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

void* reading(void* args){
    char buffer;
    while(1){
        if (local_msgCount < data->msgCount)
        {
            while(read(readFile,&buffer,1) != '\0'){
                printf("%c",buffer);
            }
            //printf("\n");
            fflush(stdout);
            local_msgCount++;
        }
        
    }
}


void* writing(void* args){
  char buffer[1000];
  while (1)
  {
    fgets(buffer,1000,stdin);
    if(buffer[0] == '\n'){
      continue;
    }
    printf("\033[A");
    printf("\033[2K");
    printf("\r");
    pthread_mutex_lock(&data->fileWr);
    int writeFile = open("chat.txt", O_WRONLY);
    if (writeFile < 0) {
      perror("fd cannot be opened");
      exit(1);
    }
    lseek(writeFile, data->msgOffset, SEEK_SET);
    int written = dprintf(writeFile, "%s: %s", user, buffer);
    data->msgOffset += written;
    data->msgCount++;
    close(writeFile);
    pthread_mutex_unlock(&data->fileWr);
  }
  
}

void handle_exit(int sig) {
    pthread_mutex_lock(&data->fileWr);
    int writeFile = open("chat.txt", O_RDWR | O_APPEND);
    if (writeFile < 0) {
        perror("fd cannot be opened");
        exit(1);
    }
    lseek(writeFile, data->msgOffset, SEEK_SET);
    int written = dprintf(writeFile, "\n------User \"%s\" has left the chat room!------\n", user);
    data->msgOffset += written;

    data->msgCount++;
    close(writeFile);
    pthread_mutex_unlock(&data->fileWr);
    
    pthread_cancel(reader);
    pthread_cancel(writer);
    
    exit(0);
}

void connect(){
  printf("Enter username for chat room : ");
  scanf("%s",user);
  user[31] = '\0';
  pthread_mutex_lock(&data->unameArr);
  
  strcpy(data->usernames[data->userCount],user);
  data->userCount++;
  //signal to server
  FILE *fp = popen("pidof ./server","r");
  int pid;
  fscanf(fp,"%d",&pid);
  kill(pid,10);
  pthread_mutex_unlock(&data->unameArr);
}

int main(){
  int fd = shm_open("chatRoom", O_RDWR, 0666);

  if (fd == -1) {
      perror("shm_open");
      exit(1);
  }

  data = mmap(NULL, sizeof(shared),PROT_READ | PROT_WRITE,MAP_SHARED, fd, 0);
  if (data == MAP_FAILED) {
      perror("mmap");
      exit(1);
  }
  readFile = open("chat.txt", O_RDONLY);
  if (readFile < 0) {
    perror("open for a+");
    exit(1);
  }
  connect();
  signal(SIGINT, handle_exit);
  signal(SIGTSTP, handle_exit); 
  
  pthread_create(&reader,NULL,reading,NULL);
  pthread_create(&writer,NULL,writing,NULL);
  pthread_join(reader,NULL);
}