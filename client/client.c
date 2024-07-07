#include <sys/types.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>

#define MSGLEN 1024
#define NUMARGS 2
#define DIRNULL NULL
#define KEY 1234

struct msg_buffer {
    long mesg_type;
    char mesg_text[MSGLEN];
};

void timestamp(){
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    printf("[%02d:%02d:%02d]: ", t->tm_hour, t->tm_min, t->tm_sec);
}

void recursiveTraverseFS(int mappers, char *basePath, FILE *fp[], int *toInsert, int *nFiles){
    struct dirent *dirContentPtr;
    char path[1024];  // Increased buffer size to accommodate larger paths
    DIR *dir = opendir(basePath);
    
    if (dir == NULL){
        perror("Unable to read directory");
        exit(1);
    }
    int count =0;
    while ((dirContentPtr = readdir(dir)) != NULL){
        if (strcmp(dirContentPtr->d_name, ".") != 0 && strcmp(dirContentPtr->d_name, "..") != 0) {
            snprintf(path, sizeof(path), "%s/%s", basePath, dirContentPtr->d_name);
            if (dirContentPtr->d_type == DT_REG) {
                fprintf(fp[*toInsert], "%s\n", path);
                *toInsert = (*toInsert + 1) % mappers;
                (*nFiles)++;
            } 
        }   
    }
    closedir(dir);
}

void traverseFS(int clients, char *path){
    FILE *fp[clients];
    char clientInputFolder[256];
    snprintf(clientInputFolder, sizeof(clientInputFolder), "%s/ClientInput", "../client");
    mkdir(clientInputFolder, 0755);

    for (int i = 0; i < clients; i++){
        char fileName[300];
        snprintf(fileName, sizeof(fileName), "%s/client%d.txt", clientInputFolder, i);
        fp[i] = fopen(fileName, "w");
        if (!fp[i]) {
            fprintf(stderr, "Failed to open file: %s\n", fileName);
            exit(1);
        }
    }

    int toInsert = 0;
    int nFiles = 0;
    recursiveTraverseFS(clients, path, fp, &toInsert, &nFiles);

    for (int i = 0; i < clients; i++){
        fclose(fp[i]);
    }
}

int receive_message_with_timeout(int msqid, struct msg_buffer *msg, long msg_type, int timeout_ms) {
    int rc;
    struct timespec ts;
    ts.tv_sec = 1; // 等待1秒
    ts.tv_nsec = 0;

    timestamp();
    printf("Start waiting for message with type %ld\n", msg_type);

    while ((rc = msgrcv(msqid, msg, sizeof(msg->mesg_text), msg_type, IPC_NOWAIT)) == -1) {
        if (errno == ENOMSG) { // 没有消息可读
            nanosleep(&ts, NULL); // 等待一定时间再尝试
            continue;
        }

        perror("msgrcv failed");
        return -1;
    }

    timestamp();
    printf("Received message with type %ld\n", msg_type);
    return 0;
}


int countFilesInDirectory(const char *basePath) {
    DIR *dir = opendir(basePath);
    struct dirent *dirEntry;
    int fileCount = 0;

    if (dir == NULL) {
        perror("Unable to open directory");
        return -1; // 返回-1表示打开目录失败
    }

    while ((dirEntry = readdir(dir)) != NULL) {
        if (dirEntry->d_type == DT_REG) {  // 检查是否是常规文件
            fileCount++;  // 只计数文件
        }
    }
    printf("hi,%d\n",fileCount);
    fileCount = 0;
    while ((dirEntry = readdir(dir)) != NULL) {
    printf("Entry: %s, Type: %d\n", dirEntry->d_name, dirEntry->d_type);
    if (dirEntry->d_type == DT_REG) {
        fileCount++;
    }
}
    printf("hi,%d\n",fileCount);
    closedir(dir);
    return fileCount;
}





int main(int argc, char *argv[]){ 
    if (argc != 3) {
        fprintf(stderr, "Usage: %s [input folder] [process num]\n", argv[0]);
        return EXIT_FAILURE;
    }
    char folderName[256];
    strcpy(folderName, argv[1]);
    int num_clients = atoi(argv[2]);
    int result = countFilesInDirectory(argv[1]);
    traverseFS(num_clients, folderName);

    int msgqueue = msgget(KEY, 0666 | IPC_CREAT);
    if (msgqueue < 0){
        perror("Failed to create/access message queue");
        return EXIT_FAILURE;
    }

    for (int i = 0; i < num_clients; i++){
        pid_t pid = fork();
        if (pid < 0){
            perror("Fork failed");
            return EXIT_FAILURE;
        }
        if (pid == 0){  // Child process
            char fileName[300];
            snprintf(fileName, sizeof(fileName), "ClientInput/client%d.txt", i);
            FILE *ftr = fopen(fileName, "r");
            if (!ftr) {
                perror("Unable to open file");
                exit(EXIT_FAILURE);
            }

            struct msg_buffer msg, ack_msg;
            msg.mesg_type = i + 1;  // Unique type for each client

            char line[MSGLEN];

            timestamp();
            printf("Client %d starts sending file paths\n", i + 1);

            while (fgets(line, sizeof(line), ftr) != NULL) {
                line[strcspn(line, "\n")] = 0;  // Remove newline character
                strcpy(msg.mesg_text, line);

                if (msgsnd(msgqueue, &msg, strlen(msg.mesg_text) + 1, 0) == -1) {
                    perror("msgsnd failed");
                    exit(EXIT_FAILURE);
                }

                timestamp();
                printf("Client %d waiting for ACK...\n", i + 1);

                if (receive_message_with_timeout(msgqueue, &ack_msg, i + 1001, 5000) == -1) {
                    fprintf(stderr, "Timeout waiting for ACK from server\n");
                    exit(EXIT_FAILURE);
                }

                timestamp();
                printf("Client %d received ACK for: %s\n", i + 1, msg.mesg_text);
            }

            timestamp();
            printf("Client %d finished sending file paths, sending END\n", i + 1);

            strcpy(msg.mesg_text, "END");
            if (msgsnd(msgqueue, &msg, strlen(msg.mesg_text) + 1, 0) == -1) {
                perror("END msgsnd failed");
                exit(EXIT_FAILURE);
            }

            fclose(ftr);

            timestamp();
            if (receive_message_with_timeout(msgqueue, &msg, i + 2001, 5000) == -1) {
                fprintf(stderr, "Timeout waiting for final result from server\n");
                exit(EXIT_FAILURE);
            }

            printf("Client %d received final result: %s\n", i + 1, msg.mesg_text);

            char outputDir[293];
            snprintf(outputDir, sizeof(outputDir), "%s/Output", "../client");
            mkdir(outputDir, 0755);  // Ensure the output directory exists

            char resultFilePath[293];
            snprintf(resultFilePath, sizeof(resultFilePath), "%s/Output/client%d_result.txt", "../client", i);
            FILE *outputFile = fopen(resultFilePath, "w");
            if (!outputFile) {
                perror("Unable to open output file");
                exit(EXIT_FAILURE);
            }
            fprintf(outputFile, "%s", msg.mesg_text);
            fclose(outputFile);

            exit(0);  // Terminate the child process successfully
        }
    }

    // Parent process waits for all children to finish
    for (int j = 0; j < num_clients; j++) {
        wait(NULL);
    }

    return 0;
}
