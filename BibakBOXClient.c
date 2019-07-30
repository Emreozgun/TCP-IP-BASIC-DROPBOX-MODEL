#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close
#include <pthread.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h> // dirname
#include <signal.h>

#include "ServerClient.h"


struct argsClient
{
  char dirName[PATH_MAX];
  char ipAdress[100];
  int portNumber;
};


void * client(char *ClientFolder, char *client_ipAdress, int client_PortNum);
void sig_handler(int signo);

//BibakBOXClient [dirName] [ip address] [portnumber]


int main(int argc, char **argv) {

  //fprintf(stdout, "argc : %d \n",argc );
  
  if(argc != 4){
    fprintf(stderr, "--Less or more argument-- Usage: %s [dirName] [ip address] [portnumber] \n", argv[0]);
    exit(-1);
  }



  struct sigaction a;
  a.sa_handler = sig_handler;
  a.sa_flags = 0;
  sigemptyset( &a.sa_mask );
  sigaction( SIGINT, &a, NULL );
  sigaction( SIGTERM, &a, NULL );
  sigaction( SIGPIPE, &a, NULL );

  char realPath[PATH_MAX];
  realpath(argv[1], realPath);

  client(realPath, argv[2], atoi(argv[3]));
  
  return 0;

}

void sig_handler(int signo)
{

  if (signo == SIGINT || signo == SIGTERM){
    printf("\n Goodbye ... \n");
    exit(0);
  }

  if (signo == SIGPIPE) {
    printf("SERVERLA BAGLANTI KOPTU\n");
    exit(0);
  }


}


int getFileList(int clientSocket, FileList_t * serverFiles) {

  sendData(clientSocket, &MESSAGE_REQUEST_FILE_LIST, sizeof(MESSAGE_REQUEST_FILE_LIST));

  if (receiveData(clientSocket, serverFiles, sizeof(FileList_t)) < 0)
  {
    printf("Receive failed\n");
    return -1;
  }

  serverFiles->array = malloc(serverFiles->size * sizeof(buffer_t));

  if (serverFiles->count > 0) {

    // printf("serverFiles->size: %d\n", serverFiles->size);
    // printf("serverFiles.count: %d\n", serverFiles->count);
    int r = receiveData(clientSocket, serverFiles->array, serverFiles->count * sizeof(buffer_t));
    if (r < 0 || r != sizeof(buffer_t) * serverFiles->count) {
      printf("Receive failed, %d\n", r);
      return -1;
    }
  }


  return 0;
}


void copyFiles(int clientSocket, char * folder, int firstRun, char * clientPathName) {
  int i;


  FileList_t serverFiles = { 0 };
  if (getFileList(clientSocket, &serverFiles) != 0) {
    fprintf(stderr, "getFileList error! \n");
    return;
  }
  /*
  printf("--------------------\n");
  for (i = 0; i < serverFiles.count ; i++) {
    if (serverFiles.count > 0)
      printf("%s\n", serverFiles.array[i].pathName);
  }*/

  FileList_t clientFiles;
  fileList_init(&clientFiles);
  depthFirstApply(folder, &clientFiles, folder);

  /*
  for (i = 0; i < clientFiles.count ; i++) {
    if (clientFiles.count > 0)
      printf("%s\n", clientFiles.array[i].pathName);
  }*/


  for (i = 0 ; i < clientFiles.count ; i++) {
    int index = fileList_search(&serverFiles, clientFiles.array[i].pathName);
    if (index == -1) {
      //printf("clientta var serverda yok: %s\n", clientFiles.array[i].pathName);

      if (clientFiles.array[i].type == 0) {
          sendCreateFolder(clientSocket, clientFiles.array[i].pathName);
      } else {

          char * tmp = strdup(folder);
          sendFile(clientFiles.array[i].pathName, dirname(tmp), clientSocket);
          free(tmp);
      }

    } else {

      if (serverFiles.array[index].type == 0 && clientFiles.array[i].type == 0) { //Folder ise bakmadan devam et
        continue;
      }

      if (serverFiles.array[index].modificationTime < clientFiles.array[i].modificationTime) {

        char * tmp = strdup(folder);
        sendFile(clientFiles.array[i].pathName, dirname(tmp), clientSocket);
        free(tmp);

      }
    }
  }

  for (i = 0 ; i < serverFiles.count ; i++) {
    //printf("%s\n", serverFiles.array[i].pathName);
    int index = fileList_search(&clientFiles, serverFiles.array[i].pathName);
    if (index == -1) {
      //printf("serverda var clientta yok: %s\n", serverFiles.array[i].pathName);

      if (serverFiles.array[i].type == 0) {
        if (firstRun) {
          multiple_mkdir(folder, serverFiles.array[i].pathName + strlen(clientPathName) + 1);
        } else {
          //printf("REMOVE FOR FOLDER: %s \n",serverFiles.array[i].pathName);
          sendRemoveFolder(clientSocket,serverFiles.array[i].pathName);
        }

      } else {

        if (firstRun) {
          sendData(clientSocket, &MESSAGE_FILE_REQUEST, sizeof(MESSAGE_FILE_REQUEST));
          sendData(clientSocket, serverFiles.array[i].pathName, PATH_MAX);

          int messageNo;
          if (receiveData(clientSocket, &messageNo, sizeof(messageNo)) < 0 || messageNo != MESSAGE_FILE) {
            break;
          }

          char folder2[PATH_MAX];
          strncpy(folder2, folder, strlen(folder) - (strlen(clientPathName)));

          if (receiveFile(clientSocket, folder2) != 0) {
            printf("receiveFile error\n");
            break;
          }

        }else{

          //printf("REMOVE FOR FILE: %s  \n",serverFiles.array[i].pathName);
          sendRemoveFile(clientSocket,serverFiles.array[i].pathName);
        }


      }
    }
  }


  fileList_free(&clientFiles);
  fileList_free(&serverFiles);
}

void * client(char *ClientFolder, char *client_ipAdress, int client_PortNum)
{

  int clientSocket;
  struct sockaddr_in serverAddr;
  socklen_t addr_size;


  clientSocket = socket(PF_INET, SOCK_STREAM, 0);
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(client_PortNum);
  serverAddr.sin_addr.s_addr = inet_addr(client_ipAdress);

  memset(serverAddr.sin_zero, '\0', sizeof(serverAddr.sin_zero));
 

  addr_size = sizeof(serverAddr);

  if (connect(clientSocket, (struct sockaddr * ) & serverAddr, addr_size) != 0) {
    printf("connect failed\n");
    printf("%s\n", strerror(errno));
    exit(1);
  }


  char * tmp = strdup(ClientFolder);
  char clientPathName[PATH_MAX];
  strcpy(clientPathName, basename(tmp));
  free(tmp);
  

  if (sendData(clientSocket, clientPathName, sizeof(clientPathName)) < 0) {
    printf("send failed\n");
    exit(1);
  }

  copyFiles(clientSocket, ClientFolder, 1, clientPathName);

  time_t lastModification = getModificationTime(ClientFolder);


  while (1) {
    if (sendData(clientSocket, &MESSAGE, sizeof(MESSAGE)) < 0) {
      printf("server kapandi\n");
    }
    
    if (lastModification != getModificationTime(ClientFolder)) {
      copyFiles(clientSocket, ClientFolder, 0, clientPathName);
      lastModification = getModificationTime(ClientFolder);
    }

  }

  close(clientSocket);

  return 0;

}

