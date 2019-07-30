#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <libgen.h> // dirname

#include "ServerClient.h"


struct argsServer
{
  int threadPoolsize;
  char directory[PATH_MAX];
  int portNumber;
  int socket;

  pthread_t * tid;
  int * tidEmpty;
  pthread_attr_t * thread_attr;
  int thread_i;
};



int serverSocket = -1;
struct argsServer arguments;
int main_exit = 0;

void * socketThread(void *arg);
void sig_handler(int signo);

//BibakBOXServer [directory] [threadPoolSize] [portnumber]


int main(int argc, char **argv) {
  int i = 0;
  int newSocket;
  struct sockaddr_in serverAddr;
  struct sockaddr_storage serverStorage;
  socklen_t addr_size;

  struct stat statbuf;

  realpath(argv[1], arguments.directory);


  if (argc != 4) {
    fprintf(stderr, "--Less or more argument-- Usage: %s [directory] [threadPoolSize] [portnumber] \n", argv[0]);
    exit(-1);
  } else {
    lstat(arguments.directory, &statbuf);
    if (S_ISREG(statbuf.st_mode))  {
      fprintf(stderr, "Wrong argument to directory -- Usage: %s [directory] [threadPoolSize] [portnumber] \n", argv[0]);
      exit(0);
    } else {
      if (stat(arguments.directory, &statbuf) == -1) {
        mkdir(arguments.directory, 0700);
      }
    }
  }


  struct sigaction a;
  a.sa_handler = sig_handler;
  a.sa_flags = 0;
  sigemptyset( &a.sa_mask );
  sigaction( SIGINT, &a, NULL );
  sigaction( SIGTERM, &a, NULL );

  arguments.threadPoolsize = atoi(argv[2]);
  arguments.portNumber = atoi(argv[3]);

  //fprintf(stdout, "Dirname: %s threadPoolsize : %d portnumber: %d \n",arguments->directory,arguments->threadPoolsize, arguments->portNumber );


  serverSocket = socket(PF_INET, SOCK_STREAM, 0);
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(arguments.portNumber);
  serverAddr.sin_addr.s_addr = inet_addr("0.0.0.0");
  memset(serverAddr.sin_zero, '\0', sizeof(serverAddr.sin_zero));

  if ( 0 != bind(serverSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr))) {
    printf("bind error\n");
    exit(0);
  }

  if (listen(serverSocket, arguments.threadPoolsize) == 0)
    printf("Listening\n");
  else {
    printf("Error\n");
    exit(0);
  }

  pthread_t tid[arguments.threadPoolsize];
  pthread_attr_t thread_attr[arguments.threadPoolsize];
  int tidEmpty[arguments.threadPoolsize];
  for (i = 0 ; i < arguments.threadPoolsize ; i++)
    tidEmpty[i] = 1;

  arguments.tid = tid;
  arguments.tidEmpty = tidEmpty;
  arguments.thread_attr = thread_attr;
  arguments.thread_i = -1;



  struct argsServer = threadArgs[arguments.threadPoolsize];

  while (main_exit == 0)
  {
    //printf("main_exit: %d\n", main_exit);
    addr_size = sizeof(serverStorage);
    newSocket = accept(serverSocket, (struct sockaddr *) &serverStorage, &addr_size);
    if (newSocket == -1)
      continue;


    for (i = 0 ; i < arguments.threadPoolsize ; i++) {
      if (tidEmpty[i] == 1)
        break;
    }
    if (i == arguments.threadPoolsize) {
      close(newSocket);
      continue;
    }

    int thread_i = i;
    memcpy(&threadArgs[thread_i], &arguments, sizeof(arguments));
    threadArgs[thread_i].socket = newSocket;
    threadArgs[thread_i].thread_i = thread_i;

    if ( pthread_create(&tid[thread_i], NULL, socketThread, (void *)&threadArgs[thread_i]) == 0 ) {
      tidEmpty[thread_i] = 0;
    } else {
      printf("Failed to create thread\n");
      close(newSocket);
    }
  }

  close(serverSocket);

  printf("THREADLER SONLANDIRILIYOR...\n");

  for (i = 0 ; i < arguments.threadPoolsize ; i++) {
    if (tidEmpty[i] == 0) {
      pthread_join(tid[i], NULL);
    }
  }

  printf("Goodbye ... \n");

  return 0;
}

void sig_handler(int signo)
{

  if (signo == SIGINT || signo == SIGTERM) {

    main_exit = 1;
    return;
  }

}


void * socketThread(void *arg) {


  //printf("socketThread\n");

  struct argsServer * arguments = (struct argsServer*)arg;

  int newSocket = ((struct argsServer*)arg)->socket;
  char ServerFolder[PATH_MAX];
  strcpy(ServerFolder, ((struct argsServer*)arg)->directory);

  struct timeval tv;
  tv.tv_sec = 10;
  tv.tv_usec = 0;
  setsockopt(newSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

  char clientPathName[PATH_MAX];
  int run = 1;
  if (receiveData(newSocket, &clientPathName, sizeof(clientPathName)) < 0) {
    run = 0;
  }

  printf("CLIENT CONNECTED: %s\n", clientPathName);

  char clientFolder[PATH_MAX];
  char tmp2[PATH_MAX];

  sprintf(tmp2, "%s/%s", ServerFolder, clientPathName);
  realpath(tmp2, clientFolder);

  struct stat statbuf;
  if (stat(clientFolder, &statbuf) == -1) {
    mkdir(clientFolder, 0700);
  }

  while (run==1 && main_exit==0) {
    int messageNo;
    if (receiveData(newSocket, &messageNo, sizeof(messageNo)) <= 0) {
      break;
    }

    if (messageNo == MESSAGE_REQUEST_FILE_LIST) {

      //printf("MESSAGE_REQUEST_FILE_LIST\n");

      FileList_t serverFiles;
      fileList_init(&serverFiles);
      depthFirstApply(clientFolder, &serverFiles, clientFolder);

      /*
      int i;
      for (i = 0; i < serverFiles.count ; i++) {
        printf("%s\n", serverFiles.array[i].pathName);
      }*/

      sendData(newSocket, &serverFiles, sizeof(FileList_t));

      if (serverFiles.count > 0) {
        sendData(newSocket, serverFiles.array, serverFiles.count * sizeof(buffer_t));
      }


      fileList_free(&serverFiles);

    } else if (messageNo == MESSAGE_FILE) {
     
     // printf("MESSAGE_FILE\n");

      int r = receiveFile(newSocket, ServerFolder);
      if (r != 0) {
        break;
      }

    } else if (messageNo == MESSAGE_FILE_REQUEST) {
      
      //printf("MESSAGE_FILE_REQUEST\n");

      char pathName[PATH_MAX];
      if (receiveData(newSocket, &pathName, PATH_MAX) < 0) {
        printf("Receive failed\n");
        break;
      }
      sendFile(pathName, ServerFolder, newSocket);

    } else if (messageNo == MESSAGE_FOLDER) {

      //printf("MESSAGE_FOLDER\n");

      receiveCreateFolder(newSocket, ServerFolder);

    } else if (messageNo == MESSAGE_FOLDER_REMOVE) {

     // printf("MESSAGE_FOLDER_REMOVE\n");

      receiveRemoveFolder(newSocket, ServerFolder);


    } else if (messageNo == MESSAGE_FILE_REMOVE) {

     // printf("MESSAGE_FILE_REMOVE\n");

      receiveRemoveFile(newSocket, ServerFolder);


    } else if (messageNo == MESSAGE) {
      //printf("MESSAGE\n");

    } else {
     // printf("messageNo: %x\n", messageNo);
      break;
    }

  }

  close(newSocket);
  arguments->tidEmpty[arguments->thread_i] = 1;
 // printf("Exit socketThread \n");
  pthread_exit(NULL);
}

