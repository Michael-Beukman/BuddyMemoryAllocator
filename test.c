#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "buddy.h"

int testsPartA();
int testsPartB();

int oldTest();
bool testNew();
bool testBuddy();
bool testPrimary();
bool testHideMagic();
bool testLevel();

// Part b

bool testBalloc();
bool testBFree();
bool testBallocAndBFree();
void benchmark();
void cleanFreeList();
int requestSmall();
int requestLarge();
int requestExp();
int requestRandom();
void doMemoryAllocs(void *func(size_t), void freeFunc(void*), int request(void), int buffSize);
void bufferBenchmarks(void *func(size_t), void freeFunc(void*), char* name);

bool verbose = false;
void strRev(char *s) {
    int count = 0;
    while (s[count] != '\0')
        ++count;
    int end = count - 1;

    for (int a = 0; a < count / 2; ++a) {
        char c = s[a];
        s[a] = s[end];
        s[end] = c;
        end--;
    }
}

void printBin(int l) {
    char s[40];
    int j = 0;
    int counts = 0;
    while (l) {
        int i = l % 2;
        if (i)
            s[j] = '1';
        else
            s[j] = '0';
        l /= 2;
        counts++;
        if ((j % 5 == 0 && l))
            s[++j] = ' ';
        ++j;
    }
    s[j] = '\0';
    strRev(s);
    char tmp[16] = "";
    int begin = j;
    for (; counts < 13; ++j) {
        tmp[j - begin] = '0';
        counts++;
        if (j % 5 == 0 && counts < 13) {
            tmp[j + 1 - begin] = ' ';
            j++;
        }
    }
    tmp[j - begin] = '\0';
    strRev(tmp);
    printf("%s%s\n", tmp, s);
}
int lowest_bits(int num, long l) {
    int mask = (1 << (num)) - 1;
    return mask & l;
}

int main() {
    testsPartA();
    freeAll();
    cleanFreeList();
    testsPartB();
}

// I decided to put the test functions in this file, as the starter code had it like that.
// Also, it's cleaner to separate the actual buddy code from the testing stuff.

// These tests passed on a WSL machine (ubuntu), a proper linux machine (ubuntu) and a mac OSX machine.
int testsPartA() {
    testNew();
    printf("--------------------------------\n");
    testBuddy();
    printf("--------------------------------\n");
    testPrimary();
    printf("--------------------------------\n");
    testHideMagic();
    printf("--------------------------------\n");
    testLevel();
    printf("--------------------------------\n");

    printf("\n\n--------------------------------\n");
    printf("All tests for part A are successful\n");
    printf("--------------------------------\n\n\n");
}

int testsPartB() {
    // here we need to free the memory and clean the free list between tests,
    // as each test assumes a clean slate.
    testBalloc();
    printf("--------------------------------\n");
    freeAll();
    cleanFreeList();
    testBFree();
    printf("--------------------------------\n");
    freeAll();
    cleanFreeList();
    testBallocAndBFree();
    printf("--------------------------------\n");
    freeAll();
    cleanFreeList();
    benchmark();
    printf("\n\n--------------------------------\n");
    printf("All tests for part B are successful\n");
    printf("--------------------------------\n");
}

void assertBallocStuffSmall(bool checkZero, head *nodeJustAllocated) {
    // This function asserts that there is 1 node in each of the flist slots.
    // It also checks flists[0] if checkZero is true
    for (int i = 1; i < MAX_LEVEL; ++i) {
        assert(flists[i] != NULL);
        assert(flists[i]->prev == NULL);
        assert(flists[i]->next == NULL);
        assert(flists[i]->status == STATUS_FREE);
        assert(flists[i]->level == i);
    }
    if (checkZero && nodeJustAllocated) {
        // now there needs to be 2 at level 0
        head *a = flists[0];
        assert(a != NULL);
        head *b = a->next;
        assert(b != NULL);
        assert(b->prev == a);
        assert(a->prev == NULL);
        assert(b->next == NULL);
        assert(b->level == 0);
        assert(a->level == 0);

        assert(a->status == STATUS_FREE || b->status == STATUS_FREE);
        assert(a->status == STATUS_USED || b->status == STATUS_USED);

        head *usedOne = a->status == STATUS_USED ? a : b;
        assert(hide(usedOne) == nodeJustAllocated);
        assert((usedOne) == magic(nodeJustAllocated));
    }
}

bool testBalloc() {
    void *temp = balloc(2048);  // has to be top most level.
    assert(temp != NULL);
    assert(flists[MAX_LEVEL] != NULL);
    assert(flists[MAX_LEVEL]->status == STATUS_USED);
    assert(hide(flists[MAX_LEVEL]) == temp);
    assert(flists[MAX_LEVEL]->level == MAX_LEVEL);
    for (int i = 0; i < MAX_LEVEL; ++i) {
        assert(flists[i] == NULL);
    }

    void *tempSmall = balloc(7);  // 7 + 24 < 31
    // now, there has to be one head* in each of the MAX_LEVEL. In flists[0] there has to be 2, 
    // a free one and an used one.

    assertBallocStuffSmall(true, tempSmall);
    void* shouldBeNull = balloc(0);
    assert(shouldBeNull == NULL);
    shouldBeNull = balloc(4090);
    assert(shouldBeNull == NULL);
    printf("Balloc is successful\n");

}

bool testBFree() {
    void *temp = balloc(7);  // lowest down;
    assertBallocStuffSmall(true, temp);

    bfree(temp);

    for (int i = 0; i < MAX_LEVEL; ++i) {
        assert(flists[i] == NULL);  // should have freed everything except highest level.
    }
    assert(flists[MAX_LEVEL] != NULL);

    assert(flists[MAX_LEVEL]->next == NULL);
    assert(flists[MAX_LEVEL]->prev == NULL);
    assert(flists[MAX_LEVEL]->status == STATUS_FREE);
    assert(flists[MAX_LEVEL]->level == MAX_LEVEL);
    printf("BFree is successful\n");
}

bool testBallocAndBFree() {
    // testing balloc and bfree together.

    int num_tries = 128 * 10;
    // 8 + 24 = 32 => Level 0.
    // We allocate 10 * 128 * 32 = 10 * 4096 This means that there must be no things in flists[1:7], but 128 * 10 in flists[0]
    void *array[num_tries];
    for (int i = 0; i < num_tries; ++i) {
        array[i] = balloc(8);
    }
    for (int i = 1; i < LEVELS; ++i) {
        assert(flists[i] == NULL);
    }
    head *h = flists[0];
    int count = 0;
    int countPrevNotNull = 0, countNextNotNull = 0; // these check the state of the linked list.
    while (h != NULL) {
        // h = h->next;
        if (h->next != NULL) {
            assert(h->next->prev == h);
            countNextNotNull++;
        }
        if (h->prev != NULL) {
            assert(h->prev->next == h);
            countPrevNotNull++;
        }
        assert(h->status == STATUS_USED);
        count++;
        h = h->next;
    }
    assert(count == num_tries);
    assert(countPrevNotNull == num_tries - 1);
    assert(countNextNotNull == num_tries - 1);
    // now free
    for (int i = 0; i < num_tries; ++i) {
        bfree(array[i]);
    }

    // now check that everything is freed
    for (int i = 0; i < MAX_LEVEL; ++i) {
        assert(flists[i] == NULL);
    }

    count = 0;
    countPrevNotNull = 0;
    countNextNotNull = 0;
    h = flists[LEVELS - 1];
    while (h != NULL) {
        if (h->next != NULL) {
            assert(h->next->prev == h);
            countNextNotNull++;
        }
        if (h->prev != NULL) {
            assert(h->prev->next == h);
            countPrevNotNull++;
        }
        assert(h->status == STATUS_FREE);
        count++;
        h = h->next;
    }
    assert(count == num_tries / 128);
    assert(countPrevNotNull == count - 1);
    assert(countNextNotNull == count - 1);

    freeAll();
    cleanFreeList();

    // now test the pdf example (use 450 instead of 500, as 500 + 24 > 512)
    int* temp = balloc(450);
    assert(flists[MAX_LEVEL] == NULL);
    for (int i=0; i< 4; ++i) assert(flists[i] == NULL);
    for (int i=5; i < 7; ++i) {
        assert(flists[i] != NULL);
        assert(flists[i]->next == NULL);
        assert(flists[i]->prev == NULL);
        assert(flists[i]->status == STATUS_FREE);
    }
    assert(flists[4] != NULL);
    assert(flists[4]->next != NULL);
    assert(flists[4]->prev == NULL);

    assert(flists[4]->next->next == NULL);
    assert(flists[4]->next->prev == flists[4]);
    assert(magic(temp) == flists[4] || magic(temp) == flists[4]->next);
    assert(STATUS_USED == flists[4]->status || STATUS_USED == flists[4]->next->status);

    bfree(NULL); // shouldn't error
    bfree(temp); // actually free
    for (int i=0; i< MAX_LEVEL; ++ i){
        assert(flists[i] == NULL);
    }
    assert(flists[MAX_LEVEL] != NULL);
    assert(flists[MAX_LEVEL]->next == NULL);
    assert(flists[MAX_LEVEL]->prev == NULL);
    assert(flists[MAX_LEVEL]->status == STATUS_FREE);
    printf("Balloc + BFree is successful\n");

}

void benchmark() {
    // This is inspired by the benchmarks in lab4.
    /* SAMPLE OUTPUT: (for 1000 * 100000)
    Balloc Took: 1.937500s and Malloc Took: 0.859375s for random requests
    Balloc Took: 1.609375s and Malloc Took: 1.031250s for exponential requests

    balloc Took: 1.687500s for small requests and 4.812500s for large requests with a buffer of 100
    balloc Took: 3.625000s for small requests and 0.296875s for large requests with a buffer of 1

    malloc Took: 0.203125s for small requests and 0.343750s for large requests with a buffer of 100
    malloc Took: 0.187500s for small requests and 0.343750s for large requests with a buffer of 1
    */
    // random requests
    clock_t startTimeB = clock();
    doMemoryAllocs(balloc, bfree, requestRandom, 100);
    clock_t endTimeB = clock();

    clock_t startTime = clock();
    doMemoryAllocs(malloc, free, requestRandom, 100);
    clock_t endTime = clock();

    printf("Balloc Took: %fs and Malloc Took: %fs for random requests\n",(double)(endTimeB-startTimeB) / CLOCKS_PER_SEC, 
    (double)(endTime - startTime)/CLOCKS_PER_SEC);
    freeAll();
    cleanFreeList();

    // exponential requests
    startTimeB = clock();
    doMemoryAllocs(balloc, bfree, requestExp, 100);
    endTimeB = clock();

    startTime = clock();
    doMemoryAllocs(malloc, free, requestExp, 100);
    endTime = clock();

    printf("Balloc Took: %fs and Malloc Took: %fs for exponential requests\n",(double)(endTimeB-startTimeB) / CLOCKS_PER_SEC, 
    (double)(endTime - startTime)/CLOCKS_PER_SEC);
    printf("\n");
    bufferBenchmarks(balloc, bfree, "balloc");
    printf("\n");
    bufferBenchmarks(malloc, free, "malloc");
    printf("--------------------------------\n");
    printf("Benchmarks are successful\n");
}

void bufferBenchmarks(void *func(size_t), void freeFunc(void*), char* name){
        // just runs benchmarks with a variety of buffer sizes, as well as large and small requests.
        freeAll();
        cleanFreeList();
        // arbitrary small and large requests
        clock_t startTimeBSmall = clock();
        doMemoryAllocs(func, freeFunc, requestSmall, 100);
        clock_t endTimeBSmall = clock();

        freeAll();
        cleanFreeList();

        clock_t startTimeB = clock();
        doMemoryAllocs(func, freeFunc, requestLarge, 100);
        clock_t endTimeB = clock();
        
        printf("%s Took: %fs for small requests and %fs for large requests with a buffer of 100\n",
        name,
        (double)(endTimeBSmall-startTimeBSmall) / CLOCKS_PER_SEC, 
        (double)(endTimeB-startTimeB) / CLOCKS_PER_SEC);

        freeAll();
        cleanFreeList();
        startTimeBSmall = clock();
        doMemoryAllocs(func, freeFunc, requestSmall, 1);
        endTimeBSmall = clock();

        freeAll();
        cleanFreeList();

        startTimeB = clock();
        doMemoryAllocs(func, freeFunc, requestLarge, 1);
        endTimeB = clock();
        
        printf("%s Took: %fs for small requests and %fs for large requests with a buffer of 1\n",
        name,
        (double)(endTimeBSmall-startTimeBSmall) / CLOCKS_PER_SEC, 
        (double)(endTimeB-startTimeB) / CLOCKS_PER_SEC);
}
int requestRandom(){
    return  (rand() % 4000) + sizeof(int);
}

int requestExp(){
    int max = 4000;
    int min = 8;
    double k = log(((double) max)/ min);
    
    double r = ((double)(rand() % (int)(k*10000)))/10000;

    int size = (int)((double)max / exp(r));
    return size;

}

void cleanFreeList() {
    // this is just a temporary thing to clean up the free list so that the different tests don't interfere with each other.
    for (int i = 0; i < LEVELS; ++i) flists[i] = NULL;
}

// This function does memory allocations in a loop, 
// func is either malloc of balloc, freefunc is free or bfree. request is a function that returns
// a size to allocate
// buffsize is the size of the buffer used.
void doMemoryAllocs(void *func(size_t), void freeFunc(void*), int request(void), int buffSize) {
    // int buffSize = 100;
    void* buffer[buffSize];
    for (int i=0; i < buffSize; ++i) buffer[i] = NULL;

    int ROUNDS = 1000;
    int LOOP = 1000;
    for (int j = 0; j < ROUNDS; j++) {
        for (int i = 0; i < LOOP; i++) {
            int index = rand() % buffSize;
            if (buffer[index] != NULL){
                freeFunc(buffer[index]);
            }
            int *memory;
            size_t size = request();

            memory = func(size);
            if (memory == NULL) {
                fprintf(stderr, "malloc failed\n");
            }  
            // writing to the memory so we know it exists
            *memory = 123;
            buffer[index] = memory;
            // freeFunc(memory);
        }
    }
}


int requestSmall(){
    return 8;
}
int requestLarge(){
    return 4000;
}


bool testNew() {
    void *init = sbrk(0);
    void *current;

    struct head *block = new ();
    /*  
    As explained by branden, sbrk gets the top of the heap, but mmap uses a different memory space
    (somewhere between the stack and the heap.)
    So, it doesn't really make sense to use sbrk to check if mmap worked. 
    This test is thus a bit superfluous, but that's ok.

    Also, we never delete anything, so this causes a memory leak. 
    The assignment didn't specify to do any freeing (munmap), so I think we'll do that in Part B.
     */
    if (verbose) {
        printf("The initial top of the heap is %p.\n", init);
        printf("Head is %p. \n", block);
    }
    current = sbrk(0);
    int allocated = (int)((current - init) / 1024);
    if (verbose) {
        printf("The current top of the heap is %p. \n", current);
        printf("Allocated %d. KB \n", allocated);
    }
    return true;
}

bool testBuddy() {
    struct head *block = new ();
    head *buddyTmp = buddy(block);
    assert(buddyTmp == block);  // testing the noop.
    head *spl = split(block);
    head *budOfSpl = buddy(spl);
    head *budOfOG = buddy(block);

    int bitsBlock = lowest_bits(12, (long long)block);
    int bitsSplit = lowest_bits(12, (long long)spl);
    int bitsBudSpl = lowest_bits(13, (long long)budOfSpl);
    int bitsBudOG = lowest_bits(13, (long long)budOfOG);

    if (verbose) {
        printf("Block: ");
        printBin(bitsBlock);
        printf("Split: ");
        printBin(bitsSplit);
        printf("BudBl: ");
        printBin(bitsBudOG);
        printf("BudSp: ");
        printBin(bitsBudSpl);
    }

    // assert that the lowest bits are as expected.
    if (verbose) {
        printf("%d should be 0 (lowest 12 bits)\n", bitsBlock);
        printf("%d should be 2048 (lowest 12 bits of split)\n", bitsSplit);
    }
    assert(bitsBlock == 0);
    assert(bitsSplit == 2048);

    // assert that the buddies are each other
    assert(budOfSpl == block);
    assert(budOfOG == spl);

    // assert MAX_LEVEL

    assert(block->level == 6);
    assert(spl->level == 6);
    assert(budOfOG->level == 6);
    assert(budOfSpl->level == 6);

    assert(block->status == STATUS_FREE);
    assert(spl->status == STATUS_FREE);
    assert(budOfOG->status == STATUS_FREE);
    assert(budOfSpl->status == STATUS_FREE);

    // split again

    head *split2 = split(spl);
    assert(spl->level == 5);
    assert(split2->level == 5);

    assert(buddy(spl) == split2);
    assert(buddy(split2) == spl);

    assert(lowest_bits(12, (ll)spl) == 2048);
    assert(lowest_bits(12, (ll)split2) == 2048 + 1024);
    if (verbose) {
        printf("%d should be 2048 (lowest 12 bits)\n", lowest_bits(12, (ll)spl));
        printf("%d should be 3072 (lowest 12 bits of split)\n", lowest_bits(12, (ll)split2));
    }

    // now do some merging and assert stuff.
    // first merge spl & split2
    head *merged = merge(spl, split2);
    assert(merged->level == 6);

    // the memory should have been set to 0.
    assert(split2->level == 0);
    assert(split2->status == 0);
    assert(split2->next == NULL);
    assert(split2->prev == NULL);

    assert(spl == merged);

    head *mainMerged = merge(spl, block);

    // we check that the memory of the secondary was actually set to 0.
    assert(spl->level == 0);
    assert(spl->status == 0);
    assert(spl->next == NULL);
    assert(spl->prev == NULL);

    assert(mainMerged->level == MAX_LEVEL);
    assert(mainMerged == block);

    printf("Buddy, split  & merge is successful\n");
}

bool testPrimary() {
    struct head *block = new ();
    head *spl = split(block);
    head *budOfSpl = buddy(spl);
    head *budOfOG = buddy(block);

    head *prim1 = primary(budOfSpl);
    head *prim2 = primary(budOfOG);
    head *prim3 = primary(block);
    assert(prim1 == prim2);
    assert(prim1 == prim3);
    assert(prim1 == block);

    // another level deeper
    head *splitAgain = split(block);
    head *buddySplitAgain = buddy(splitAgain);
    head *prim4 = primary(splitAgain);
    head *prim5 = primary(buddySplitAgain);
    assert(prim4 == block);
    assert(prim4 == prim5);
    assert(block->level == 5);

    printf("Primary is successful!\n");
    return true;
}

bool testHideMagic() {
    struct head *block = new ();
    char *hidden = (char *)(hide(block));
    int diff = (hidden - (char *)block);  // the difference.
    assert(diff == sizeof(head));

    void *hiddens = hide(block);
    head *magics = magic(hiddens);
    assert(magics == block);

    // another level deeper
    head *spl = split(block);

    diff = (hidden - (char *)block);  // the difference.
    assert(diff == sizeof(head));

    head *hideOfSplit = hide(spl);
    head *magicOfHide = magic(hideOfSplit);
    assert(magicOfHide == spl);

    printf("Hide and Magic is successful!\n");
    return true;
}

bool testLevel() {
    for (int i = 2; i < MAX_LEVEL; ++i) {
        int tmp = (1 << (i + MIN - 1));
        int tmpReqSize = tmp;
        if (verbose)
            printf("Expecting %d to have a level of %i, but it has %i\n", tmpReqSize, i, level(tmpReqSize));
        assert(level(tmpReqSize) == i);
        assert(level(tmp + 1) == i);
        // assert(level(tmp << 1) == i);
    }

    // some assorted ones.
    assert(level(8) == 0);
    assert(level(16) == 1);
    assert(level(32) == 1);
    assert(level(64) == 2);

    assert(level(104) == 2);  // 104 + 24 = 128, so level 2 is fine
    assert(level(105) == 3);  // 105 + 24 = 129, so level 3

    assert(level(128) == 3);
    assert(level(256) == 4);
    assert(level(1024) == 6);
    assert(level(2048) == 7);
    assert(level(400) == 4);
    printf("Level is successful!\n");
}

int oldTest() {
    // More or less the starter code. Not really necessary now.
    void *init = sbrk(0);
    void *current;
    struct head *block;
    block = new ();
    // to my knowledge, getting a buddy of a big block does not make sense.
    // struct head *bud = buddy(block);
    // if (bud != NULL)
    // blockinfo(bud);
    printf("Main Block\n");
    blockinfo(block);
    struct head *splitted = split(block);
    printf("Expecting level 6\n");
    blockinfo(splitted);

    printf("Expecting Main block\n");
    struct head *primes = primary(splitted);
    blockinfo(primes);

    printf("Expecting Main block \n");
    struct head *primeb = primary(block);
    blockinfo(primeb);

    printf("Expecting intial again \n");
    struct head *hiddenblockhead = hide(block);
    // blockinfo(hiddenblockhead);

    struct head *unhiddenblockhead = magic(hiddenblockhead);
    blockinfo(unhiddenblockhead);

    int leveltest1 = level(32);
    printf("min required level = %d\n", leveltest1);
    int leveltest2 = level(100);
    printf("min required level = %d\n", leveltest2);
    int leveltest3 = level(1500);
    printf("min required level = %d\n", leveltest3);

    return 0;
}
