/* filesys.c
 * 
 * provides interface to virtual disk
 * 
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "filesys.h"


diskblock_t  virtualDisk [MAXBLOCKS] ;           // define our in-memory virtual, with MAXBLOCKS blocks
fatentry_t   FAT         [MAXBLOCKS] ;           // define a file allocation table with MAXBLOCKS 16-bit entries
fatentry_t   rootDirIndex            = 0 ;       // rootDir will be set by format
direntry_t * currentDir              = NULL ;
fatentry_t   currentDirIndex         = 0 ;

dirblock_t* lastTraverseResultParent = 0;
int lastTraverseResultEntryIndex = 0;
int lastTraverseResultIndex    = 0;

/* writedisk : writes virtual disk out to physical disk
 * 
 * in: file name of stored virtual disk
 */

void writedisk ( const char * filename )
{
   printf ( "writedisk> virtualdisk[0] = %s\n", virtualDisk[0].data ) ;
   FILE * dest = fopen( filename, "w" ) ;
   if ( fwrite ( virtualDisk, sizeof(virtualDisk), 1, dest ) < 0 )
      fprintf ( stderr, "write virtual disk to disk failed\n" ) ;
   //write( dest, virtualDisk, sizeof(virtualDisk) ) ;
   fclose(dest) ;
   
}

void readdisk ( const char * filename )
{
   FILE * dest = fopen( filename, "r" ) ;
   if ( fread ( virtualDisk, sizeof(virtualDisk), 1, dest ) < 0 )
      fprintf ( stderr, "write virtual disk to disk failed\n" ) ;
   //write( dest, virtualDisk, sizeof(virtualDisk) ) ;
      fclose(dest) ;
}


/* the basic interface to the virtual disk
 * this moves memory around
 */

void writeblock ( diskblock_t * block, int block_address )
{
   //printf ( "writeblock> block %d = %s\n", block_address, block->data ) ;
   memmove ( virtualDisk[block_address].data, block->data, BLOCKSIZE ) ;
   //printf ( "writeblock> virtualdisk[%d] = %s / %d\n", block_address, virtualDisk[block_address].data, (int)virtualDisk[block_address].data ) ;
}


/* read and write FAT
 * 
 * please note: a FAT entry is a short, this is a 16-bit word, or 2 bytes
 *              our blocksize for the virtual disk is 1024, therefore
 *              we can store 512 FAT entries in one block
 * 
 *              how many disk blocks do we need to store the complete FAT:
 *              - our virtual disk has MAXBLOCKS blocks, which is currently 1024
 *                each block is 1024 bytes long
 *              - our FAT has MAXBLOCKS entries, which is currently 1024
 *                each FAT entry is a fatentry_t, which is currently 2 bytes
 *              - we need (MAXBLOCKS /(BLOCKSIZE / sizeof(fatentry_t))) blocks to store the
 *                FAT
 *              - each block can hold (BLOCKSIZE / sizeof(fatentry_t)) fat entries
 */

/* implement format()
 */
void format() {
   diskblock_t block ;

   /* prepare block 0 : fill it with '\0',
    * use strcpy() to copy some text to it for test purposes
	* write block 0 to virtual disk
	*/
	block = emptyBlock();
  volumeblock_t vol = block.vol;
	strcpy(vol.name, "CS3026 Operating Systems Assessment");
  
	writeblock(&vol, 0);

	/* prepare FAT table
	 * write FAT blocks to virtual disk
	 */
	for (int i = 0; i < MAXBLOCKS; i++) {
		FAT[i] = UNUSED;
	}
	FAT[0] = ENDOFCHAIN;
	FAT[1] = 2;
	FAT[2] = ENDOFCHAIN;
	copyFAT();

	 /* prepare root directory
	  * write root directory block to virtual disk
	  */
	block = emptyBlock();
	block.dir.isdir = 1;
	block.dir.nextEntry = 0;
	writeblock(&block, 3);
	
	//Set up root index block
	FAT[3] = ENDOFCHAIN;
	copyFAT();
	rootDirIndex = 3;
	currentDirIndex = 3;
}

diskblock_t emptyBlock() {
	diskblock_t empty;
	//instantiate empty block and fill all the data with empty chars
	for(int i = 0; i < BLOCKSIZE; i++) {
		empty.data[i] = '\0';
	}
	return empty;
}

void copyFAT() {
	//Create the two fat blocks needed
	diskblock_t block1 = emptyBlock();
  diskblock_t block2 = emptyBlock();
  
	//assign values from the FAT array to block 1, up to FATENTRYCOUNT, or half the values
  for (int i = 0; i < FATENTRYCOUNT; i++) {
    block1.fat[i] = FAT[i];
  }
	//assign the other half, by accessing half+i of the FAT array
  for (int i = 0; i < FATENTRYCOUNT; i++) {
    block2.fat[i] = FAT[FATENTRYCOUNT + i];
  }
	//write these blocks to the virtual disk
  writeblock(&block1, 1);
  writeblock(&block2, 2);
}

void writeVirtualDisk(char* name) {
	//open real file stream
	FILE *f = fopen(name, "wb");
	//write the virtual disk binary data
	fwrite(&virtualDisk, sizeof(diskblock_t), sizeof(virtualDisk), f);
	fclose(f);
}

MyFILE* myfopen(const char* filename, const char* mode) {
  if (!(strcmp(mode, "r") == 0 || strcmp(mode, "w") == 0)) {
    printf("File mode not valid\n");
    return NULL;
  }
  printf("finding dir\n");
	//Find the file on the file system, and passing TRUE to create if it does not exist.
  dirblock_t* dir = traverseToDir(filename, TRUE);
  
  if (dir == NULL) {
    return NULL;
  }
  
  printf("found dir\n");
  
	//get the index of this file
  int fileBlockNo = lastTraverseResultIndex;
	//allocate space for the virtual file
  MyFILE* file = malloc(sizeof(MyFILE));
	//setup the virtual files members
  file->blockno = fileBlockNo;
  file->buffer = virtualDisk[fileBlockNo];
  
	//assign the file mods
  strcpy(file->mode, mode);
  file->pos = 0; //Set the starting pos for the file stream
  return file;
}

void myfputc(int b, MyFILE* stream) {
   //printf("block pos: %d - stream pos: %d\n", stream->blockno, stream->pos);
   //initial block of the file
   diskblock_t* buffer = &stream->buffer;
   //using the current char pos from the file struct, 
   //we assign b to the next array position
   buffer->data[stream->pos] = b;
   stream->pos++; //increment stream position for the next call
  
   if (stream->pos >= BLOCKSIZE) {
     printf("Extending file block chain - stream pos:%d\n", stream->pos);
     //flush whats currently written to the virtual disk
     writeblock(buffer, stream->blockno);
     
     int freeBlockPos;
     //find a free block pos
     for (int i = 4; i < MAXBLOCKS; i++) {
         if (FAT[i] == UNUSED) {
           freeBlockPos = i;
           break;
         }
     }
     
     //Update current pos to point to the next block for this file
     FAT[stream->blockno] = freeBlockPos;
     //make sure the new pos is the end of the current chain
     FAT[freeBlockPos] = ENDOFCHAIN;
     copyFAT();
     
     //update stream struture values
     stream->blockno = freeBlockPos;
     stream->pos = 0;
     stream->buffer = virtualDisk[freeBlockPos];
   }
}

int myfgetc(MyFILE* stream) {
	//set up the buffer objects
  diskblock_t* buffer = &stream->buffer;
  int b = buffer->data[stream->pos];
  stream->pos++;
  
  if (stream->pos >= BLOCKSIZE) {
    //goto next block
    stream->blockno = FAT[stream->blockno];
    stream->pos = 0;
    stream->buffer = virtualDisk[stream->blockno];
    printf("\nJumping to next block:%d - stream pos:%d\n", stream->blockno, stream->pos);
  }
  return b;
}

void myfclose(MyFILE* stream) {
  //closes the file and write out any blocks not written to the disk yet
  writeblock(&stream->buffer, stream->blockno);
  free(stream);
}

dirblock_t* traverseToDir(char* path, int createWhenNotFound) {
  //if the path starts with a slash, assume initial block is root, otherwise
  //use current dir reference
  diskblock_t* block;
  if (path[0] == '/') {
    printf("searching by absolute path!\n");
    block = &virtualDisk[rootDirIndex];
  } else {
    printf("searching by relative path!\n");
    if (currentDir == NULL) {
      currentDir = &virtualDisk[rootDirIndex];
    }
    block = currentDir;
  }
  
  //Only travel down the tree if we are not listing the root
  if (strcmp(path, "/") > 0) {
    int isdir;
    char* charArray[strlen(path)];
    strcpy(charArray, path);
    char* dir = strtok(charArray, "/");
    
    while(1) {
      //Check if the current dir name has a ., meaning its a file
      int isdir;
      if (strstr(dir, ".") != NULL) {
        //if the next token is a file, break out of the loop and return the last found dirblock
        isdir = 0;
      } else {
        isdir = 1;
      }
      
      int existingPos = doesExist(block, dir);
      if (existingPos != -1) {
				//Already exists, does not need to be created
				//Assign global vars to be used after this function is called
				//finally assign block to the reference of the firstblock of the found entrylist entry
        lastTraverseResultParent = block;
        lastTraverseResultIndex = block->dir.entrylist[existingPos].firstblock;
        lastTraverseResultEntryIndex = existingPos;
        block = &virtualDisk[block->dir.entrylist[existingPos].firstblock];
      } else {
        if (createWhenNotFound) {
          int newDirBlockPos = createDirBlock(block, isdir);
          //Block has now been created, now to link it to the parent dir!!
          int addToEntryResult = addDirBlockToEntryList(block, newDirBlockPos, isdir, dir);
          if (addToEntryResult == -1) {
            return NULL;
          }
          
					//assign global vars to be used after this fucntion is calld
          lastTraverseResultParent = block;
          lastTraverseResultIndex = newDirBlockPos;
          lastTraverseResultEntryIndex = addToEntryResult;
          
					//return a referene to the new block position that was just created
          block = &virtualDisk[newDirBlockPos];
        } else {
          printf("Path does not exist!\n");
          return NULL; 
        }
      }

      dir = strtok(NULL, "/");
			//if no more tokens, break out of the loop
      if (!dir) {
        break;
      }
    }
  }
 
  return block;
}

int getFreeFATPos() {
  //find a free block pos in the FAT table
  int freeBlockPos = -1;
  for (int i = 4; i < MAXBLOCKS; i++) {
     if (FAT[i] == UNUSED) {
       freeBlockPos = i;
       break;
     }
  }
  return freeBlockPos;
}

int createDirBlock(dirblock_t* parent, int isdir) {
  //create the new empty directory block
  diskblock_t newBlock = emptyBlock();
  newBlock.dir.isdir = isdir;
  newBlock.dir.nextEntry = 0;
  
  int freeBlockPos = getFreeFATPos();
  
	//create the "current dir" entry
  newBlock.dir.entrylist[0].isdir = TRUE;
  newBlock.dir.entrylist[0].used = TRUE;
  newBlock.dir.entrylist[0].firstblock = freeBlockPos;
  newBlock.dir.entrylist[0].name[0] = '.';
  
	//Find the index of the parent of this block
  int parentPos = 0;
  for (int i = 4; i < MAXBLOCKS; i++) {
     if (&virtualDisk[i] == parent) {
       parentPos = i;
       break;
     }
  }
  
	//create the "parent dir" entry
  printf("parentPos: %d\n", parentPos);
  newBlock.dir.entrylist[1].isdir = TRUE;
  newBlock.dir.entrylist[1].used = TRUE;
  newBlock.dir.entrylist[1].firstblock = parentPos;
  newBlock.dir.entrylist[1].name[0] = '.';
  newBlock.dir.entrylist[1].name[1] = '.';
  
  //insert the new block and update the FAT table
  FAT[freeBlockPos] = ENDOFCHAIN;
  writeblock(&newBlock, freeBlockPos);
  copyFAT();
  
  return freeBlockPos;
}

int addDirBlockToEntryList(dirblock_t* parent, int newDirBlockPos, int isdir, char* name) {
  if (parent->nextEntry >= DIRENTRYCOUNT) {
    printf("cannot add more directory entries to this block");
    return -1;
  }
  
  //Find the next free position in the entry list
  int nextFreeParentEntryListPos = -1;
  for (int i = 0; i < DIRENTRYCOUNT; i++) {
    if (parent->entrylist[i].used == FALSE) {
      nextFreeParentEntryListPos = i;
      break;
    }
  }
  
  if (nextFreeParentEntryListPos == -1) {
    printf("Could not find entryListPos for %s\n", name);
    return -1;
  }
  
  //add this new dir to the current block
  parent->nextEntry++;
  parent->entrylist[nextFreeParentEntryListPos].isdir = isdir;
  parent->entrylist[nextFreeParentEntryListPos].used = TRUE;
  parent->entrylist[nextFreeParentEntryListPos].firstblock = newDirBlockPos;
  strcpy(parent->entrylist[nextFreeParentEntryListPos].name, name);
  
  return nextFreeParentEntryListPos;
}

void mymkdir(char* path) {
  traverseToDir(path, TRUE);
}

char** mylistdir(char* path) {
  dirblock_t* block;
  if (strcmp(path, ".") == 0) { //Use current dir
    printf("listing current!\n");
		//assign a reference to the current dir by using the current dir index
    block = &virtualDisk[currentDirIndex];
  } else if (strcmp(path, "..") == 0) { //Use parent dir  
		//get the parent dir index, and then return a reference to that block from the virtual disk
    printf("listing parent!\n");
    int parentIndex = virtualDisk[currentDirIndex].dir.entrylist[1].firstblock;
    block = &virtualDisk[parentIndex];
  } else { //Otherwise, find the dir
    printf("listing path!\n");
		//If none of these cases math, traverse the dir structre as normal
    block = traverseToDir(path, FALSE);
  }
  
  if (block == NULL) {
    return NULL;
  }

  //Build list of directories
  int index;
  char** list = malloc(sizeof(char*)*DIRENTRYCOUNT);
  char** temp = list;
	//For every dir in the entrylist, append this to the list of pointers, only if its in use
  for (int i = 0; i < DIRENTRYCOUNT; i++) {
    if (block->entrylist[i].used == TRUE && strlen(block->entrylist[i].name) > 0) {
      *temp = block->entrylist[i].name;
      temp++;
    }
  }
  *temp = NULL;
  
  return list;
}

int doesExist(diskblock_t* block, char* name) {
  //Loop over all possible entries in the root directory
  for (int i = 0; i < DIRENTRYCOUNT; i++) {
    //Skip unused entries, as these cannot be the existing sub dir
    if (block->dir.entrylist[i].used == FALSE) {
      continue;
    }
    //If the current entry matches the filename we're looking for, then it exists
    if (strcmp(block->dir.entrylist[i].name, name) == 0) {
      return i;
      break;
    }
  }
  return -1;
}

void deleteDirBlock(int index) {
  //Safe guard block 0 from being deleted
  if (index == 0) {
    return;
  }
	//write an empty block to the index on the virtual disk
  diskblock_t empty = emptyBlock();
  writeblock(&empty, index);
}

void deleteDirBlockFromEntryList(dirblock_t* parent, dirblock_t* entry) {
  int entryListPos = -1;
  for (int i = 0; i < DIRENTRYCOUNT; i++) {
    if (parent->entrylist[i].used == FALSE) {
      continue;
    }
    //If the current entry matches the filename we're looking for, then it exists
    if (&virtualDisk[parent->entrylist[i].firstblock] == entry) {
      entryListPos = i;
      break;
    }
  }
  if (entryListPos == -1) {
      printf("Could not find dir to delete\n");
  }
  
  //Reset entry list position
  parent->nextEntry--;
  if (parent->nextEntry < 0) {
    parent->nextEntry = 0;
  }
	
	//Reset all members of this entrylist object
  parent->entrylist[entryListPos].entrylength = 0;
  parent->entrylist[entryListPos].isdir = 0;
  parent->entrylist[entryListPos].used = 0;
  parent->entrylist[entryListPos].modtime = 0;
  parent->entrylist[entryListPos].filelength = 0;
  parent->entrylist[entryListPos].firstblock = 0;
  memset(parent->entrylist[entryListPos].name, 0, sizeof(parent->entrylist[entryListPos].name));
}

void mychdir(char* path) {
  currentDir = traverseToDir(path, FALSE);
  currentDirIndex = lastTraverseResultIndex;
}

void myremove(char* path) {
  dirblock_t* file = traverseToDir(path, FALSE);
  dirblock_t* parent = lastTraverseResultParent;
  
  //Make sure we are working with a file
  if (file->isdir == 1) {
    printf("path is not a file!\n");
    return;
  }
  
  //Get the starting block for this dir
  int firstBlock;
  for (int i = 0; i < DIRENTRYCOUNT; i++) {
    if (parent->entrylist[i].used == FALSE) {
      continue;
    }
    //Check if the current entry entry list has an equal pointer to the passed file pointer
    if (&virtualDisk[parent->entrylist[i].firstblock] == file) {
      firstBlock = parent->entrylist[i].firstblock;
      break;
    }
  }
  
  //Reset the block chain in the fat table
  int cur = firstBlock;
  while (1) {
    int last = cur;
    //Delete the block from the virtualDisk
    deleteDirBlock(cur);
    
    //Reset this position on the FAT
    int next = FAT[cur];
    FAT[cur] = UNUSED;
    cur = next;
    
    if (last == ENDOFCHAIN) {
      break;
    }
  }
  copyFAT();
  
  deleteDirBlockFromEntryList(lastTraverseResultParent, file);
}

void myrmdir(char* path) {
  dirblock_t* dir = traverseToDir(path, FALSE);
  
  if (dir == NULL) {
    return;
  }
  
  //Make sure we are working with a directory
  if (dir->isdir == 0) {
    printf("path is not a directory!\n");
    return;
  }
  
  //Check if the directory is empty
  if (dir->nextEntry != 0) {
    printf("directory is not empty!\n");
  }
  
  deleteDirBlockFromEntryList(lastTraverseResultParent, dir);
}

void mycpy(char* source, char* destination) {
	//open a virtual stream for both the source and destination
  MyFILE* srcFile = myfopen(source, "r");
  MyFILE* destFile = myfopen(destination, "w");
	
	//escape if any of these could not be opened
  if (srcFile == NULL || destFile == NULL) {
    printf("Could not open file\n");
    return;
  }
	//For every char in the source stream, append it to the destination
  while (1) {
    int b = myfgetc(srcFile);
    if (b == EOF_) {
      break;
    }
    myfputc(b, destFile);
  }
  myfputc(EOF_, destFile);
  
	//make sure to close both streams
  myfclose(srcFile);
  myfclose(destFile);
}

void mymove(char* source, char* dest) {
	//Copy the data from the source to the destination, then remove the source
  mycpy(source, dest);
  myremove(source);
}

void loadRealFile(char* source, char* dest) {
	//Open a real file stream
  FILE *src = fopen(source, "r");
  if (src) {
		//Open a virtual file stream
    MyFILE *file = myfopen(dest, "w");
    char c;
		//append a char from the real stream to the virtual stream until we hit the EOF char
    while ((c = fgetc(src)) != EOF) {
      myfputc(c, file);
    }
    fclose(src);
    myfclose(file);
  }
}

direntry_t* locate(char* startingPath, char* filename) {
  //Initiall traverse to the starting directory and search all sub directories
  dirblock_t* start = traverseToDir(startingPath, FALSE);
  direntry_t* result = searchDir(start, filename);
  return result;
}

direntry_t* searchDir(dirblock_t* dir, char* filename) {
	//loop over all entires in the passed starting dir
	//skip any entires that are the "." or ".." directories and also any unused entries.
  for (int i = 0; i < DIRENTRYCOUNT; i++) {
    if (strcmp(dir->entrylist[i].name, ".") == 0 || strcmp(dir->entrylist[i].name, "..") == 0) {
      continue;
    }
    if (dir->entrylist[i].used == 0) {
      continue;
    }
		//string compare the current entry file name and return its pointer if its a match
    if (strcmp(dir->entrylist[i].name, filename) == 0) {
      return &dir->entrylist[i];
    } 
		//Otherwise, if its another directory, recursively call searchDir
		else if (dir->entrylist[i].isdir == 1) {
      return searchDir(&virtualDisk[dir->entrylist[i].firstblock].dir, filename);
    }
  }
}

pthread_mutex_t* getLockVar() {
	//return a reference to the locking var from the first block of the file system
  diskblock_t block = virtualDisk[0];
  pthread_mutex_t* lock = &block.vol.lock;
  return lock;
}

/* use this for testing
 */

void printDirBlock(dirblock_t* dir) {
  if (dir == NULL) {
    dir = &virtualDisk[rootDirIndex];
  }
  
  printf("-------------\n");
  for (int i = 0; i < DIRENTRYCOUNT; i++) {
    if (dir->entrylist[i].used == FALSE) continue;
    char* name = dir->entrylist[i].name;
    
    printf("%d-%s dir:%d first:%d\n", i, name, dir->entrylist[i].isdir, dir->entrylist[i].firstblock);
  }
}

void printBlock ( int blockIndex )
{
   printf ( "virtualdisk[%d] = %s\n", blockIndex, virtualDisk[blockIndex].data ) ;
}

