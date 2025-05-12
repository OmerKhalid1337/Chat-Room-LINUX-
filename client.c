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
int readFile, writeFile;
char user[32];
pthread_t reader, writer;

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

void* reading(void* args) {
    char buffer[1024];
    ssize_t bytesRead;
    while (1) {
        if (local_msgCount < data->msgCount) {
            pthread_mutex_lock(&data->fileWr);
            lseek(readFile, 0, SEEK_SET);
            while ((bytesRead = read(readFile, buffer, sizeof(buffer) - 1)) > 0) {
                buffer[bytesRead] = '\0';
                printf("%s", buffer);
            }
            pthread_mutex_unlock(&data->fileWr);
            fflush(stdout);
            local_msgCount++;
        }
        usleep(500000); // Sleep for 0.5 seconds
    }
}

void* writing(void* args) {
    char buffer[1000];
    while (1) {
        fgets(buffer, sizeof(buffer), stdin);
        if (buffer[0] == '\n') {
            continue;
        }

        buffer[strcspn(buffer, "\n")] = '\0'; // Strip newline

        pthread_mutex_lock(&data->fileWr);
        lseek(writeFile, data->msgOffset, SEEK_SET);
        int written = dprintf(writeFile, "%s: %s\n", user, buffer);
        data->msgOffset += written;
        data->msgCount++;
        pthread_mutex_unlock(&data->fileWr);
    }
}

void connect() {

    FILE* fp = popen("pidof ./server", "r");
    int pid;
    if(fscanf(fp, "%d", &pid) == EOF){
        printf("\n SERVER NOT FOUND \n");
        fflush(stdout);
        exit(0);
    }
    pclose(fp);

    while (1) {

        printf("Enter username for chat room: ");
        fgets(user, sizeof(user), stdin);
        user[strcspn(user, "\n")] = '\0';

        int isDuplicate = 0;  //tls

        pthread_mutex_lock(&data->unameArr);
        for (int i = 0; i < data->userCount; i++) {
            if (strcmp(data->usernames[i], user) == 0) {
                isDuplicate = 1;
                break;
            }
        }

        if (isDuplicate) {
            printf("Username already taken. Please try another one.\n");
            pthread_mutex_unlock(&data->unameArr);
        } 
        else {
            strcpy(data->usernames[data->userCount], user);
            data->userCount++;

            // Notify server
            kill(pid, 10);
            pthread_mutex_unlock(&data->unameArr);
            break; // Exit loop if username is unique
        }
    }
}


void handle_exit(int sig) {
    FILE* fp = popen("pidof ./server", "r");
    int pid;
    fscanf(fp, "%d", &pid);

    pthread_mutex_lock(&data->userIndex);
    for (int i = 0; i < data->userCount; i++) {
        if (strcmp(data->usernames[i], user) == 0) {
            data->userExitIndex = i;
            break;
        }
    }

    // shm_unlink("chatRoom");
    kill(pid, 12);
    pthread_cancel(reader);
    pthread_cancel(writer);
    exit(0);
}

int main() {
    int fd = shm_open("chatRoom", O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
        exit(1);
    }

    data = mmap(NULL, sizeof(shared), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    readFile = open("chat.txt", O_RDONLY);
    if (readFile < 0) {
        perror("open for read");
        exit(1);
    }

    writeFile = open("chat.txt", O_WRONLY);
    if (writeFile < 0) {
        perror("open for write");
        exit(1);
    }

    connect();

    signal(SIGINT, handle_exit);
    signal(SIGTSTP, handle_exit);

    pthread_create(&reader, NULL, reading, NULL);
    pthread_create(&writer, NULL, writing, NULL);

    pthread_join(reader, NULL);
    pthread_join(writer, NULL);

    return 0;
}

