#ifndef FUNCTIONS_H_INCLUDED
#define FUNCTIONS_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define PATH_MAX 4096


typedef struct {
	int type; // type 0 == folder type 1 = file type 2 = end
	int size;
	time_t modificationTime;
	char pathName[PATH_MAX];
}buffer_t;


typedef struct {
  buffer_t * array;
  int size;
  int count;
} FileList_t;

int sendData(int fd, const void * buf, int size);
int receiveData(int fd, void * buf, int size);

void fileList_add(FileList_t * a, char * pathName, int size, long int modificationTime, int type);
void fileList_init(FileList_t * a);
void fileList_free(FileList_t * a);
int fileList_search(FileList_t * a, char * pathName);

void depthFirstApply (char *path, FileList_t * array, char *mainFolder);

int sendFile(char *pathName, char *directory, int socket);

int receiveFile(int socket, char * directory);
void multiple_mkdir(char *directory, char *pathName);

void sendCreateFolder(int socket, char *pathName);
int receiveCreateFolder(int socket, char *directory);


time_t getModificationTime(char * file_path);
void setModificationTime(char * file_path);

void sendRemoveFolder(int socket, char *pathName);
int receiveRemoveFolder(int socket, char *directory);

int receiveRemoveFile(int socket, char *directory);
void sendRemoveFile(int socket,char *pathName);

int multiple_rmdir(char *path);


extern const int MESSAGE_REQUEST_FILE_LIST;
extern const int MESSAGE_FILE_LIST;
extern const int MESSAGE_FILE_REQUEST;
extern const int MESSAGE_FILE;
extern const int MESSAGE_FOLDER;
extern const int MESSAGE;
extern const int MESSAGE_FOLDER_REMOVE;
extern const int MESSAGE_FILE_REMOVE;


#endif
