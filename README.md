# Virtual-FAT16-Disk

Implementation of a simple file system that allows you to manage files and directories in a
virtual in-memory disk. The file system is based on simplified concepts of a File Allocation Table (FAT).
The implementation allows the creation of files and directories within this virtual hard disk and the
performance of simple read-and-write operations on files.
There are interface functions for creating files and directories and for reading and writing
operations. The virtual disk will be simulated by an array of memory blocks, where each
block is a fixed array of bytes. Each block has a block number (from 0 to MAXBLOCKS-1). The allocation of a new
block to a file is recorded in the FAT. The FAT is a table (an array of integers) of size MAXBLOCKS that acts as a
kind of block directory for the complete disk content: it contains an entry for each block and records whether
this block is allocated to a file or unused. The FAT itself is also stored on this virtual disk at a particular location. 
