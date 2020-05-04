#ifndef __PROGTEST__

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <cmath>

using namespace std;
#endif /* __PROGTEST__ */

struct Block {
    size_t size;
    Block *next;
};

const unsigned int LEVELS = 31;
unsigned int lowestLevel = 4, highestLevel = 0, usingLevels = 0, indexesBlocks = 0, fillerBlocks = 0;
unsigned int lowestLevelBlockSize = 16, highestLevelBlockSize = 0;
size_t heapMemorySize = 0, realMemorySize = 0, occupancyIndexBits = 0, splitIndexBits = 0;

uint8_t *realMemoryStart = NULL, *heapMemoryStart = NULL;
Block *lists[LEVELS];
unsigned int blockCounts[LEVELS];

size_t nearestHigherPower(int memSize) {
    double nhp = pow(2, ceil(log2(memSize)));
    //printf("N^P of %d is %f\n", memSize, nlp);
    return nhp;
}

size_t nearestLowerPower(int memSize) {
    double nlp = pow(2, floor(log2(memSize)));
    //printf("NˇP of %d is %f\n", memSize, nlp);
    return nlp;
}

unsigned int highestPowerOfABlock(int memSize) {
    return floor(log2(memSize));
}

unsigned int setIndexesBlocks(size_t indexesBits) {
    unsigned int lowestLevelBlockBits = 8 * lowestLevelBlockSize;
    unsigned int indexesBlocks = indexesBits / lowestLevelBlockBits;

    //only 1 lowest-level block needed
    if (!indexesBlocks)
        return 1;

    if (indexesBits % lowestLevelBlockBits)
        return indexesBlocks + 1;
    return indexesBlocks;
}

void initSeq1(void *memPool, int memSize) {
    //the whole enlarged memory block
    realMemoryStart = (uint8_t *) memPool;

    //the actual memory amount to govern
    //heapMemoryStart = (uint8_t *) memPool;

    heapMemorySize = memSize;
    printf("I GOT %d Bytes\n", memSize);

    realMemorySize = nearestHigherPower(memSize);
    printf("HEAP ENLARGED TO %zu Bytes\n", realMemorySize);

    fillerBlocks = (realMemorySize - heapMemorySize) / 16;
    if ((realMemorySize - heapMemorySize) % 16)
        fillerBlocks += 1;

    printf("FILLER BLOCKS USED: %d\n", fillerBlocks);

    highestLevel = highestPowerOfABlock(realMemorySize);
    highestLevelBlockSize = pow(2, highestLevel);
    printf("TOP LEVEL (POWER) IN HEAP IS LEVEL %d OF SIZE %d\n", highestLevel, highestLevelBlockSize);

    usingLevels = highestLevel - lowestLevel + 1;
    printf("WILL BE USING %d LEVELS IN HEAP\n", usingLevels);

    //prepare occupancy index
    occupancyIndexBits = (realMemorySize / lowestLevelBlockSize) * 2 - 1;
    printf("OCCUPANCY INDEX WILL NEED %zu BITS\n", occupancyIndexBits);

    //prepare split index
    splitIndexBits = occupancyIndexBits / 2;
    printf("SPLIT INDEX WILL NEED %zu BITS\n", splitIndexBits);

    //update heapMemory pointer to be set after indexes space
    indexesBlocks = setIndexesBlocks(occupancyIndexBits + splitIndexBits);
    printf("COMBINED INDEX WILL NEED %d OF %d-B-SIZED BLOCKS\n", indexesBlocks, lowestLevelBlockSize);
    printf("TOTAL AMOUNT OF PREALLOCATED 16-B BLOCKS: %d \n", indexesBlocks + fillerBlocks);

    //heapMemoryStart += indexesBlocks * lowestLevelBlockSize;
    //printf("MOVING heapMemoryStart by %d BYTES\n", indexesBlocks * lowestLevelBlockSize);
}

void initSeq2() {
    //set top-level free list

    //set indexes blocks as allocated in the bitfield
    size_t occupyIndex;
    size_t shiftBytes;
    size_t shiftBits;
    uint8_t *ptr = NULL;
    uint8_t mask;

    //clear out memory of the indexes blocks
    unsigned int indexBytes = indexesBlocks * lowestLevelBlockSize;
    ptr = realMemoryStart;

    for (unsigned int i = 0; i < indexBytes; i++)
    {
        *ptr = 0;
        ptr++;
    }

    printf("-----------------ALLOCATING INDEX BLOCKS -------------------\n");

    for (unsigned int i = 0; i < indexesBlocks; ++i) {
        ptr = realMemoryStart;

        occupyIndex = (1 << (highestLevel - 4)) + i - 1;
        printf("marking index %zu as allocated\n", occupyIndex);

        shiftBytes = occupyIndex / 8;
        printf("shifted BYTES: %zu\n", shiftBytes);
        shiftBits = occupyIndex % 8;
        printf("shifted bits: %zu\n", shiftBits);

        ptr += shiftBytes;
        printf("*ptr BEFORE masking: %d\n", *ptr);
        mask = 128 >> shiftBits;
        *ptr = *ptr | mask;
        printf("*ptr AFTER masking: %d\n\n", *ptr);
    }

    printf("-----------------ALLOCATING FILLER BLOCKS -------------------\n");

    //set filler blocks as allocated
    for (unsigned int j = fillerBlocks; j > 0; --j) {
        ptr = realMemoryStart;

        occupyIndex = 2 * (1 << (highestLevel - 4)) - j - 1;
        printf("marking index %zu as allocated\n", occupyIndex);

        shiftBytes = occupyIndex / 8;
        printf("shifted BYTES: %zu\n", shiftBytes);
        shiftBits = occupyIndex % 8;
        printf("shifted bits: %zu\n", shiftBits);

        ptr += shiftBytes;
        printf("*ptr BEFORE masking: %d\n", *ptr);
        mask = 128 >> shiftBits;
        *ptr = *ptr | mask;
        printf("*ptr AFTER masking: %d\n\n", *ptr);
    }
}

void printBlocks(unsigned int level)
{
    Block * ptr = lists[level];
    printf("LIST: ");

    while (ptr != NULL)
    {
        printf("block-%zu-bytes -> ", ptr->size);
        ptr = ptr->next;
    }

    printf("NULL\n");
}

bool createLowestLevelBlocks() {

    if (lowestLevel == highestLevel) {
        printf("**** LOWEST IS HIGHEST!!!\n");
        return false;
    }

    unsigned int curLevel = lowestLevel + 1;
    //printf("**** CUR_LEVEL: %d\n", curLevel);
    uint8_t* ptr1 = NULL, *ptr2 = NULL;
    Block* endblock = NULL;

    while (curLevel <= highestLevel) {
        if (lists[curLevel] == NULL) {
            curLevel++;
        } else {
            if (lists[curLevel - 1] != NULL)
            {
                endblock = lists[curLevel - 1];
                ptr1 = (uint8_t *)(lists[curLevel - 1]);
                size_t x = pow(2, curLevel - 1) * (blockCounts[curLevel - 1] - 1);
                ptr1 += x;
                endblock = (Block *) ptr1;

                //printf("ENDBLOCK IS: block-%zu-bytes\n", endblock->size);
                //printf("ENDBLOCK -> NEXT IS NULL: %d\n", endblock->next == NULL);

                endblock->next = lists[curLevel];
                lists[curLevel] = lists[curLevel]->next;
                endblock->next->size = endblock->size;
                ptr2 = ((uint8_t *)(endblock->next)) + endblock->size;
                endblock->next->next = (Block *)ptr2;
                endblock->next->next->size = lists[curLevel - 1]->size;
                endblock->next->next->next = NULL;
                blockCounts[curLevel] -= 1;
                blockCounts[curLevel - 1] += 2;
            } else {
                //printf("INSIDE LEVEL THATS NULL\n");
                lists[curLevel - 1] = lists[curLevel];
                lists[curLevel] = lists[curLevel]->next;
                lists[curLevel - 1]->size /= 2;
                ptr2 = ((uint8_t*)lists[curLevel - 1]) + lists[curLevel - 1]->size;
                lists[curLevel - 1]->next = (Block*)ptr2;
                lists[curLevel - 1]->next->size = lists[curLevel - 1]->size;
                lists[curLevel - 1]->next->next = NULL;
                blockCounts[curLevel] -= 1;
                blockCounts[curLevel - 1] += 2;
                //printf("**** CREATED BLOCKS OF LEVEL %d\n", curLevel - 1);
            }

            //printBlocks(lowestLevel);

            if (curLevel - 1 == lowestLevel)
                return true;

            curLevel = lowestLevel + 1;
        }
    }

    printf("/// WENT BEHIND WHILE!!!\n");
    return false;
}

bool removePreallocatedBlocksFromList(unsigned int numBlocks)
{
    if (numBlocks > blockCounts[lowestLevel])
    {
        printf("NOT ENOUGH BLOCKS AT LOWEST LEVEL FOR REMOVAL\n");
        return false;
    }

    if (lists[lowestLevel] == NULL)
    {
        printf("NOTHING IN LOWEST LEVEL\n");
        return false;
    }

    printf("REMOVING BLOCKS\n");
    
    uint8_t* ptr = (uint8_t *)(lists[lowestLevel]);
    unsigned int cntr = 0;

    size_t x = (numBlocks - 1) * pow(2, lowestLevel);
    ptr += x;

    //before removal state
    //printBlocks(lowestLevel);

    Block *lastBlock = (Block *)ptr;
    lists[lowestLevel] = lastBlock->next;
    blockCounts[lowestLevel] -= numBlocks;

    return true;
}

bool initSeq3() {
    lists[highestLevel] = (Block *) realMemoryStart;
    lists[highestLevel]->next = NULL;
    lists[highestLevel]->size = highestLevelBlockSize;
    blockCounts[highestLevel] = 1;

    //printf("lists[HL]->size = %d\n", lists[highestLevel]->size);

    unsigned int numBlocks = indexesBlocks + fillerBlocks;

    //create numBlocks free blocks of lowest level
    while (blockCounts[lowestLevel] < numBlocks) {
        createLowestLevelBlocks();
        //printf("**** AFTER CREATION OF SOME BLOCKS\n");
    }
    
    return true;
}

void HeapInit(void *memPool, int memSize) {
    /**
     * set real/heap-only pointers
     * set sizes and level values
     * prepare occupancy and split indexes
     * update heapMemory start pointer
     */
    initSeq1(memPool, memSize);

    printf("**** AFTER SEQ 1\n");

    /**
     * set top-level free list
     * set my infoblocks as allocted (index blocks, filler blocks) in the index bitfields
     */
    //initSeq2();

    //printf("**** AFTER SEQ 2\n");

    /**
     * allocate x+y 16-B blocks (x = # of index blocks, y = # of filler blocks)
     */
    bool status = initSeq3();

    if (!status)
        printf("INIT PROBLEM Seq3\n");

    printf("**** AFTER SEQ 3\n");

    printf("--------INFO--------\n");

    //for (unsigned int i = 0; i < LEVELS; i++)
    //    printf("LIST OF POWER %d HAS - %d - FREE BLOCKS\n", i, blockCounts[i]);

    //printBlocks(lowestLevel);

    if (!removePreallocatedBlocksFromList(indexesBlocks + fillerBlocks))
        printf("ERROR REMOVING PREALLOCATED BLOCKS\n");

    printf("--------AFTER REMOVAL--------\n");

    //for (unsigned int i = 0; i < LEVELS; i++)
    //   printf("LIST OF POWER %d HAS - %d - FREE BLOCKS\n", i, blockCounts[i]);

    printBlocks(lowestLevel);
    
}

void *HeapAlloc(int size) {
    /* todo */
}

bool HeapFree(void *blk) {
    /* todo */
}

void HeapDone(int *pendingBlk) {
    /* todo */
}

#ifndef __PROGTEST__

int main(void) {

    static uint8_t memPool[3 * 1048576];
    HeapInit(memPool, 1048576);

    /*
    uint8_t *p0, *p1, *p2, *p3, *p4;
    int pendingBlk;
    static uint8_t memPool[3 * 1048576];

    HeapInit(memPool, 2097152);
    assert((p0 = (uint8_t *) HeapAlloc(512000)) != NULL);
    memset(p0, 0, 512000);
    assert((p1 = (uint8_t *) HeapAlloc(511000)) != NULL);
    memset(p1, 0, 511000);
    assert((p2 = (uint8_t *) HeapAlloc(26000)) != NULL);
    memset(p2, 0, 26000);
    HeapDone(&pendingBlk);
    assert(pendingBlk == 3);


    HeapInit(memPool, 2097152);
    assert((p0 = (uint8_t *) HeapAlloc(1000000)) != NULL);
    memset(p0, 0, 1000000);
    assert((p1 = (uint8_t *) HeapAlloc(250000)) != NULL);
    memset(p1, 0, 250000);
    assert((p2 = (uint8_t *) HeapAlloc(250000)) != NULL);
    memset(p2, 0, 250000);
    assert((p3 = (uint8_t *) HeapAlloc(250000)) != NULL);
    memset(p3, 0, 250000);
    assert((p4 = (uint8_t *) HeapAlloc(50000)) != NULL);
    memset(p4, 0, 50000);
    assert(HeapFree(p2));
    assert(HeapFree(p4));
    assert(HeapFree(p3));
    assert(HeapFree(p1));
    assert((p1 = (uint8_t *) HeapAlloc(500000)) != NULL);
    memset(p1, 0, 500000);
    assert(HeapFree(p0));
    assert(HeapFree(p1));
    HeapDone(&pendingBlk);
    assert(pendingBlk == 0);


    HeapInit(memPool, 2359296);
    assert((p0 = (uint8_t *) HeapAlloc(1000000)) != NULL);
    memset(p0, 0, 1000000);
    assert((p1 = (uint8_t *) HeapAlloc(500000)) != NULL);
    memset(p1, 0, 500000);
    assert((p2 = (uint8_t *) HeapAlloc(500000)) != NULL);
    memset(p2, 0, 500000);
    assert((p3 = (uint8_t *) HeapAlloc(500000)) == NULL);
    assert(HeapFree(p2));
    assert((p2 = (uint8_t *) HeapAlloc(300000)) != NULL);
    memset(p2, 0, 300000);
    assert(HeapFree(p0));
    assert(HeapFree(p1));
    HeapDone(&pendingBlk);
    assert(pendingBlk == 1);


    HeapInit(memPool, 2359296);
    assert((p0 = (uint8_t *) HeapAlloc(1000000)) != NULL);
    memset(p0, 0, 1000000);
    assert(!HeapFree(p0 + 1000));
    HeapDone(&pendingBlk);
    assert(pendingBlk == 1);

     */

    return 0;
}

#endif /* __PROGTEST__ */

