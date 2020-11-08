
#ifndef buddy_h
# define buddy_h
#include <stdbool.h>
#include <stddef.h>
// 4KB
#define PAGE 1024 * 4

// Smallest page size is 2^MIN = 32
#define MIN 5

// The number of levels, 0 to 7.
#define LEVELS 8
// the max level we can give.
#define MAX_LEVEL LEVELS - 1

// if a block is free
#define STATUS_FREE 0
// a block is used.
#define STATUS_USED 1

// some debug stuff
#define DEBUG_MODE 1
#ifdef DEBUG_MODE
    #define ASSERT_DEBUG(x) assert(x)
#else
    #define ASSERT_DEBUG(x) 
#endif

typedef struct head head;
typedef unsigned long long ll;

// put the definition of head in the .h file, so it is visible in the main function
struct head{
    int level; // the level of the block 0 is smallest (32), 7 is largest(4096)
    bool status; // free or taken
    head* prev;
    head* next;
};

struct head * flists[LEVELS];

struct head *new();
struct head *buddy(struct head*);
struct head *split(struct head*);
struct head *primary(struct head*);

struct head *merge(head*, head*);

void *hide(struct head*);
struct head *magic(void*);
int level(int);

void dispblocklevel(struct head*);
void dispblockstatus(struct head*);
void blockinfo(struct head*);


/***************************************************/
// Buddy Algorithm Functions
/***************************************************/
void *balloc(size_t size);
void bfree(void *memory);

void freeAll();
#endif


