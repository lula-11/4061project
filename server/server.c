#include "server.h"
#include "handleClients.h"


#define KEY 1234

int letterCount[26];
int completedClients = 0;
int num_clients; // the number of client threads


int main(int argc, char *argv[]){
  // server only take one argument
  // Usage: ./server [process_num]

  // Print log for server start

  // intialize letterCount content to zero

  // get access to msg Queue using msgget()

  // create threads to handle incoming client requests
  // num_clients: the number of threads
  // pthread_create() should call processClients() (defined in handleClients.c) and the param should be struct thd_data* 
  // NOTE: clientID starts from 1

  // join all the threads



  if (argc != 2) {
      fprintf(stderr, "Usage: %s [process_num]\n", argv[0]);
      return EXIT_FAILURE;
  }

  num_clients = atoi(argv[1]);

  // Print log for server start
  timestamp();
  printf("Server has started\n");

  // Initialize letterCount content to zero
  memset(letterCount, 0, sizeof(letterCount));

  // Get access to msg Queue using msgget()
  int msgqueue = msgget(KEY, 0666 | IPC_CREAT);
  if (msgqueue == -1) {
    perror("Failed to create message queue");
    return EXIT_FAILURE;
  }
  printf("】Message queue created with id: %d\n", msgqueue);
  struct msqid_ds buf;
  buf.msg_qbytes = 65536;  // 设置容量为64KB
  if (msgctl(msgqueue, IPC_STAT, &buf) < 0) {
      perror("msgctl failed");
      return EXIT_FAILURE;
  }
printf("Message queue capacity: %ld bytes\n", buf.msg_qbytes);
  // Create threads to handle incoming client requests
  pthread_t threads[num_clients];
  struct thd_data data[num_clients];

  for (int i = 0; i < num_clients; i++) {
      data[i].clientID = i + 1; 
      data[i].msgqueue = msgqueue;
      if (pthread_create(&threads[i], NULL, processClients, &data[i]) != 0) {
          perror("Failed to create thread");
          return EXIT_FAILURE;
      }
  }

  // Join all the threads
  for (int i = 0; i < num_clients; i++) {
      pthread_join(threads[i], NULL);
  }

    // Add a short delay before removing the message queue
    sleep(5);

  // Close msgqueue
  msgctl(msgqueue, IPC_RMID, NULL);

  timestamp();
  printf("Server has ended\n");

  return 0;
}
