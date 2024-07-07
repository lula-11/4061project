#include "handleClients.h"
#include <errno.h>
pthread_mutex_t clientCountLock = PTHREAD_MUTEX_INITIALIZER, letterLock = PTHREAD_MUTEX_INITIALIZER;

// print timestamp
// You can get timestamp using localtime()
void timestamp(){
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    printf("[%02d:%02d:%02d]: ", t->tm_hour, t->tm_min, t->tm_sec);
}

// Count the occurence of all 26 letters, and update letterCount[] correspondingly
void calculateLetterCount(char *filename) {
    FILE *pathFile = fopen(filename, "r");
    if (!pathFile) {
        perror("Error opening file");
        return;
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    while ((read = getline(&line, &len, pathFile)) != -1) {
        // Process each character in the line
        pthread_mutex_lock(&letterLock);
        int index = tolower(line[0]) - 'a';
        letterCount[index]++;
        pthread_mutex_unlock(&letterLock);
    }
    free(line);
    fclose(pathFile);
}



// Create a string based on letterCount[], the final return character array 
// The string looks like count1#count2#....#count26#
char* convertLetterCountToChar(){
    static char result[MSGLEN];
    int offset = 0;
    for (int i = 0; i < ALPHACOUNT; i++) {
        //printf("[helper]: count of i: %d", letterCount[i]);
        offset += snprintf(result + offset, MSGLEN - offset, "%d#", letterCount[i]);
    }
    //printResult(result,MSGLEN);
    return result;
}


pthread_mutex_t endCountLock = PTHREAD_MUTEX_INITIALIZER;
int endCount = 0;
int finalResultSent = 0;
void* processClients(void* args){
    struct thd_data *tdata = (struct thd_data *) args;
    timestamp(); // Log when thread processing a client starts
    printf("Server thread started processing client %d\n", tdata->clientID);
    int result;
    int messageCount = 0;
    
    while (1) {
        struct msg_buffer buff;

        // Waiting to receive from client process
        // Should store the message in buff
        result = msgrcv(tdata->msgqueue, &buff, MSGLEN, tdata->clientID, 0);
        if (result < 0) {
            perror("msgrcv failed");
            pthread_exit(NULL);
        }
        messageCount++;
        
        // Handle the received message
        timestamp(); // Print timestamp
        printf("Received message from client %d: %s\n", tdata->clientID, buff.mesg_text);
        
        if (strcmp(buff.mesg_text, "END") == 0){
            printf("Received END message from client %d.\n", tdata->clientID);
            
            printf("Sending final result to client %d.\n", tdata->clientID);
            char* letterCountString = convertLetterCountToChar();
            
            struct msg_buffer result_buff;
            strcpy(result_buff.mesg_text, letterCountString);
            result_buff.mesg_type = tdata->clientID + 2000; // 为每个客户端使用唯一的消息类型
            
            if (msgsnd(tdata->msgqueue, &result_buff, strlen(result_buff.mesg_text) + 1, 0) < 0) {
                perror("msgsnd failed");
                pthread_exit(NULL); // 发送失败,线程退出
            }

            timestamp();
            printf("Sent final letter count to client %d: %s\n", tdata->clientID, letterCountString);
            
            printf("Thread for client %d exiting after processing %d messages\n", tdata->clientID, messageCount);
            return NULL;
        }else{
            // if the message is not END, then it can only be a file name (one line in clienti.txt)
            // Call calculateLetterCount() to count letters
            calculateLetterCount(buff.mesg_text);
            // Send ACK message
            buff.mesg_type = tdata->clientID + 1000; // Specific ACK type for client
            strcpy(buff.mesg_text, "ACK");
            printf("Sending ACK to client %d, message type: %ld, content: %s\n", tdata->clientID, buff.mesg_type, buff.mesg_text);
            if (msgsnd(tdata->msgqueue, &buff, strlen(buff.mesg_text) + 1, 0) < 0) {
                perror("msgsnd failed");
                pthread_exit(NULL); // 发送失败,线程退出
            }
            printf("Sent ACK to client %d, message type: %ld, content: %s\n", tdata->clientID, buff.mesg_type, buff.mesg_text);
        }
    }
}
