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
    Block *next;
    Block *prev;
};

int allocatedBlocks = 0;
const unsigned int LEVELS = 31;
unsigned int lowestLevel = 4, highestLevel = 0, usingLevels = 0, indexesBlocks = 0, fillerBlocks = 0;
unsigned int lowestLevelBlockSize = (1 << lowestLevel), highestLevelBlockSize = 0;
size_t heapMemorySize = 0, realMemorySize = 0, occupancyIndexBits = 0, splitIndexBits = 0;

uint8_t *realMemoryStart = NULL, *heapMemoryStart = NULL;
Block *lists[LEVELS];
unsigned int freeBlocksOfLevel[LEVELS];
unsigned int splitCounts[LEVELS];
uint8_t bitmap[65536];

size_t nearestHigherPower(int memSize)
{
    size_t nhp = (size_t)pow(2, ceil(log2(memSize)));
    //printf("N^P of %d is %f\n", memSize, nhp);
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

bool isBlockFree(size_t index)
{
    unsigned int bytes = index / 8;
    unsigned int bits = index % 8;

    uint8_t *ptr = bitmap + (size_t)bytes;
    uint8_t mask = 128 >> bits;
    uint8_t res = *ptr & mask;

    if (res == 0)
    {
        //printf("BLOCK %zu IS - FREE -\n", index);
        return true;
    }

    //printf("BLOCK %zu IS - ALLOCATED -\n", index);
    return false;
}

bool isBlockSplit(size_t index)
{
    index += occupancyIndexBits;
    unsigned int bytes = index / 8;
    unsigned int bits = index % 8;
    //size_t oldIndex = index;

    //printf("BYTES: %d, BITS: %d\n", bytes, bits);

    uint8_t *ptr = bitmap;
    ptr += bytes;
    uint8_t mask = 128 >> bits;
    //printf("THE BIT: %d\n", mask & *ptr);
    uint8_t res = *ptr & mask;

    if (res == 0)
    {
        //printf("BLOK %zu Z INDEXU - NENI - SPLIT\n", oldIndex);
        return false;
    }

    //printf("BLOK %zu Z INDEXU - JE - SPLIT\n", oldIndex);
    return true;
}

/*
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
*/

void initSeq1(void *memPool, int memSize)
{
    //zero-out info
    for (size_t i = 0; i < LEVELS; i++)
        lists[i] = NULL;

    //printf("SIZE OF BITMAP IS: %d BYTES\n", sizeof(bitmap));

    for (size_t i = 0; i < LEVELS; i++)
    {
        splitCounts[i] = 0;
        freeBlocksOfLevel[i] = 0;
    }

    for (size_t i = 0; i < 65536; i++)
    {
        bitmap[i] = 0;
    }

    allocatedBlocks = 0;
    //the whole enlarged memory block
    realMemoryStart = (uint8_t *)memPool;

    //the actual memory amount to govern
    heapMemoryStart = (uint8_t *)memPool;

    heapMemorySize = memSize;
    //printf("I GOT %d Bytes\n", memSize);

    realMemorySize = nearestHigherPower(memSize);
    //printf("HEAP ENLARGED TO %zu Bytes\n", realMemorySize);

    fillerBlocks = (realMemorySize - heapMemorySize) / 16;
    if ((realMemorySize - heapMemorySize) % 16)
        fillerBlocks += 1;

    //printf("FILLER BLOCKS USED: %d\n", fillerBlocks);

    highestLevel = highestPowerOfABlock(realMemorySize);
    highestLevelBlockSize = pow(2, highestLevel);
    //printf("TOP LEVEL (POWER) IN HEAP IS LEVEL %d OF SIZE %d\n", highestLevel, highestLevelBlockSize);

    usingLevels = highestLevel - lowestLevel + 1;
    //printf("WILL BE USING %d LEVELS IN HEAP\n", usingLevels);

    //prepare occupancy index
    occupancyIndexBits = (realMemorySize / lowestLevelBlockSize) * 2 - 1;
    //printf("OCCUPANCY INDEX WILL NEED %zu BITS\n", occupancyIndexBits);

    //prepare split index
    splitIndexBits = occupancyIndexBits / 2;
    //printf("SPLIT INDEX WILL NEED %zu BITS\n", splitIndexBits);

    //update heapMemory pointer to be set after indexes space
    //indexesBlocks = setIndexesBlocks(occupancyIndexBits + splitIndexBits);
    //printf("COMBINED INDEX WILL NEED %d OF %d-B-SIZED BLOCKS\n", indexesBlocks, lowestLevelBlockSize);
    //printf("TOTAL AMOUNT OF PREALLOCATED 16-B BLOCKS: %d \n", indexesBlocks + fillerBlocks);
}

void setIndexBit(size_t index)
{
    unsigned int bytes = index / 8;
    unsigned int bits = index % 8;

    uint8_t mask;
    uint8_t *ptr = bitmap;
    ptr += bytes;

    mask = 128 >> bits;
    *ptr = *ptr | mask;
}

void unsetIndexBit(size_t index)
{
    unsigned int bytes = index / 8;
    unsigned int bits = index % 8;

    uint8_t res = 128 >> bits;
    uint8_t *ptr = bitmap;
    ptr += bytes;
    //printf("--- BEFORE UNSETTING BIT: %d\n", *ptr);
    //printf("RES: %d\n", res);
    res = *ptr ^ res;
    *ptr = res;
    //printf("--- AFTER UNSETTING BIT: %d\n", *ptr);
}

void setSplitBit(size_t index)
{
    index += occupancyIndexBits;
    unsigned int bytes = index / 8;
    unsigned int bits = index % 8;

    uint8_t mask;
    uint8_t *ptr = bitmap;
    ptr += bytes;

    mask = 128 >> bits;
    *ptr = *ptr | mask;

    //printf("SPLIT BIT SET: %d\n", *ptr);
}

void unsetSplitBit(size_t index)
{
    index += occupancyIndexBits;
    unsigned int bytes = index / 8;
    unsigned int bits = index % 8;

    uint8_t res = 128 >> bits;
    uint8_t *ptr = bitmap;
    ptr += bytes;
    //printf("--- BEFORE UNSETTING SPLIT BIT: %d\n", *ptr);
    //printf("RES: %d\n", res);
    res = *ptr ^ res;
    *ptr = res;
    //printf("--- AFTER UNSETTING SPLIT BIT: %d\n", *ptr);
}

bool createLowestLevelBlocks()
{
    if (lowestLevel == highestLevel)
    {
        //printf("**** LOWEST IS HIGHEST!!!\n");
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
            if (!isBlockSplit(splitIndex))
                setSplitBit(splitIndex);

            //if (isBlockFree(splitIndex))
            //    setIndexBit(splitIndex);

            //printf("SPLITTING INDEX %zu\n", splitIndex);
            splitCounts[curLevel] += 1;

            //printBlocks(lowestLevel);

            if (curLevel - 1 == lowestLevel)
                return true;

            curLevel = lowestLevel + 1;
        }
    }

    //printf("/// WENT BEHIND WHILE!!!\n");
    return false;
}

bool removePreallocatedBlocksFromList(unsigned int numBlocks)
{
    //printf("HAVE TO REMOVE %d BLOCKS AT LOWEST LEVEL\n", numBlocks);
    if (numBlocks > freeBlocksOfLevel[lowestLevel])
    {
        //printf("NOT ENOUGH BLOCKS AT LOWEST LEVEL FOR REMOVAL\n");
        return false;
    }

    if (freeBlocksOfLevel[lowestLevel] == 0)
    {
        //printf("NOTHING IN LOWEST LEVEL\n");
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
    size_t usedIndex = 0;

    for (unsigned int i = 0; i < numBlocks; i++)
    {
        usedIndex = pow(2, highestLevel - lowestLevel) + splitCounts[lowestLevel] - 1;
        splitCounts[lowestLevel] += 1;
        //printf("USED INDEX: %zu\n", usedIndex);
        if (isBlockFree(usedIndex))
            setIndexBit(usedIndex);
        //isBlockFree(usedIndex);
    }

    return true;
}

bool initSeq3()
{
    //lists[highestLevel] = (Block *) realMemoryStart;
    //lists[highestLevel]->next = NULL;
    //lists[highestLevel]->size = highestLevelBlockSize;
    freeBlocksOfLevel[highestLevel] = 1;

    //printf("lists[HL]->size = %d\n", lists[highestLevel]->size);
    //printf("PREALL BLOCKS BUDE: %d\n", numBlocks);

    //create numBlocks free blocks of lowest level
    while (freeBlocksOfLevel[lowestLevel] < fillerBlocks)
    {
        createLowestLevelBlocks();
        //printf("**** AFTER CREATION OF SOME BLOCKS\n");
    }

    return true;
}

//sets the correct memory pointers into place
void initSeq4()
{
    uint8_t *startAt = heapMemoryStart;
    //printf("HEAPMEMORY STARTS AT ADDRESS %ld\n", startAt - realMemoryStart);
    size_t i = 0;
    size_t blockIndex = 0, blockIndexInLevel = 0, parentIndex = 0;

    while (freeBlocksOfLevel[i] == 0 && i < LEVELS)
    {
        lists[i] = NULL;
        i++;
    }

    //first nonempty level
    while (i < LEVELS)
    {
        if (freeBlocksOfLevel[i] != 0)
        {
            //ONE FREE BLOCK AT THIS EVEL
            lists[i] = (Block *)startAt;
            //printf("BLOCK OF LEVEL %zu STARTS AT ADDRESS: %ld\n", i, startAt - realMemoryStart);
            lists[i]->prev = NULL;
            lists[i]->next = NULL;

            //split parent
            blockIndexInLevel = (startAt - realMemoryStart + fillerBlocks * lowestLevelBlockSize) / (size_t)pow(2, i);
            blockIndex = (1 << (highestLevel - i)) + blockIndexInLevel - 1;
            parentIndex = (size_t)floor((blockIndex - 1) / 2);

            if (!isBlockSplit(parentIndex))
                setSplitBit(parentIndex);

            startAt += (size_t)pow(2, i);
            i++;
        }
        else
        {
            //NO FREE BLOCKS AT THIS LEVEL
            lists[i] = NULL;
            //printf("BLOCK OF LEVEL %zu - WOULD START - AT ADDRESS: %ld\n", i, startAt - realMemoryStart);
            i++;
        }
    }
}

void printSplits()
{
    size_t bits = (size_t)pow(2, (highestLevel - lowestLevel)) * 2 - 1;
    for (size_t i = 0; i < bits; i++)
    {
        isBlockSplit(i);
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

    //printf("**** AFTER SEQ 1\n");

    /**
     * set top-level free list
     * set my infoblocks as allocted (index blocks, filler blocks) in the index bitfields
     */
    //initSeq2();

    //printf("**** AFTER SEQ 2\n");

    /**
     * allocate x+y 16-B blocks (x = # of index blocks, y = # of filler blocks)
     */
    initSeq3();

    //printf("**** AFTER SEQ 3\n");

    //printf("--------INFO--------\n");

    //for (unsigned int i = 0; i < LEVELS; i++)
    //    printf("LIST OF POWER %d HAS - %d - FREE BLOCKS\n", i, freeBlocksOfLevel[i]);

    //printBlocks(lowestLevel);

    //printf("--- HAVE TO REMOVE %d BLOCKS AT LOWEST LEVEL ---\n", indexesBlocks + fillerBlocks);

    removePreallocatedBlocksFromList(fillerBlocks);

    //printf("--------AFTER REMOVAL--------\n");

    //printf("\n");
    //for (unsigned int i = 0; i < LEVELS; i++)
    //    printf("LIST OF POWER %d HAS - %d - FREE BLOCKS\n", i, freeBlocksOfLevel[i]);
    //printf("\n");

    //printBlocks(lowestLevel);

    //printf("SETTING FINAL MEMORY POINTERS\n");

    //set the final memory pointers
    initSeq4();
    //printf("**** AFTER SEQ 4\n");

    //printf("MEMORY SET AND PREPARED FOR USE\n");

    //printf("PRINTING SPLITS\n");

    //printSplits();
}

bool createBlocksOfLevel(size_t level)
{
    if (level == highestLevel)
    {
        //printf("**** THERE IS NO HIGHER LEVEL!!!\n");
        return false;
    }

    unsigned int curLevel = level + 1;
    size_t splitIndex = 0, newSize = 0;
    //printf("**** CUR_LEVEL: %d\n", curLevel);
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

            if (lists[curLevel] != NULL)
                lists[curLevel]->prev = NULL;

            //lists[curLevel - 1]->size /= 2;
            newSize = pow(2, curLevel - 1);

            ptr = (uint8_t *)lists[curLevel - 1];
            ptr += newSize;

            lists[curLevel - 1]->next = (Block *)ptr;
            //lists[curLevel - 1]->next->size = newSize;
            lists[curLevel - 1]->next->next = NULL;
            lists[curLevel - 1]->next->prev = lists[curLevel - 1];

            freeBlocksOfLevel[curLevel] -= 1;
            freeBlocksOfLevel[curLevel - 1] += 2;

            splitIndex = (1 << (highestLevel - curLevel)) + (((uint8_t *)lists[curLevel - 1] - realMemoryStart + fillerBlocks * lowestLevelBlockSize) / pow(2, curLevel)) - 1;

            //printf("highestLevel: %d, curLevel: %d, lists[curLevel - 1]: %d, realMemoryStart: %d\n", highestLevel, curLevel, lists[curLevel - 1], realMemoryStart);

            //printSplits();
            if (!isBlockSplit(splitIndex))
                setSplitBit(splitIndex);
            //printf("SPLITTING INDEX: %zu\n", splitIndex);
            //splitCounts[curLevel] += 1;

            //printSplits();

            //printBlocks(lowestLevel);

            if (curLevel - 1 == level)
                return true;

            curLevel = level + 1;
        }
    }

    //printf("/// UNABLE TO ALLOCATE BLOCK!!!\n");
    return false;
}

void printAllocated()
{
    size_t bits = (size_t)pow(2, (highestLevel - lowestLevel)) * 2 - 1;
    for (size_t i = 0; i < bits; i++)
    {
        isBlockFree(i);
    }
}

void *HeapAlloc(int size)
{
    size_t blockLevel = log2(nearestHigherPower(size)), index = 0, indexInLevel = 0;
    //printf("NHP LEVEL is: %zu\n", blockLevel);
    Block *block = NULL;
    uint8_t *blockAddress = NULL;

    if (size > pow(2, highestLevel - 1))
    {
        //printf("moc velke\n");
        return NULL;
    }

    while (freeBlocksOfLevel[blockLevel] == 0)
    {
        //printf("*** BLOCK OF ADEQUATE SIZE NOT AVAILABLE ***\n");
        //create block of size nhp
        if (!createBlocksOfLevel(blockLevel))
        {
            //printf("COULD NOT CREATE A DESIRED BLOCK\n");
            return NULL;
        }

        //printf("CREATED BLOCKS OF DESIRED SIZE\n");

        //printf("\n");
        //for (unsigned int i = 0; i < LEVELS; i++)
        //    printf("LIST OF POWER %d HAS - %d - FREE BLOCKS\n", i, freeBlocksOfLevel[i]);
        //printf("\n");
    }

    //printf("MYSLI SI TO\n");

    //there is a free block of a correct size for allocation already

    block = lists[blockLevel];
    blockAddress = (uint8_t *)block;

    //only one block in free list
    if (lists[blockLevel]->next == NULL)
    {
        lists[blockLevel] = NULL;
    }
    else
    {
        //more than one block in free list
        lists[blockLevel] = lists[blockLevel]->next;
        lists[blockLevel]->prev = NULL;
    }

    allocatedBlocks += 1;
    freeBlocksOfLevel[blockLevel] -= 1;
    indexInLevel = (blockAddress - realMemoryStart + fillerBlocks * lowestLevelBlockSize) / (size_t)pow(2, blockLevel);
    index = (1 << (highestLevel - blockLevel)) + indexInLevel - 1;

    //isBlockFree(index);

    //printf("ALLOCATING INDEX %zu\n", index);
    if (isBlockFree(index))
        setIndexBit(index);

    //isBlockFree(index);

    //printf("OVERENI ALOKOVANOSTI BLOKU %zu\n", index);

    //printAllocated();

    return (void *)block;
}

bool isParentSplit(size_t blockIndex)
{
    //printf("CURRENT BLOCK INDEX: %zu\n", blockIndex);
    size_t parentIndex = (size_t)floor((blockIndex - 1) / 2);
    //printf("PARENT INDEX: %zu\n", parentIndex);
    size_t parentSplitIndex = parentIndex + occupancyIndexBits;
    //printf("PARENT SPLIT INDEX: %zu\n", parentSplitIndex);

    unsigned int bytes = parentSplitIndex / 8;
    unsigned int bits = parentSplitIndex % 8;

    uint8_t mask = 128 >> bits;
    //printf("BYTES: %d\n", bytes);
    uint8_t res = bitmap[bytes] & mask;

    if (res == 0)
        return false;
    return true;
}

void finishMergingUp(Block *block, size_t blockIndex, unsigned int blockLevel);

void mergeBlocks(uint8_t *leftBlock, uint8_t *rightBlock, unsigned int destLevel, size_t newBlockIndex)
{
    Block *newHigherLevelBlock = (Block *)leftBlock;
    //isBlockSplit(newBlockIndex);
    //if (!isBlockFree(newBlockIndex))
    //    unsetIndexBit(newBlockIndex);

    if (isBlockSplit(newBlockIndex))
        unsetSplitBit(newBlockIndex);

    //printf("INFO IS PARENT SPLIT\n");
    //isBlockSplit(newBlockIndex);

    //printf("MERGED BLOCK INDEX: %zu\n", newBlockIndex);

    //merge up
    finishMergingUp(newHigherLevelBlock, newBlockIndex, destLevel);
}

void finishMergingUp(Block *block, size_t blockIndex, unsigned int blockLevel)
{
    size_t buddyIndex = 0;
    uint8_t *blockStart = (uint8_t *)block;
    //printf("---- FINISHING ----\n");
    //printf("BLOCK INDEX: %zu\n", blockIndex);

    //printf("BLOCK ADDRESS START: %ld\n", blockStart - realMemoryStart);

    if (blockIndex == 0)
    {
        freeBlocksOfLevel[blockLevel] += 1;
        freeBlocksOfLevel[blockLevel - 1] -= 1;

        //printf("FREE BLOCKS OF LEVEL %d: %d\n", blockLevel, freeBlocksOfLevel[blockLevel]);
        //printf("FREE BLOCKS OF LEVEL %d: %d\n", blockLevel - 1, freeBlocksOfLevel[blockLevel - 1]);

        //allocatedBlocks -= 1;
        //if (!isBlockFree(blockIndex))
        //    unsetIndexBit(blockIndex);

        if (isBlockSplit(blockIndex))
            unsetSplitBit(blockIndex);

        if (lists[blockLevel] == NULL)
        {
            //printf("EMPTY\n");
            lists[blockLevel] = block;
            block->prev = NULL;
            block->next = NULL;
        }
        else
        {
            //printf("SOMETHIN IN THERE\n");
            block->next = lists[blockLevel];
            lists[blockLevel]->prev = block;
            lists[blockLevel] = block;
            block->prev = NULL;
        }

        return;
    }

    if (blockIndex % 2 == 0)
    {
        buddyIndex = blockIndex - 1;
        //printf("---- FINISH: BUDDY INDEX: %zu\n", buddyIndex);
        if (isBlockFree(buddyIndex) && !isBlockSplit(buddyIndex))
        {
            //printf("TST, BLOCKINDEX: %zu\n", blockIndex);
            freeBlocksOfLevel[blockLevel - 1] -= 1;
            //printf("FREE OF %d = %d\n", blockLevel - 1, freeBlocksOfLevel[blockLevel - 1]);
            //printf("BUDDY %zu IS FREE\n", buddyIndex);
            mergeBlocks((uint8_t *)(blockStart - (size_t)pow(2, blockLevel)), blockStart, blockLevel + 1, (size_t)floor((blockIndex - 1) / 2));

            //allocatedBlocks -= 1;
        }
        else
        {
            //printf("BUDDY %zu OF %zu CANNOT MERGE, UNINDEXING %zu\n", buddyIndex, blockIndex, blockIndex);
            freeBlocksOfLevel[blockLevel] += 1;
            freeBlocksOfLevel[blockLevel - 1] -= 1;
            //allocatedBlocks -= 1;
            //if (!isBlockFree(blockIndex))
            //    unsetIndexBit(blockIndex);

            if (isBlockSplit(blockIndex))
                unsetSplitBit(blockIndex);

            if (lists[blockLevel] == NULL)
            {
                lists[blockLevel] = block;
                block->prev = NULL;
                block->next = NULL;
            }
            else
            {
                block->next = lists[blockLevel];
                lists[blockLevel]->prev = block;
                lists[blockLevel] = block;
                block->prev = NULL;
            }
        }
    }
    else
    {
        buddyIndex = blockIndex + 1;
        //printf("---- FINISH: BUDDY INDEX: %zu\n", buddyIndex);
        if (isBlockFree(buddyIndex) && !isBlockSplit(buddyIndex))
        {
            freeBlocksOfLevel[blockLevel - 1] -= 1;
            //printf("BUDDY %zu IS FREE\n", buddyIndex);
            mergeBlocks(blockStart, (uint8_t *)(blockStart + (size_t)pow(2, blockLevel)), blockLevel + 1, (size_t)floor((blockIndex - 1) / 2));

            //allocatedBlocks -= 1;
        }
        else
        {
            //printf("BUDDY %zu OF %zu CANNOT MERGE, UNINDEXING %zu\n", buddyIndex, blockIndex, blockIndex);
            freeBlocksOfLevel[blockLevel] += 1;
            freeBlocksOfLevel[blockLevel - 1] -= 1;
            //allocatedBlocks -= 1;
            //if (!isBlockFree(blockIndex))
            //    unsetIndexBit(blockIndex);

            if (isBlockSplit(blockIndex))
                unsetSplitBit(blockIndex);

            if (lists[blockLevel] == NULL)
            {
                lists[blockLevel] = block;
                block->prev = NULL;
                block->next = NULL;
            }
            else
            {
                block->next = lists[blockLevel];
                lists[blockLevel]->prev = block;
                lists[blockLevel] = block;
                block->prev = NULL;
            }
        }
    }
}

bool freeBlock(uint8_t *block)
{
    unsigned int indexInLastLevel = (block - realMemoryStart + fillerBlocks * lowestLevelBlockSize) / lowestLevelBlockSize;
    //printf("INDEX IN LAST LEVEL: %d\n", indexInLastLevel);
    unsigned int index = (1 << (highestLevel - lowestLevel)) + indexInLastLevel - 1;
    //printf("BLOCK INDEX IN INDEX: %d\n", index);

    if (block == realMemoryStart + realMemorySize)
        return false;

    //parent starts at different address -> pointer points to the correct (lowest-level) block
    if (indexInLastLevel % 2 != 0)
    {
        if (isBlockFree(index))
            return false;

        //MERGING!!!?
        Block *blk = (Block *)block;
        Block *buddy = NULL;

        if (isBlockFree(index - 1))
        {
            //remove my free budy from free list
            buddy = (Block *)(block - lowestLevelBlockSize);

            //remove buddy from free list
            //all buddy location options
            if (buddy->prev == NULL && buddy->next != NULL)
            {
                lists[lowestLevel] = buddy->next;
                lists[lowestLevel]->prev = NULL;
            }
            else if (buddy->prev == NULL && buddy->next == NULL)
            {
                lists[lowestLevel] = NULL;
            }
            else if (buddy->prev != NULL && buddy->next == NULL)
            {
                buddy->prev->next = NULL;
            }
            else if (buddy->prev != NULL && buddy->next != NULL)
            {
                buddy->prev->next = buddy->next;
                buddy->next->prev = buddy->prev;
            }
            //both buddies gone from free list

            if (!isBlockFree(index))
                unsetIndexBit(index);
            //block that is being freed gets 0 to his index (he is free now)
            //freeBlocksOfLevel[lowestLevel] -= 1;
            //allocatedBlocks -= 1;

            //merge + update all info
            mergeBlocks((uint8_t *)buddy, block, lowestLevel + 1, floor((index - 1) / 2));
            return true;
        }

        //adding freed block to the beginning of the free list

        if (lists[lowestLevel] == NULL)
        {
            lists[lowestLevel] = blk;
            blk->prev = NULL;
            blk->next = NULL;
        }
        else
        {
            blk->next = lists[lowestLevel];
            lists[lowestLevel]->prev = blk;
            lists[lowestLevel] = blk;
            blk->prev = NULL;
        }

        freeBlocksOfLevel[lowestLevel] += 1;
        //allocatedBlocks -= 1;

        if (!isBlockFree(index))
            unsetIndexBit(index);
        return true;
    }
    else
    {
        //even index in last level
        unsigned int level = lowestLevel;
        size_t parentIndex = floor((index - 1) / 2);

        //printf("SEARCHING FOR REAL BLOCK\n");

        while (!isParentSplit(index))
        {
            //printf("PARENT OF INDEX %d IS - NOT - SPLIT\n", index);
            index = floor((index - 1) / 2);
            level++;
        }

        //printf("PARENT OF INDEX %d - IS - SPLIT\n", index);

        //parent is split, newIndex is the wanted block index
        //printf("PARENT - IS - SPLIT\n");
        //printf("INDEX TO FREE IS: %zu\n", toFree);
        //printf("INDEX NEWINDEX IS: %zu\n", newIndex);

        //printf("INDEX TO BE FREED: %d\n", index);

        //printAllocated();

        if (isBlockFree(index))
        {
            //printf("HEHEHE/////////////////\n");
            return false;
        }

        Block *blk = (Block *)block;
        Block *buddy = NULL;
        size_t buddyIndex = 0;

        if (index % 2 == 0)
            buddyIndex = index - 1;
        else
            buddyIndex = index + 1;

        parentIndex = (index - 1) / 2;

        //printf("BEFORE MERGING: INDEX=%d, BUDDYINDEX=%zu\n", index, buddyIndex);

        //MERGING!!!?
        if (isBlockFree(buddyIndex) && !isBlockSplit(buddyIndex))
        {
            //printf("---- JE FREE ----\n");

            //remove my free budy from free list
            if (index % 2 == 0)
                buddy = (Block *)(block - (size_t)(pow(2, level)));
            else
                buddy = (Block *)(block + (size_t)(pow(2, level)));

            //buddy is first in list
            if (buddy->prev == NULL && buddy->next != NULL)
            {
                //printf("1\n");
                lists[level] = buddy->next;
                lists[level]->prev = NULL;
            }
            else if (buddy->prev == NULL && buddy->next == NULL)
            {
                //printf("2\n");
                //printf("LEVEL: %d, PARENT INDEX:%zu\n", level, parentIndex);
                lists[level] = NULL;
            }
            else if (buddy->prev != NULL && buddy->next == NULL)
            {
                //printf("3\n");
                buddy->prev->next = NULL;
            }
            else if (buddy->prev != NULL && buddy->next != NULL)
            {
                //printf("4\n");
                buddy->prev->next = buddy->next;
                buddy->next->prev = buddy->prev;
            }
            //buddies both gone from free list

            if (!isBlockFree(index))
                unsetIndexBit(index);

            //freeBlocksOfLevel[level] -= 1;
            //allocatedBlocks -= 1;
            //isBlockFree(index);
            //printf("PARENT INDEX IS: %zu\n", parentIndex);

            //merge + update all info
            if (index % 2 == 0)
                mergeBlocks(block - (size_t)(pow(2, level)), block, level + 1, parentIndex);
            else
                mergeBlocks(block, block + (size_t)(pow(2, level)), level + 1, parentIndex);

            return true;
        }

        //printf("UVOLNOVANI BLOKU LEVELU %d\n", level);

        if (lists[level] == NULL)
        {
            lists[level] = blk;
            blk->prev = NULL;
            blk->next = NULL;
        }
        else
        {
            blk->next = lists[level];
            lists[level]->prev = blk;
            lists[level] = blk;
            blk->prev = NULL;
        }

        //printf("--- DEALLOCATED INDEX %d NO MERGE ---\n", index);

        if (!isBlockFree(index))
            unsetIndexBit(index);

        freeBlocksOfLevel[level] += 1;
        //allocatedBlocks -= 1;

        //printAllocated();
        return true;
    }
}

bool HeapFree(void *blk)
{
    uint8_t *block = (uint8_t *)blk;

    if (block < heapMemoryStart || block > (realMemoryStart + heapMemorySize))
    {
        //printf("ER1\n");
        return false;
    }

    if (((size_t)(block - heapMemorySize) % lowestLevelBlockSize) != 0)
    {
        //printf("ER2\n");
        return false;
    }

    //now i know i have a ptr to some block (its a 16 mult.)
    //printf("IT SEEMS TO BE A BLOCK ...\n");

    bool res = freeBlock(block);

    if (!res)
        return false;

    allocatedBlocks -= 1;
    return true;
}

void HeapDone(int *pendingBlk)
{
    *pendingBlk = allocatedBlocks;
    //printf("PENDING BLOCKS: %d\n", allocatedBlocks);
}

#ifndef __PROGTEST__

int main(void)
{
    uint8_t *p0, *p1, *p2, *p3, *p4;
    int pendingBlk;
    static uint8_t memPool[3 * 1048576];

    HeapInit(memPool, 2097152);
    assert((p0 = (uint8_t *)HeapAlloc(512000)) != NULL);
    memset(p0, 0, 512000);
    assert((p1 = (uint8_t *)HeapAlloc(511000)) != NULL);
    memset(p1, 0, 511000);
    assert((p2 = (uint8_t *)HeapAlloc(26000)) != NULL);
    memset(p2, 0, 26000);
    HeapDone(&pendingBlk);
    assert(pendingBlk == 3);

    HeapInit(memPool, 2097152);
    assert((p0 = (uint8_t *)HeapAlloc(1000000)) != NULL);
    memset(p0, 0, 1000000);

    //printf("--- PRINT ALLOCATED A1 ---\n");
    //printAllocated();
    //printf("--- DONE ---\n");

    assert((p1 = (uint8_t *)HeapAlloc(250000)) != NULL);
    memset(p1, 0, 250000);

    //printf("--- PRINT ALLOCATED A2 ---\n");
    //printAllocated();
    //printf("--- DONE ---\n");

    assert((p2 = (uint8_t *)HeapAlloc(250000)) != NULL);
    memset(p2, 0, 250000);

    //printf("--- PRINT ALLOCATED A3 ---\n");
    //printAllocated();
    //printf("--- DONE ---\n");

    assert((p3 = (uint8_t *)HeapAlloc(250000)) != NULL);
    memset(p3, 0, 250000);

    //printf("--- PRINT ALLOCATED A4 ---\n");
    //printAllocated();
    //printf("--- DONE ---\n");

    assert((p4 = (uint8_t *)HeapAlloc(50000)) != NULL);
    memset(p4, 0, 50000);

    //printf("--- PRINT ALLOCATED A5 ---\n");
    //printAllocated();
    //printf("--- DONE ---\n");

    assert(HeapFree(p2));

    //printf("--- PRINT ALLOCATED F1 ---\n");
    //printAllocated();
    //printf("--- DONE ---\n");

    assert(HeapFree(p4));

    //printf("--- PRINT ALLOCATED F2 ---\n");
    //printAllocated();
    //printf("--- DONE ---\n");

    assert(HeapFree(p3));

    //printf("--- PRINT ALLOCATED F3 ---\n");
    //printAllocated();
    //printf("--- DONE ---\n");

    assert(HeapFree(p1));

    //printf("--- PRINT ALLOCATED F4 ---\n");
    //printAllocated();
    //isBlockSplit(2);
    //printf("--- DONE ---\n");

    //printf("\n");
    //for (unsigned int i = 0; i < LEVELS; i++)
    //    printf("LIST OF POWER %d HAS - %d - FREE BLOCKS\n", i, freeBlocksOfLevel[i]);
    //printf("\n");

    assert((p1 = (uint8_t *)HeapAlloc(500000)) != NULL);
    memset(p1, 0, 500000);

    //printf("\n");
    //for (unsigned int i = 0; i < LEVELS; i++)
    //    printf("LIST OF POWER %d HAS - %d - FREE BLOCKS\n", i, freeBlocksOfLevel[i]);
    //printf("\n");

    //printf("--- PRINT ALLOCATED A6 ---\n");
    //printAllocated();
    //printf("--- DONE ---\n");

    assert(HeapFree(p0));

    //printf("--- PRINT ALLOCATED F5 ---\n");
    //printAllocated();
    //printf("--- DONE ---\n");

    //printf("\n");
    //for (unsigned int i = 0; i < LEVELS; i++)
    //    printf("LIST OF POWER %d HAS - %d - FREE BLOCKS\n", i, freeBlocksOfLevel[i]);
    //printf("\n");

    assert(HeapFree(p1));

    //printf("--- PRINT ALLOCATED F6 ---\n");
    //printAllocated();
    //printf("--- DONE ---\n");

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

    return 0;
}

#endif /* __PROGTEST__ */
