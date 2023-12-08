#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "filesys.h"

pthread_t tid[2];

void genTestBytes(MyFILE* stream, int blocks) {
  //insert a text of size 4kb
  char* str = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  int counter = 0;
  for (int i = 0; i < (blocks*BLOCKSIZE); i++) {
    if (counter >25) {
      counter = 0;
    }
    myfputc(str[counter], stream);
    counter++;
  }
  
  myfputc(EOF_, stream);
}

void readTestBytes(MyFILE* stream) {
  FILE *f = fopen("testfileC3_C1_copy.txt", "w");
  printf("File contents start:\n");
   while(1) {
     int b = myfgetc(stream);
     if (b == EOF_) {
       break;
     }
     printf("%c", b); 
     fputc(b, f);
   }
  printf("\nFile contents end:\n");
  fclose(f);
}

void printDir(char** list) {
 while (*list != NULL) {
     printf("%s\n", *list);
     list++;
  } 
}

void writeFile(char* path, int blocks) {
  //writing the test bytes
  MyFILE* file = myfopen(path, "w");
  if (!file) {
    printf("Could not open file\n");
    return;
  }
  
  //printf("starting block no: %d\n", file->blockno);
  genTestBytes(file, blocks);
  myfclose(file);
}

void readFile() {
  //reading the test bytes
  MyFILE* file = myfopen("testfile.txt", "r");
  if (!file) {
    printf("Could not open file\n");
    return;
  }
  printf("starting block no: %d\n", file->blockno);
  readTestBytes(file);
}

void printList(char** list) {
  if (list == NULL) {
    return;
  }
  while (*list != NULL) {
    printf("--%s\n", *list);
    list++;
  }
}

void* threadCallback(void *arg) {
  pthread_mutex_t* lock = getLockVar();
  pthread_mutex_lock(lock); //Lock the mutex inside the virtual file system
  
  //Manipulations on the file system are now thread safe!
  writeFile("/firstdir/seconddir/testfile1.txt", 4);
  printList(mylistdir("/firstdir/seconddir"));
  myremove("/firstdir/seconddir/testfile1.txt");
  
  pthread_mutex_unlock(lock); //relaese the virtual file system lock
  return NULL;
}

void initThreads() {
  int i = 0;
  int err;
  
  pthread_mutex_t* lock = getLockVar();
  
  if (pthread_mutex_init(lock, NULL) != 0) {
    printf("Mutex init failed\n");
    return;
  }
  
  printf("LOCK VAR %p\n", lock);
  
  while (i < 2) {
    err = pthread_create(&(tid[i]), NULL, &threadCallback, NULL);
    if (err != 0) {
      printf("Could not create thread: %s", err);
    }
    i++;
  }
  
  pthread_join(tid[0], NULL);
  pthread_join(tid[1], NULL);
  pthread_mutex_destroy(lock);
}

void tests() {
  format();
  
  printf("1:----------------\n");
  mymkdir("/firstdir/seconddir");
  writeFile("/firstdir/seconddir/testfile1.txt", 4);
  printList(mylistdir("/firstdir/seconddir"));
  
  printf("2:----------------\n");
  mychdir("/firstdir/seconddir");
  printList(mylistdir("."));
  
  printf("3:----------------\n");
  writeFile("testfile2.txt", 1);
  mymkdir("thirddir");
  writeFile("thirddir/testfile3.txt", 1);
  printList(mylistdir("thirddir"));
  
  writeVirtualDisk("virtualdiskA5_A1_a");
  
  printf("4:----------------\n");
  myremove("testfile1.txt");
  myremove("testfile2.txt");
  printList(mylistdir("."));
  
  writeVirtualDisk("virtualdiskA5_A1_b");
  
  printf("5:----------------\n");
  mychdir("thirddir");
  myremove("testfile3.txt");
  printList(mylistdir("."));
  
  writeVirtualDisk("virtualdiskA5_A1_c");
  
  printf("6:----------------\n");
  mychdir("/firstdir/seconddir");
  myrmdir("thirddir");
  mychdir("/firstdir");
  myrmdir("seconddir");
  mychdir("/");
  myrmdir("firstdir");
  printList(mylistdir("/"));
  
  writeVirtualDisk("virtualdiskA5_A1_d"); 
}

int main() {
  format();
  
  mymkdir("/firstdir/seconddir");
  
  //init threads for A1
  initThreads();
  
  //Tests for A5-A4
  tests();
  
  //Tests for A3-A1
  writeFile("/firstdir/seconddir/testfile1.txt", 4);
  printList(mylistdir("/firstdir/seconddir"));
  
  direntry_t* file = locate("/", "testfile1.txt");
  printf("file: %s\n", file->name);

  mymkdir("/firstdir/seconddir/testfile2.txt");
  
  mycpy("/firstdir/seconddir/testfile1.txt", "/firstdir/seconddir/testfile2.txt");
  myremove("/firstdir/seconddir/testfile2.txt");
  
  mymove("/firstdir/seconddir/testfile1.txt", "/firstdir/seconddir/testfile1.txt");
  
  return 0;
}