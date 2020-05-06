#ifndef __PROGTEST__

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <cmath>

using namespace std;
#endif /* __PROGTEST__ */

struct Block
{
    size_t size;
    Block *next;
};

int allocatedBlocks = 0;
const unsigned int LEVELS = 31;
unsigned int lowestLevel = 4, highestLevel = 0, usingLevels = 0, indexesBlocks = 0, fillerBlocks = 0;
unsigned int lowestLevelBlockSize = (1 << lowestLevel), highestLevelBlockSize = 0;
size_t heapMemorySize = 0, realMemorySize = 0, occupancyIndexBits = 0, splitIndexBits = 0;

uint8_t *realMemoryStart = NULL, *heapMemoryStart = NULL;
static Block *lists[LEVELS];
static unsigned int freeBlocksOfLevel[LEVELS];
static unsigned int splitCounts[LEVELS];

size_t nearestHigherPower(int memSize)
{
    double nhp = pow(2, ceil(log2(memSize)));
    //printf("N^P of %d is %f\n", memSize, nlp);
    return nhp;
}

size_t nearestLowerPower(int memSize)
{
    double nlp = pow(2, floor(log2(memSize)));
    //printf("NˇP of %d is %f\n", memSize, nlp);
    return nlp;
}

unsigned int highestPowerOfABlock(int memSize)
{
    return floor(log2(memSize));
}

unsigned int setIndexesBlocks(size_t indexesBits)
{
    unsigned int lowestLevelBlockBits = 8 * lowestLevelBlockSize;
    unsigned int indexesBlocks = indexesBits / lowestLevelBlockBits;

    //only 1 lowest-level block needed
    if (!indexesBlocks)
        return 1;

    if (indexesBits % lowestLevelBlockBits)
        return indexesBlocks + 1;
    return indexesBlocks;
}

void initSeq1(void *memPool, int memSize)
{
    //zero-out info
    for (size_t i = 0; i < LEVELS; i++)
    {
        freeBlocksOfLevel[i] = 0;
        splitCounts[i] = 0;
        lists[i] = NULL;
    }

    allocatedBlocks = 0;
    //the whole enlarged memory block
    realMemoryStart = (uint8_t *)memPool;

    //the actual memory amount to govern
    heapMemoryStart = (uint8_t *)memPool;

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

    heapMemoryStart += indexesBlocks * lowestLevelBlockSize;
    printf("MOVING heapMemoryStart by %d BYTES\n", indexesBlocks * lowestLevelBlockSize);
}

void initSeq2()
{
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

    for (unsigned int i = 0; i < indexesBlocks; ++i)
    {
        ptr = realMemoryStart;

        occupyIndex = (1 << (highestLevel - 4)) + i - 1;
        //printf("marking index %zu as allocated\n", occupyIndex);

        shiftBytes = occupyIndex / 8;
        //printf("shifted BYTES: %zu\n", shiftBytes);
        shiftBits = occupyIndex % 8;
        //printf("shifted bits: %zu\n", shiftBits);

        ptr += shiftBytes;
        //printf("*ptr BEFORE masking: %d\n", *ptr);
        mask = 128 >> shiftBits;
        *ptr = *ptr | mask;
        //printf("*ptr AFTER masking: %d\n\n", *ptr);
    }

    printf("-----------------ALLOCATING FILLER BLOCKS -------------------\n");

    //set filler blocks as allocated
    for (unsigned int j = fillerBlocks; j > 0; --j)
    {
        ptr = realMemoryStart;

        occupyIndex = 2 * (1 << (highestLevel - 4)) - j - 1;
        //printf("marking index %zu as allocated\n", occupyIndex);

        shiftBytes = occupyIndex / 8;
        //printf("shifted BYTES: %zu\n", shiftBytes);
        shiftBits = occupyIndex % 8;
        //printf("shifted bits: %zu\n", shiftBits);

        ptr += shiftBytes;
        //printf("*ptr BEFORE masking: %d\n", *ptr);
        mask = 128 >> shiftBits;
        *ptr = *ptr | mask;
        //printf("*ptr AFTER masking: %d\n\n", *ptr);
    }
}

void printBlocks(unsigned int level)
{
    Block *ptr = lists[level];
    printf("LIST: ");

    while (ptr != NULL)
    {
        printf("block-%zu-bytes -> ", ptr->size);
        ptr = ptr->next;
    }

    printf("NULL\n");
}

void setIndexBit(size_t index)
{
    unsigned int bytes = index / 8;
    unsigned int bits = index % 8;

    uint8_t mask;
    uint8_t *ptr = realMemoryStart;
    ptr += bytes;

    mask = 128 >> bits;
    *ptr = *ptr | mask;
}

void unsetIndexBit(size_t index)
{
    unsigned int bytes = index / 8;
    unsigned int bits = index % 8;

    uint8_t mask;
    uint8_t *ptr = realMemoryStart;
    ptr += bytes;

    mask = 128 >> bits;
    mask = !mask;
    *ptr = *ptr & mask;
}

void setSplitBit(size_t index)
{
    index += occupancyIndexBits;
    unsigned int bytes = index / 8;
    unsigned int bits = index % 8;

    uint8_t mask;
    uint8_t *ptr = realMemoryStart;
    ptr += bytes;

    mask = 128 >> bits;
    *ptr = *ptr | mask;
}

void unsetSplitBit(size_t index)
{
    index += occupancyIndexBits;
    unsigned int bytes = index / 8;
    unsigned int bits = index % 8;

    uint8_t mask;
    uint8_t *ptr = realMemoryStart;
    ptr += bytes;

    mask = 128 >> bits;
    mask = !mask;
    *ptr = *ptr & mask;
}

bool createLowestLevelBlocks()
{
    if (lowestLevel == highestLevel)
    {
        printf("**** LOWEST IS HIGHEST!!!\n");
        return false;
    }

    unsigned int curLevel = lowestLevel + 1;
    size_t splitIndex = 0;
    //printf("**** CUR_LEVEL: %d\n", curLevel);
    //uint8_t* ptr1 = NULL, *ptr2 = NULL;
    //Block* endblock = NULL;

    while (curLevel <= highestLevel)
    {
        if (freeBlocksOfLevel[curLevel] == 0)
        {
            curLevel++;
        }
        else
        {
            //endblock = lists[curLevel - 1];
            //ptr1 = (uint8_t *)(lists[curLevel - 1]);
            //size_t x = pow(2, curLevel - 1) * (freeBlocksOfLevel[curLevel - 1] - 1);
            //ptr1 += x;
            //endblock = (Block *) ptr1;

            //printf("ENDBLOCK IS: block-%zu-bytes\n", endblock->size);
            //printf("ENDBLOCK -> NEXT IS NULL: %d\n", endblock->next == NULL);

            //endblock->next = lists[curLevel];
            //lists[curLevel] = lists[curLevel]->next;
            //endblock->next->size = endblock->size;
            //ptr2 = ((uint8_t *)(endblock->next)) + endblock->size;
            //endblock->next->next = (Block *)ptr2;
            //endblock->next->next->size = lists[curLevel - 1]->size;
            //endblock->next->next->next = NULL;
            freeBlocksOfLevel[curLevel] -= 1;
            freeBlocksOfLevel[curLevel - 1] += 2;
            splitIndex = pow(2, highestLevel - curLevel) + splitCounts[curLevel] - 1;
            setSplitBit(splitIndex);
            splitCounts[curLevel] += 1;

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
    if (numBlocks > freeBlocksOfLevel[lowestLevel])
    {
        printf("NOT ENOUGH BLOCKS AT LOWEST LEVEL FOR REMOVAL\n");
        return false;
    }

    if (freeBlocksOfLevel[lowestLevel] == 0)
    {
        printf("NOTHING IN LOWEST LEVEL\n");
        return false;
    }

    //printf("REMOVING BLOCKS\n");

    //uint8_t* ptr = (uint8_t *)(lists[lowestLevel]);
    //unsigned int cntr = 0;

    //size_t x = (numBlocks - 1) * pow(2, lowestLevel);
    //ptr += x;

    //before removal state
    //printBlocks(lowestLevel);

    //Block *lastBlock = (Block *)ptr;
    //lists[lowestLevel] = lastBlock->next;
    freeBlocksOfLevel[lowestLevel] -= numBlocks;

    return true;
}

bool initSeq3()
{
    //lists[highestLevel] = (Block *) realMemoryStart;
    //lists[highestLevel]->next = NULL;
    //lists[highestLevel]->size = highestLevelBlockSize;
    freeBlocksOfLevel[highestLevel] = 1;

    //printf("lists[HL]->size = %d\n", lists[highestLevel]->size);

    unsigned int numBlocks = indexesBlocks + fillerBlocks;

    //create numBlocks free blocks of lowest level
    while (freeBlocksOfLevel[lowestLevel] < numBlocks)
    {
        createLowestLevelBlocks();
        //printf("**** AFTER CREATION OF SOME BLOCKS\n");
    }

    return true;
}

//sets the correct memory pointers into place
void initSeq4()
{
    uint8_t *ptr = NULL;
    size_t startsAt = 0;
    size_t blockSize = 0;

    for (size_t i = 0; i < LEVELS; i++)
    {
        //printf("CHECKING LEVEL %zu\n", i);
        if (freeBlocksOfLevel[i] == 0)
            continue;

        ptr = heapMemoryStart;
        ptr += startsAt;
        lists[i] = (Block *)ptr;
        blockSize = pow(2, i);
        lists[i]->size = blockSize;
        lists[i]->next = NULL;
        startsAt += blockSize;
    }
}

void HeapInit(void *memPool, int memSize)
{
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
    initSeq2();

    printf("**** AFTER SEQ 2\n");

    /**
     * allocate x+y 16-B blocks (x = # of index blocks, y = # of filler blocks)
     */
    bool status = initSeq3();

    if (!status)
        printf("INIT PROBLEM Seq3\n");

    printf("**** AFTER SEQ 3\n");

    //printf("--------INFO--------\n");

    //for (unsigned int i = 0; i < LEVELS; i++)
    //    printf("LIST OF POWER %d HAS - %d - FREE BLOCKS\n", i, freeBlocksOfLevel[i]);

    //printBlocks(lowestLevel);

    if (!removePreallocatedBlocksFromList(indexesBlocks + fillerBlocks))
        printf("ERROR REMOVING PREALLOCATED BLOCKS\n");

    printf("--------AFTER REMOVAL--------\n");

    for (unsigned int i = 0; i < LEVELS; i++)
        printf("LIST OF POWER %d HAS - %d - FREE BLOCKS\n", i, freeBlocksOfLevel[i]);

    //printBlocks(lowestLevel);

    //set the final memory pointers
    initSeq4();
    printf("**** AFTER SEQ 4\n");

    printf("MEMORY SET AND PREPARED FOR USE\n");
}

bool createBlocksOfLevel(size_t level)
{
    if (level == highestLevel)
    {
        printf("**** THERE IS NO HIGHER LEVEL!!!\n");
        return false;
    }

    unsigned int curLevel = level + 1;
    size_t splitIndex = 0, newSize = 0;
    printf("**** CUR_LEVEL: %d\n", curLevel);
    uint8_t *ptr = NULL;

    while (curLevel <= highestLevel)
    {
        if (freeBlocksOfLevel[curLevel] == 0)
        {
            curLevel++;
        }
        else
        {
            lists[curLevel - 1] = lists[curLevel];
            lists[curLevel] = lists[curLevel]->next;
            lists[curLevel - 1]->size /= 2;
            newSize = lists[curLevel - 1]->size;

            ptr = (uint8_t *)lists[curLevel - 1];
            ptr += newSize;

            lists[curLevel - 1]->next = (Block *)ptr;
            lists[curLevel - 1]->next->size = newSize;
            lists[curLevel - 1]->next->next = NULL;

            freeBlocksOfLevel[curLevel] -= 1;
            freeBlocksOfLevel[curLevel - 1] += 2;
            splitIndex = pow(2, highestLevel - curLevel) + splitCounts[curLevel] - 1;
            setSplitBit(splitIndex);
            splitCounts[curLevel] += 1;

            //printBlocks(lowestLevel);

            if (curLevel - 1 == level)
                return true;

            curLevel = level + 1;
        }
    }

    printf("/// UNABLE TO ALLOCATE BLOCK!!!\n");
    return false;
}

void *HeapAlloc(int size)
{
    size_t blockLevel = log2(nearestHigherPower(size)), index = 0, indexInLevel = 0;
    printf("NHP LEVEL is: %zu\n", blockLevel);
    Block *block = NULL;
    uint8_t *blockAddress = NULL;

    while (lists[blockLevel] == NULL)
    {
        //create block of size nhp
        if (!createBlocksOfLevel(blockLevel))
        {
            printf("COULD NOT CREATE A DESIRED BLOCK\n");
            return NULL;
        }

        printf("CREATED BLOCKS OF DESIRED SIZE\n");
    }

    //there is a free block of a correct size for allocation already

    block = lists[blockLevel];
    blockAddress = (uint8_t *)block;
    lists[blockLevel] = lists[blockLevel]->next;
    allocatedBlocks += 1;
    freeBlocksOfLevel[blockLevel] -= 1;
    indexInLevel = (blockAddress - heapMemoryStart) / pow(2, blockLevel);
    index = (1 << (highestLevel - blockLevel)) + indexInLevel - 1;
    printf("ALLOCATING INDEX %zu\n", index);
    setIndexBit(index);

    return (void *)block;
}

bool isBlockAllocated(size_t index)
{
    unsigned int bytes = index / 8;
    unsigned int bits = index % 8;

    uint8_t *ptr = realMemoryStart;
    ptr += bytes;
    uint8_t mask = 128 >> bits;
    return (mask & *ptr);
}

bool isParentSplit(size_t blockIndex)
{
    size_t parentIndex = floor((blockIndex - 1) / 2);
    size_t parentBit = parentIndex + occupancyIndexBits + 1;
    unsigned int bytes = parentBit / 8;
    unsigned int bits = parentBit % 8;

    uint8_t *ptr = realMemoryStart;
    ptr += bytes;
    uint8_t mask = 128 >> bits;
    return (mask & *ptr);
}

bool mergeBlocks(uint8_t *leftBlock, uint8_t *rightBlock)
{
    Block *lb = (Block *)leftBlock;
    Block *rb = (Block *)rightBlock;
    Block *newHigherLevelBlock = NULL;
}

bool freeBlock(uint8_t *block)
{
    unsigned int indexInLastLevel = (block - heapMemoryStart) / lowestLevelBlockSize - 1;
    unsigned int index = (1 << (highestLevel - lowestLevel)) + indexInLastLevel - 1;

    //parent starts at different address -> pointer points to the correct (lowest-level) block
    if (index % 2)
    {
        if (!isBlockAllocated(index))
            return false;

        //MERGING!!!?
        Block *blk = (Block *)block;

        if (!isBlockAllocated(index - 1))
        {
            //merge + update all info
            mergeBlocks(block - blk->size, block);
            return true;
        }

        freeBlocksOfLevel[lowestLevel] += 1;
        allocatedBlocks -= 1;

        blk->next = lists[lowestLevel];
        lists[lowestLevel] = blk;
        unsetIndexBit(index);
        return true;
    }
    else
    {
        unsigned int level = lowestLevel;
        size_t newIndex = index;
        size_t parentIndex = floor((index - 1) / 2);

        while (!isParentSplit(newIndex))
        {
            newIndex = parentIndex;
            parentIndex = floor((newIndex - 1) / 2);
            level++;
        }

        //parent is split, newIndex is the wanted block index

        if (!isBlockAllocated(newIndex))
            return false;

        Block *blk = (Block *)block;

        //MERGING!!!?
        if (!isBlockAllocated(newIndex + 1))
        {
            //merge + update all info
            mergeBlocks(block, block + blk->size);
        }

        freeBlocksOfLevel[level] += 1;
        allocatedBlocks -= 1;

        blk->next = lists[level];
        lists[level] = blk;
        unsetIndexBit(newIndex);

        return true;
    }
}

bool HeapFree(void *blk)
{
    uint8_t *block = (uint8_t *)blk;
    unsigned int blockLevel = 0;

    if (block < heapMemoryStart || block > realMemoryStart + heapMemorySize)
        return false;

    if (((size_t)(block - heapMemorySize) % lowestLevelBlockSize) != 0)
        return false;

    // now i know i have a ptr to some block (its a 16 mult.)
    printf("IT SEEMS TO BE A BLOCK ...\n");

    bool res = freeBlock(block);

    if (!res)
        return false;
    return true;
}

void HeapDone(int *pendingBlk)
{
    *pendingBlk = allocatedBlocks;
    printf("ALLOCATED BLOCKS: %d\n", allocatedBlocks);
}

#ifndef __PROGTEST__

int main(void)
{
    uint8_t *p0, *p1, *p2, *p3, *p4;
    int pendingBlk;
    static uint8_t memPool[3 * 1048576];

    printf("---INIT END---\n");

    HeapInit(memPool, 2097152);
    assert((p0 = (uint8_t *)HeapAlloc(512000)) != NULL);
    memset(p0, 0, 512000);
    assert((p1 = (uint8_t *)HeapAlloc(511000)) != NULL);
    memset(p1, 0, 511000);
    assert((p2 = (uint8_t *)HeapAlloc(26000)) != NULL);
    memset(p2, 0, 26000);
    HeapDone(&pendingBlk);
    assert(pendingBlk == 3);

    //printf("PENDING: %d\n", pendingBlk);

    HeapInit(memPool, 2097152);
    assert((p0 = (uint8_t *)HeapAlloc(1000000)) != NULL);
    memset(p0, 0, 1000000);
    assert((p1 = (uint8_t *)HeapAlloc(250000)) != NULL);
    memset(p1, 0, 250000);
    assert((p2 = (uint8_t *)HeapAlloc(250000)) != NULL);
    memset(p2, 0, 250000);
    assert((p3 = (uint8_t *)HeapAlloc(250000)) != NULL);
    memset(p3, 0, 250000);
    assert((p4 = (uint8_t *)HeapAlloc(50000)) != NULL);
    memset(p4, 0, 50000);

    /*
    assert(HeapFree(p2));
    assert(HeapFree(p4));
    assert(HeapFree(p3));
    assert(HeapFree(p1));
    assert((p1 = (uint8_t *)HeapAlloc(500000)) != NULL);
    memset(p1, 0, 500000);
    assert(HeapFree(p0));
    assert(HeapFree(p1));
    HeapDone(&pendingBlk);
    assert(pendingBlk == 0);

    HeapInit(memPool, 2359296);
    assert((p0 = (uint8_t *)HeapAlloc(1000000)) != NULL);
    memset(p0, 0, 1000000);
    assert((p1 = (uint8_t *)HeapAlloc(500000)) != NULL);
    memset(p1, 0, 500000);
    assert((p2 = (uint8_t *)HeapAlloc(500000)) != NULL);
    memset(p2, 0, 500000);
    assert((p3 = (uint8_t *)HeapAlloc(500000)) == NULL);
    assert(HeapFree(p2));
    assert((p2 = (uint8_t *)HeapAlloc(300000)) != NULL);
    memset(p2, 0, 300000);
    assert(HeapFree(p0));
    assert(HeapFree(p1));
    HeapDone(&pendingBlk);
    assert(pendingBlk == 1);

    HeapInit(memPool, 2359296);
    assert((p0 = (uint8_t *)HeapAlloc(1000000)) != NULL);
    memset(p0, 0, 1000000);
    assert(!HeapFree(p0 + 1000));
    HeapDone(&pendingBlk);
    assert(pendingBlk == 1);
    */

    return 0;
}

#endif /* __PROGTEST__ */
