
#include "buddy.h"

#include <assert.h>
#include <math.h>
#include <memory.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/mman.h>

head *flists[LEVELS] = {NULL};

struct head *new () {
    // I opted to not use PROT_EXEC with this, since that could potentially cause security issues
    // (i.e. if you download something from the internet into the memory, and it somehow gets executed, potentially malicious)
    void *ans = mmap(NULL, PAGE, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ans == MAP_FAILED) {
        // error, return NULL.
        return NULL;
    }
    // cast it to a head pointer.
    head *headPtr = (head *)ans;

    // head h; I don't think we need to use this. Because:
    // 1. We already own the memory we're assigning to
    // 2. ans != null, so no null pointer issues
    // 3. headPtr->level is basically just doing an array[] ((int*) headPtr)[0] = MAX_LEVEL

    headPtr->level = MAX_LEVEL;
    headPtr->status = STATUS_FREE;  // intially the block is free.
    headPtr->next = NULL;
    headPtr->prev = NULL;
    return headPtr;
}

struct head *buddy(struct head *block) {
    // need to find the other one with the same level as this one.
    // We cannot find the buddy of a 4096 block, as that doesn't have a buddy.
    if (block->level == MAX_LEVEL)
        return block;  // basically a noop

    // this is the bit that is different between this block and it's buddy.
    int differentiatingBit = (1 << (block->level + MIN));

    // we first make the pointer a long long to be able to xor, and then cast back to head*
    return (head *)(((ll)block) ^ differentiatingBit);
}

struct head *split(struct head *block) {
    // the size of half the current level, i.e. 7 -> 2048, 6->1024;
    int halfSize = 1 << (block->level + MIN - 1);

    // get a new pointer to the middle of the current block
    head *newPtr = (head *)(((ll)block) ^ halfSize);

    // the level now changes, as we split
    --block->level;

    // set the level and status accordingly.
    newPtr->level = block->level;
    newPtr->status = block->status;

    return newPtr;
}

struct head *primary(struct head *block) {
    int halfSize = 1 << (block->level + MIN);  // the thing that differentiates it from it's buddy
    // we must return a pointer, which is the same as head, except the above bit must be 0.

    // ~halfSize = 1111111...0...111, with the zero in the position that differentiates it from it's buddy.
    // So if we & the above and our block, that bit will be zero, exactly the address of the primary.

    return (head *)(((ll)block) & (~halfSize));  // all will be one, except the one we need to be 0
}

// This is not strictly necessary for Part A, but I put it here nonetheless to test some stuff.
struct head *merge(head *a, head *b) {
    // first find primary
    // we must have that they are each other's buddies
    assert(buddy(a) == b);

    head *prim, *secondary;
    prim = primary(a);
    secondary = (prim == a ? b : a);

    // now merge
    // for merging, we assume the blocks are free, so it doesn't really matter
    ++prim->level;  // level increases by one.

    // now, if we were using malloc or something, we would delete our memory of secondary here.
    // But, since we're managing the memory on our own, we don't have to do that.
    // for safety, I'll just set all the bits to 0 of secondary.
    // we clear the memory, just as a precaution.
    memset(secondary, 0, sizeof(head));

    return prim;
}

void *hide(struct head *block) {
    // block + 1 will point to the next free location after the current head.
    return (void *)(block + 1);  // Then we just cast to void*.
}

struct head *magic(void *memory) {
    // this is just the reverse. of hide.
    // First cast to a head*, subtract one to get the previous head pointer.
    return ((head *)memory) - 1;
}

int level(int req) {
    // from the top down
    int l = MAX_LEVEL;

    // go until the size is at least as large as the req + head size.
    while ((1 << (l + MIN)) >= req + sizeof(head)) {
        // subtract one from l;
        --l;
    }
    // now we return the ++l, as the current value of l is just too small.
    return ++l;

    // we could also do it in the reverse way, i.e. starting from l=0, and going up.
    // But the buddy alg is traditionally from the top.
}

void dispblocklevel(struct head *block) {
    printf("block level = %d\n", block->level);
}
void dispblockstatus(struct head *block) {
    printf("block status = %d\n", block->status);
}

void blockinfo(struct head *block) {
    printf("===================================================================\n");
    dispblockstatus(block);
    dispblocklevel(block);
    printf("start of block in memory: %p\n", block);
    printf("size of block in memory: %ld in bytes\n", sizeof(struct head));
    printf("===================================================================\n");
}

/***************************************************/
// Buddy Algorithm Functions
/***************************************************/

/**
 * This finds the smallest free block in freelists[level: ].
 * If no block is found, return null.
 */
head *findSmallestFree(int level) {
    int i;
    head *temp = NULL;
    for (i = level; i < LEVELS; ++i) {
        if (flists[i] != NULL) {
            // check if there is at least one free one
            temp = flists[i];
            while (temp != NULL && temp->status == STATUS_USED) {
                // while the element is not null and used, continue
                temp = temp->next;
            }
            if (temp && temp->status == STATUS_FREE) {
                // temp is free, return it.
                break;
            } else {
                temp = NULL;
            }
        }
    }

    if (temp && temp->status == STATUS_USED) return NULL;

    return temp;
}

/**
 * This adds the node to the front of the linked list.
 * The reason for this is that adding to the front is done in constant time
 */
void addToLinkedListFront(head *node) {
    int level = node->level;
    node->prev = NULL;
    head *front = flists[level];
    if (front == NULL) {
        flists[level] = node;
        node->next = NULL;
        return;
    }
    // add to front.
    node->next = flists[level];
    flists[level] = node;
    node->next->prev = node;
}

/**
 * This deletes this node from the linked list.
 * This is done in constant time.
 */
void deleteFromLinkedList(head *node) {
    head *tempNext = node->next;
    if (node->prev) {
        node->prev->next = tempNext;
    } else {
        // the previous is NULL, so this is the front
        flists[node->level] = tempNext;
    }

    if (node->next) node->next->prev = node->prev;
    node->prev = NULL;
    node->next = NULL;
}

/**
 * This function takes the given node, and splits it as much as needed to get to the level given.
 * It also updates the free lists appropriately.
 */
head *splitNodesForLevel(int level, head *freeBlock) {
    while (level != freeBlock->level) {
        // delete freeblock from it's position in the linked list
        // Constant time.
        deleteFromLinkedList(freeBlock);
        
        // split free block
        head *splitted = split(freeBlock);
        ASSERT_DEBUG(splitted->level == freeBlock->level);
        // add the two children back into the freelist. Also constant time.
        addToLinkedListFront(freeBlock);
        addToLinkedListFront(splitted);
        // set freeBlock = second block in the split.
        freeBlock = splitted;
    }
    return freeBlock;
}

void *balloc(size_t size) {
    if (size <= 0 || size > PAGE - sizeof(head))
        return NULL;  // error if size <= 0 or if it is larger than what we can do;

    int requiredLevel = level(size);
    head *freeBlock = findSmallestFree(requiredLevel);

    if (freeBlock == NULL) {
        // then we have to add a new block at the top.
        freeBlock = new ();

        addToLinkedListFront(freeBlock);
    }

    // now we need to split this current node so that it works correctly.
    head *nodeToGive = splitNodesForLevel(requiredLevel, freeBlock);

    nodeToGive->status = STATUS_USED;  // we are using this block now.
    return hide(nodeToGive);
}

/**
 * This attempts to merge the node with its buddy and recursively calls the function again.
 */
void possiblyMergeAsFarUpAsPossibleRecursive(head *node) {
    if (node->level == MAX_LEVEL) return;  // cannot merge the large one.
    head *bud = buddy(node);
    if (bud->status == STATUS_FREE && bud->level == node->level) {
        // first delete both of these from the list. Constant time.
        deleteFromLinkedList(bud);
        deleteFromLinkedList(node);
        // then merge
        head *merged = merge(node, bud);
        merged->status = STATUS_FREE;

        // now, we could do some stuff to optimize this, and not add it to the linked list if it's
        // buddy is free. This won't help too much, as adding/deleting is done in constant time.
        addToLinkedListFront(merged);    
        possiblyMergeAsFarUpAsPossibleRecursive(merged);
    }
    // if the status of the buddy isn't free, then we can't do anything.
}

void bfree(void *memory) {
    if (memory == NULL) return;

    // get the head
    head *node = magic(memory);
    // make it free
    node->status = STATUS_FREE;
    
    // potentially merge up to the top
    possiblyMergeAsFarUpAsPossibleRecursive(node);
}
/**
 * This function frees all of the memory and returns it to the operating system.
 * It's mostly used for tesing purposes, to get a clean slate every time.
 */
void freeAll(){
    for (int level=0; level < LEVELS-1; ++level){
        head* node = flists[level];
        while (node != NULL && node->status != STATUS_USED){
            node = node->next;
        }
        if (node != NULL){
            head* tmp = node;
            bfree(hide(tmp));
            level--;
            continue;
        }
        assert(flists[level] == NULL);
    }
    head* node = flists[MAX_LEVEL];
    while (node != NULL){
        head* tmp = node;
        node = node->next;
        deleteFromLinkedList(tmp); // delete from list
        munmap(tmp, PAGE); // free memory to os
    }

    assert(flists[MAX_LEVEL] == 0);
}