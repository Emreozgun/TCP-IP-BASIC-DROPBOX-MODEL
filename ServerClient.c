#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <linux/limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <utime.h>
#include <sys/stat.h>
#include <libgen.h> // basename


#include "ServerClient.h"

const int MESSAGE_REQUEST_FILE_LIST = 0x12341111;
const int MESSAGE_FILE_LIST = 0x12342222;
const int MESSAGE_FILE_REQUEST = 0x12343333;
const int MESSAGE_FILE = 0x12344444;
const int MESSAGE = 0x12345555;
const int MESSAGE_FOLDER = 0x12346666;
const int MESSAGE_FOLDER_REMOVE = 0x12347777;
const int MESSAGE_FILE_REMOVE = 0x12348888;

void fileList_add(FileList_t * a, char * pathName, int size, time_t modificationTime, int type) {
  if (a->count == a->size) {
    a->size *= 2;
    a->array = (buffer_t *) realloc(a->array, a->size * sizeof(buffer_t));
    if (a->array == NULL) {
      printf("bellek yetersiz\n");
      exit(1);
    }
  }


  buffer_t * last = &a->array[a->count];
  strcpy(last->pathName, pathName);
  last->size = size;
  last->modificationTime = modificationTime;
  last->type = type;
  a->count++;
}

void fileList_init(FileList_t * a) {
  a->size = 16;
  a->count = 0;
  a->array = (buffer_t *) malloc(a->size * sizeof(buffer_t));
}

void fileList_free(FileList_t * a) {
  if (a->array != NULL && a->size > 0) {
    free(a->array);
    a->array = NULL;
    a->size = 0;
    a->count = 0;
  }
}

int fileList_search(FileList_t * a, char * pathName) {

  int i;
  for (i = 0 ; i < a->count ; i++) {
    if (strcmp(a->array[i].pathName, pathName) == 0) {
      return i;
    }
  }
  return -1;
}



void depthFirstApply (char *path, FileList_t * array, char *mainFolder) {
  //printf("%s %s\n", mainFolder, path);

  struct dirent *direntp;
  struct stat statbuf;
  char file_path[PATH_MAX];
  DIR *dirp;


  if ( (dirp = opendir(path)) == NULL) 
  {
    perror("Failed to open directory");
    return;
  }

  while (1)
  {
    if ((direntp = readdir(dirp)) == NULL) {
      break;
    }

    if (strcmp(direntp->d_name, ".") == 0 || strcmp(direntp->d_name, "..") == 0)
      continue;


    sprintf(file_path, "%s/%s", path, direntp->d_name);

    if (lstat(file_path, &statbuf) != 0)
      continue;

    if (S_ISDIR(statbuf.st_mode)) { 

      char * path2 = strdup(mainFolder);
      char * pathName = file_path + strlen(mainFolder) - strlen(basename(path2));
      free(path2);
    
      
      fileList_add(array, pathName, (int)statbuf.st_size, (time_t)statbuf.st_mtime, 0);

      depthFirstApply(file_path, array, mainFolder);
    }
    else if (S_ISREG(statbuf.st_mode)) {
      char * path2 = strdup(mainFolder);
      char * pathName = file_path + strlen(mainFolder) - strlen(basename(path2));
      free(path2);

      fileList_add(array, pathName, (int)statbuf.st_size, (time_t) statbuf.st_mtime, 1);

    }
    else {
      /**/

    }

  }
  
  closedir(dirp);

  return;
}


int sendData(int fd, const void * buf2, int size) {
  const char * buf = (char*)buf2;
  int sendSize = 0;
  while (sendSize < size) {
    int r = write(fd, buf + sendSize, size-sendSize);
    if (r < 0)
      return -1;
    sendSize += r;
  }
  return sendSize;
}

int receiveData(int fd, void * buf2, int size) {
  char * buf = (char*)buf2;
  int receiveSize = 0;

  while (receiveSize < size) {
    int r = read(fd, buf + receiveSize, size-receiveSize);
    if (r < 0)
      return -1;
    receiveSize += r;
  }
  return receiveSize;
}

int sendFile(char *pathName, char *directory, int socket) {

  char file_path[PATH_MAX];
  struct stat statbuf;
  buffer_t fileInfo = { 0 };

  sprintf(file_path, "%s/%s", directory, pathName);
  //printf("SEND %s\n", file_path);



  if (lstat(file_path, &statbuf) < 0) {
    printf("Size error: %s\n", file_path);
    return -1;
  }

  strcpy(fileInfo.pathName, pathName);
  fileInfo.size = (int)statbuf.st_size;
  fileInfo.type = 1;
  fileInfo.modificationTime = (time_t)statbuf.st_mtime;

  sendData(socket, &MESSAGE_FILE, sizeof(MESSAGE_FILE));
  sendData(socket, &fileInfo, sizeof(buffer_t));

  int fd = open(file_path, O_RDONLY);

  int retRead;
  char buf[500];

  memset(buf, 0, sizeof(buf));

  int remainSize = fileInfo.size;

  while (remainSize > 0) {

    int readSize;
    if (remainSize >= 500)
      readSize = 500;
    else
      readSize = remainSize;

    if ((retRead = read(fd, &buf, readSize)) < 0) {
      fprintf(stdout, "Okunamadı: %d %s\n", retRead, fileInfo.pathName);
    }
    sendData(socket, &buf, readSize);


    remainSize -= readSize;

  }
  close(fd);

  return 0;
}



int receiveFile(int socket, char * directory) {
  buffer_t fileInfo;


  if (receiveData(socket, &fileInfo, sizeof(fileInfo)) < 0)
    return -1;


  char file_path[PATH_MAX];

  sprintf(file_path, "%s/%s", directory, fileInfo.pathName);
  //printf("RECEIVE %s\n", file_path);
  

  int fd = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH );



  int retRead;
  char buf[500] = { 0 };
  int remainSize = fileInfo.size;



  while (remainSize > 0) {

    int readSize;
    if (remainSize >= 500)
      readSize = 500;
    else
      readSize = remainSize;

    if ((retRead = receiveData(socket, &buf, readSize)) < 0)
    {
      fprintf(stderr, "Failed to read clientSocket\n");
      return -1;
    }

    write(fd, &buf, readSize);
    remainSize -= readSize;
  }

  close(fd);
  return 0;

}


void sendRemoveFile(int socket,char *pathName){

  char folder[PATH_MAX] = { 0 };
  strcpy(folder, pathName);

  sendData(socket, &MESSAGE_FILE_REMOVE, sizeof(MESSAGE_FILE_REMOVE));
  sendData(socket, &folder, PATH_MAX);

}

void sendRemoveFolder(int socket, char *pathName) {



  char folder[PATH_MAX] = { 0 };
  strcpy(folder, pathName);

  sendData(socket, &MESSAGE_FOLDER_REMOVE, sizeof(MESSAGE_FOLDER_REMOVE));
  sendData(socket, &folder, PATH_MAX);

}


void sendCreateFolder(int socket, char *pathName) {


  char folder[PATH_MAX] = { 0 };
  strcpy(folder, pathName);

  sendData(socket, &MESSAGE_FOLDER, sizeof(MESSAGE_FOLDER));
  sendData(socket, &folder, sizeof(folder));
}

void multiple_mkdir(char *directory, char *pathName) {
  struct stat statbuf = {0};

  //fprintf(stdout, "MULTİPLE MKDİR %s\n", pathName );

  char folder_path[PATH_MAX];
  char * c = strchr(pathName, '/');
  while (c != NULL) {
    *c = '\0';
    sprintf(folder_path, "%s/%s", directory, pathName);
    if (stat(folder_path, &statbuf) == -1) {

      char realPath[PATH_MAX];
      realpath(folder_path, realPath);
      if (strlen(realPath) <= strlen(directory)) {
        printf("HATALI PATH: %s\n", folder_path);
        return;
      }


      // len(absolute(folder_path)) > len(directory)

      mkdir(realPath, 0700);
    }

    *c = '/';
    c = strchr(c + 1, '/');
  }

  sprintf(folder_path, "%s/%s", directory, pathName);
  if (stat(folder_path, &statbuf) == -1) {
    mkdir(folder_path, 0700);
  }

}

int receiveRemoveFolder(int socket, char *directory) {

  char pathName[PATH_MAX];

  if (receiveData(socket, &pathName, sizeof(pathName)) < 0)
    return -1;

  char folder_path[PATH_MAX];

  sprintf(folder_path, "%s/%s", directory, pathName);
  

  char realPath[PATH_MAX];
  realpath(folder_path, realPath);
  if (strlen(realPath) <= strlen(directory)) {
    printf("HATALI PATH: %s\n", folder_path);
    return -1;
  }

  multiple_rmdir(realPath);

  return 0;
}

int receiveRemoveFile(int socket, char *directory) {
  
  char pathName[PATH_MAX];

  if (receiveData(socket, &pathName, sizeof(pathName)) < 0)
    return -1;

  char folder_path[PATH_MAX];
  sprintf(folder_path, "%s/%s", directory, pathName);



  unlink(folder_path);

  return 0;
}

/*İnternetten alındı 
  https://stackoverflow.com/questions/2256945/removing-a-non-empty-directory-programmatically-in-c-or-c/42978529
*/
int multiple_rmdir(char *path){
  DIR *d = opendir(path);
  size_t path_len = strlen(path);
  int r = -1;

  if (d){
    
    struct dirent *p;
    r = 0;
    while (!r && (p = readdir(d))){
      int r2 = -1;
      char *buf;
      size_t len;

      if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
        continue;

      len = path_len + strlen(p->d_name) + 2;
      buf = malloc(len);

      if (buf){
        struct stat statbuf;
        snprintf(buf, len, "%s/%s", path, p->d_name);

        if (!stat(buf, &statbuf)){
          if (S_ISDIR(statbuf.st_mode))
            r2 = multiple_rmdir(buf);
          else
            r2 = unlink(buf);
        }
        free(buf);
      }
      r = r2;
    }
    closedir(d);
  }

  if (!r){
    r = rmdir(path);
  }

  return r;
}





int receiveCreateFolder(int socket, char *directory) {
  char pathName[PATH_MAX];

  if (receiveData(socket, &pathName, sizeof(pathName)) < 0)
    return -1;


  multiple_mkdir(directory, pathName);

  return 0;
}

time_t getModificationTime(char * path) {

  struct dirent* dent;
  DIR* srcdir = opendir(path);

  struct stat max;

  lstat(path, &max);

  time_t maxTime = (time_t)max.st_mtime;

  if (srcdir == NULL)
  {
    perror("opendir error");
    return -1;
  }

  while ((dent = readdir(srcdir)) != NULL)
  {
    struct stat st;

    if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
      continue;

    if (fstatat(dirfd(srcdir), dent->d_name, &st, 0) < 0)
    {
      perror(dent->d_name);
      continue;
    }

    if (S_ISDIR(st.st_mode)) {

      if ((time_t)st.st_mtime > maxTime)
        maxTime = (time_t)st.st_mtime;
    }
  }

  closedir(srcdir);

  //fprintf(stdout, "modificationTime :  %ld\n", maxTime );

  return maxTime;
}


