#ifndef __PROGTEST__
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <random>
#include <iostream>
using namespace std;
#endif /* __PROGTEST__ */

#define BLOCK 16
#define TOPLEVEL 26

struct Block
{
	Block *prev;
	Block *next;
};

//how many blocks in level
unsigned int blkCnt[TOPLEVEL];

//how many below level
unsigned int blkSum[TOPLEVEL];

Block *lists[TOPLEVEL];

uint8_t *start;
uint8_t *fin;
uint8_t *bitfield;

size_t allocated = 0;
size_t topIndex = 0;

void print_heap()
{
	size_t b = 0;
	size_t level = 0;
	size_t bits_total = 0;
	size_t eo = 0;
	cout << endl;

	for (size_t i = 0; i < TOPLEVEL; i++)
		bits_total += blkCnt[i];

	string space;
	string wh = "\033[37;40m";
	string bl = "\033[30;47m";
	int val;

	while (b < bits_total)
	{
		val = ((bitfield[b / 8] >> (7 - (b % 8))) & 1);

		//if (val)
		//	cout << wh;
		//else
		//	cout << bl;

		cout << space << val;

		eo++;
		if (b == blkSum[level] + blkCnt[level] - 1)
		{
			level++;
			space = space + space + " ";
			//cout << "\033[0m" << endl;
			cout << endl;
			eo = 0;
		}

		b++;
	}

	//cout << "\033[0m" << endl;
	cout << endl;
}

size_t lti(size_t level)
{
	return level - 4;
}

void initBlock(Block *block)
{
	block->prev = NULL;
	block->next = NULL;
}

void addBlock(Block *block, int level)
{
	int idx = lti(level);
	initBlock(block);

	if (lists[idx] == NULL)
		lists[idx] = block;
	else
	{
		Block *oldRoot = lists[idx];
		lists[idx] = block;
		block->next = oldRoot;
		oldRoot->prev = block;
	}
}

Block *removeBlock(int level)
{
	int idx = lti(level);

	if (lists[idx] == NULL)
		return NULL;

	Block *removed = lists[idx];
	lists[idx] = removed->next;

	if (lists[idx])
		lists[idx]->prev = NULL;

	return removed;
}

void removeBlock(Block *block, int level)
{
	if (block->prev)
		block->prev->next = block->next;
	else // is surely first in the list
		lists[lti(level)] = block->next;

	if (block->next)
		block->next->prev = block->prev;
}

void setupBlock(size_t size)
{
	start = start - size;
	Block *newBlock = (Block *)(start);
	int level = log2(size);
	topIndex = topIndex < lti(level) ? lti(level) : topIndex;
	addBlock(newBlock, level);
}

void printList(Block *list)
{
	while (list != NULL)
	{
		printf("| ");
		list = list->next;
	}
	printf("\n");
}

void printLists()
{
	for (unsigned char level = 0; level < TOPLEVEL; level++)
	{
		printf("power %d: ", level + 4);
		printList(lists[level]);
	}
}

void setBlocks(size_t bits, size_t memSize)
{
	size_t size = 0, heapBits = 0, heapBytes = 0, extraBits = 0, extraBytes = 0;

	//find matching block
	size = 1 << (int)floor(log2(memSize));

	while (true)
	{
		//# blocks
		heapBits = (size / 16) * 2;

		if (!heapBits)
			return;

		//no bits are wasted!!!
		heapBits -= 1;
		heapBytes = heapBits / 8 + 1;
		extraBits = bits + (8 - heapBits % 8);
		extraBytes = extraBits / 8;

		if (size + heapBytes > memSize + extraBytes)
		{
			size /= 2; // resize
			if (size <= BLOCK)
				return;
		}
		else
		{
			blkCnt[0] += size / BLOCK;
			setupBlock(size);
			return setBlocks(extraBits % 8, memSize + extraBytes - size - heapBytes);
		}
	}
}

void countBlocks()
{
	int i = 0;
	while (i < TOPLEVEL - 1)
	{
		if (blkCnt[i] == 0)
			break;

		blkCnt[i + 1] = blkCnt[i] / 2;
		blkSum[i + 1] = blkSum[i] + blkCnt[i];
		i++;
	}
}

void HeapInit(void *memPool, int memSize)
{
	memset(blkCnt, 0, TOPLEVEL * sizeof(unsigned int));
	memset(blkSum, 0, TOPLEVEL * sizeof(unsigned int));
	memset(lists, 0, TOPLEVEL * sizeof(Block *));

	start = (uint8_t *)memPool + memSize;
	fin = (uint8_t *)memPool + memSize;
	bitfield = (uint8_t *)memPool;
	allocated = 0;
	topIndex = 0;

	setBlocks(0, memSize);
	countBlocks();
	//printLists();
}

#define NOAVAILABLE 0
#define AVAILABLE 1

int splitParent(int level) // I am spliting the parent of the level
{
	cout << "-- splitting --" << endl;
	if (lti(level + 1) > topIndex)
		return NOAVAILABLE;

	if (lists[lti(level + 1)] == NULL)
		if (!splitParent(level + 1))
			return NOAVAILABLE;

	Block *parent = removeBlock(level + 1);

	cout << "removed a parent starting at address " << (uint8_t *)parent - start << " from level " << level + 1 << endl;

	addBlock(parent, level);
	addBlock((Block *)(((uint8_t *)parent) + (size_t)(1 << level)), level);

	cout << "added blocks starting on address " << (uint8_t *)parent - start << " and " << ((uint8_t *)parent + (size_t)(1 << level)) - start << " into level " << level << endl;

	return AVAILABLE;
}

Block *getBlock(int level)
{
	//if available, use
	Block *block = removeBlock(level);

	if (block)
		return block;

	//NULL returned
	cout << "block not available so far ... trying split" << endl;

	//otherwise split upper available
	if (!splitParent(level))
		//if no upper available return null
		return NULL;

	cout << "did a split" << endl;
	block = removeBlock(level);

	if (block)
	{
		cout << "created a block by slitting" << endl;
		return block;
	}

	cout << "did not get a block" << endl;
	return NULL;
}

void flipBitOn(size_t bitsOffset)
{
	size_t bytes = bitsOffset / 8;
	size_t bits = bitsOffset % 8;
	uint8_t byte = bitfield[bytes];
	byte |= 128 >> bits;
	bitfield[bytes] = byte;
}

void flipBitOff(size_t bitsOffset)
{
	size_t bytes = bitsOffset / 8;
	size_t bits = bitsOffset % 8;
	uint8_t byte = bitfield[bytes];
	byte ^= 128 >> bits; //xor
	bitfield[bytes] = byte;
}

int isBitOn(size_t bitsOffset)
{
	size_t bytes = bitsOffset / 8;
	size_t bits = bitsOffset % 8;
	uint8_t byte = bitfield[bytes];
	return byte & (128 >> bits);
}

int blockLevelIdx(Block *block, int level)
{
	return ((uint8_t *)block - start) / (1 << level);
}

int blockBit(Block *block, int level)
{
	int levelIdx = blockLevelIdx(block, level);
	return blkSum[lti(level)] + levelIdx;
}

void setBlockUnused(Block *block, int level)
{
	int bitsOffset = blockBit(block, level);
	flipBitOff(bitsOffset);
}

void setBlockUsed(Block *block, int level)
{
	int bitsOffset = blockBit(block, level);
	flipBitOn(bitsOffset);
}

bool blockIsInFreeList(Block *block, int level)
{
	Block *blk = lists[lti(level)];

	while (blk != NULL)
	{
		if (blk == block)
		{
			cout << "blok je ve free listu" << endl;
			return true;
		}

		blk = blk->next;
	}

	cout << "blok je ve free listu" << endl;
	return false;
}

int isBlockUsed(Block *block, int level)
{
	int bitsOffset = blockBit(block, level);
	return isBitOn(bitsOffset);
}

void *HeapAlloc(int size)
{
	if (size < 16)
		size = 16;

	//go from the bottom
	int level = ceil(log2(size));

	//cout << "getting a block of level: " << level << endl;

	//find fit
	Block *block = getBlock(level);

	if (block == NULL)
	{
		cout << "could not allocate anything" << endl;
		return NULL;
	}

	allocated++;
	setBlockUsed(block, level);
	return (void *)block;
}

//works only if the pointer is really a block in the lvel
Block *findParent(Block *block, int level)
{
	unsigned int levelCnt = blkCnt[lti(level)];
	int levelIdx = blockLevelIdx(block, level);

	if (levelCnt % 2) //odd
	{
		if (levelIdx % 2) // me is odd
			return block;
		else
			return (Block *)(((uint8_t *)block) - (1 << level));
	}
	else
	{					  //even
		if (levelIdx % 2) // me is odd
			return (Block *)(((uint8_t *)block) - (1 << level));
		else
			return block;
	}
}

Block *findBuddy(Block *block, int level)
{
	unsigned int levelCnt = blkCnt[lti(level)];
	int levelIdx = blockLevelIdx(block, level);

	if (levelCnt % 2) //odd
	{
		if (levelIdx % 2) // me is odd
			return (Block *)(((uint8_t *)block) + (1 << level));
		else
			return (Block *)(((uint8_t *)block) - (1 << level));
	}
	else
	{					  //even
		if (levelIdx % 2) // me is odd
			return (Block *)(((uint8_t *)block) - (1 << level));
		else
			return (Block *)(((uint8_t *)block) + (1 << level));
	}
}

bool exists(uint8_t *block)
{
	cout << "free a block that starts at address: " << block - start << endl;
	return (block >= start) && (block < fin) && ((block - start) % 16 == 0);
}

int findLevel(Block *block)
{
	size_t level = 4; // lowest
	Block *bl;

	while (lti(level) <= topIndex)
	{
		if (isBlockUsed(block, level))
			return level;

		bl = findParent(block, level);

		if (bl != block)
			return -1;

		block = bl;
		level++;
	}

	return -1;
}

void merge(Block *blk, int level)
{
	if (lti(level) > topIndex)
		return;

	Block *buddy = findBuddy(blk, level);

	if (!exists((uint8_t *)buddy))
		return;

	if (isBlockUsed(buddy, level))
		return;

	if (!blockIsInFreeList(buddy, level))
		return;

	removeBlock(buddy, level);
	removeBlock(blk, level);

	Block *merged = buddy < blk ? buddy : blk;
	addBlock(merged, level + 1);
	merge(merged, level + 1);
}

bool HeapFree(void *blk)
{
	if (!exists((uint8_t *)blk))
	{
		cout << "no block starts at this address" << endl;
		return false;
	}

	int level = findLevel((Block *)blk);

	if (level == -1)
	{
		cout << "the block is already free" << endl;
		return false;
	}

	addBlock((Block *)blk, level);

	setBlockUnused((Block *)blk, level);

	merge((Block *)blk, level);

	allocated--;
	cout << "freed a block starting at address: " << (uint8_t *)blk - start << "\n"
		 << endl;
	return true;
}

void HeapDone(int *pendingBlk)
{
	(*pendingBlk) = allocated;
}

#ifndef __PROGTEST__

//generators

void testHeap(size_t blocks, size_t maxSize)
{
	static uint8_t memPool[3 * 1048576];
	uint8_t **ptrArray = new uint8_t *[blocks];
	size_t totalSize = 0;
	int heapMem = 2048;

	HeapInit(memPool, heapMem);

	size_t heapSize = fin - start;
	size_t randomSize, blockSize;
	srand(time(0));

	for (size_t i = 0; i < blocks; i++)
	{
		randomSize = rand() % maxSize + 1;
		blockSize = (size_t)(1 << (int)ceil(log2(randomSize)));

		if (blockSize < 16)
			blockSize = 16;

		cout << "WANT: " << randomSize << " "
			 << " WILL ALLOCATE: " << blockSize << " FREE: " << heapSize - totalSize << endl;
		totalSize += blockSize;
		assert((ptrArray[i] = (uint8_t *)HeapAlloc(randomSize)) != NULL);
		memset(ptrArray[i], 0, randomSize);

		cout << "--- ALLOCATED: " << totalSize << " " << heapSize << endl;
	}

	cout << "--- TOTAL: " << totalSize << endl;
}

#define MAPSIZE 512
#define USED 'o'
#define START '*'
#define END '#'

void print_mem(char *mem, size_t size, size_t mark)
{

	for (size_t i = 0; i < size; i++)
	{
		if (i % 20 == 0)
			cout << "\n";

		if (i == mark)
			cout << '^';
		else
			cout << mem[i];
	}
	cout << endl;
}

void tester()
{
	char map[MAPSIZE];
	memset(map, '.', MAPSIZE);
	static uint8_t memPool[MAPSIZE];
	memset(memPool, 0, MAPSIZE);

	HeapInit(memPool, MAPSIZE);

	cout << "\nLISTS START" << endl;
	printLists();
	cout << "LISTS END\n"
		 << endl;

	srand(time(0));
	random_device rd;
	mt19937 gen = mt19937(rd());
	uniform_int_distribution<int> bin(0, 1);
	uniform_int_distribution<int> rnd_block(4, 128);
	uniform_int_distribution<int> addr(0, MAPSIZE);

	size_t request, address, all, allocated, used;
	uint8_t *mem;

	while (true)
	{
		bool alloc = bin(gen);
		request = 0;
		address = 0;
		all = 0;
		allocated = 0;
		used = 0;
		mem = NULL;

		if (alloc)
		{
			request = rnd_block(gen);
			mem = (uint8_t *)HeapAlloc(request);
			cout << "alloc " << request << endl;

			if (mem == NULL)
			{
				//nothing returned
				cout << "did not allocate anything" << endl;
			}
			else
			{
				//check range
				if (mem < memPool || mem >= memPool + MAPSIZE)
				{
					cout << "err range" << endl;
					//exit(1);
				}

				//check usage
				used += request;
				all = 1 << (int)(ceil(log2(request)));
				all = all > 16 ? all : 16;
				cout << "expected blk " << all << endl;
				allocated += all;
				address = (mem - memPool);
				cout << "allocated block starting at address: " << address << endl;

				for (size_t i = 0; i < all; i++)
				{
					if (map[address + i] == USED || map[address + i] == START || map[address + i] == END)
					{
						cout << "err used" << endl;
						//exit(1);
					}
					else
					{
						map[address + i] = USED;
						if (i == 0)
							map[address + i] = START;
						else if (i == all - 1)
							map[address + i] = END;
					}
				}
			}
			print_mem(map, MAPSIZE, MAPSIZE + 1);
		}
		else
		{
			//address = addr(gen);
			//mem = memPool + address;

			address = (rand() % (MAPSIZE / 16)) * 16;
			mem = memPool + address;

			cout << "free " << address << " " << mem << endl;

			if (HeapFree(mem))
			{
				if (map[address] == START)
				{
					while (map[address] != END)
					{
						map[address++] = '.';
					}
					map[address] = '.';
				}
				else
				{
					cout << "err free 1" << endl;
					//exit(1);
				}
			}
			else
			{
				if (map[address] == START)
				{
					cout << "err free 2" << endl;
					//exit(1);
				}
			}
			print_mem(map, MAPSIZE, address);
		}

		print_heap();

		cout << "\nLISTS START" << endl;
		printLists();
		cout << "LISTS END\n"
			 << endl;

		cin.get();
	}
}

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
	assert((p1 = (uint8_t *)HeapAlloc(250000)) != NULL);
	memset(p1, 0, 250000);
	assert((p2 = (uint8_t *)HeapAlloc(250000)) != NULL);
	memset(p2, 0, 250000);
	assert((p3 = (uint8_t *)HeapAlloc(250000)) != NULL);
	memset(p3, 0, 250000);
	assert((p4 = (uint8_t *)HeapAlloc(50000)) != NULL);
	memset(p4, 0, 50000);
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

	//-------------------------------------------------------------------

	cout << "--- TEST HEAP ---" << endl;

	//testHeap(140, 16);

	tester();

	return 0;
}
#endif /* __PROGTEST__ */
