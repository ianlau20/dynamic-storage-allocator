# dynamic-storage-allocator
Dynamic Storage Allocator for C Programs

This project is a C storage allocator which does not use any memory-management-related library calls or system calls. The file is adapted from mm-naive.c, the least memory-efficient malloc package provided by University of Utah Computer Science.

In my approach, memory blocks have packed headers & footers and are coalesced, freed, and reused. A block is allocated by using an explicit free list, adding pages as needed. Pages are unmapped when emptied.

Provided is one C file intended to serve as a code sample and not meant to be run outside of the project.
