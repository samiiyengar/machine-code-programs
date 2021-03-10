/**
 * Project 4
 * EECS 370 LC-2K Instruction-level simulator
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#define NUMMEMORY 65536 /* maximum number of words in memory */
#define NUMREGS 8 /* number of machine registers */
#define MAXLINELENGTH 1000
#define MIN_OFFSET -32768
#define MAX_OFFSET 32767
#define BITMASK_FOR_PARSING_MACHINE_CODE 0x00000007
#define BITMASK_BITS_ZERO_TO_FIFTEEN 0xFFFF
#define MAX_CACHE_SIZE 256
#define MAX_BLOCK_SIZE 256

typedef struct stateStruct {
    int pc;
    int mem[NUMMEMORY];
    int reg[NUMREGS];
    int numMemory;
} stateType;

typedef struct instructionData {
    int opcode;
    int arg0;
    int arg1;
    int arg2;
} instructionInfo;

enum cacheOperation {
    read,
    save
};

enum actionType
{
    cacheToProcessor,
    processorToCache,
    memoryToCache,
    cacheToMemory,
    cacheToNowhere
};

typedef struct blockStruct
{
    int data[MAX_BLOCK_SIZE];
    bool isDirty;
    int lruLabel;
    int set;
    int tag;
} blockStruct;

typedef struct cacheStruct
{
    blockStruct blocks[MAX_CACHE_SIZE];
    int blocksPerSet;
    int blockSize;
    int lru;
    int numSets;
} cacheStruct;

/* Global Cache variable */
cacheStruct cache;
int blockAccessTimestamp = 0;

void printState(stateType *);
int convertNum(int);
void exitProgram(const char* message);
int isValidRegister(int reg);
void initializeCache(void);
int load(int, stateType *);
void store(int, int, stateType *);
int getBlockHead(int);
int performCacheOperation(enum cacheOperation, int, int, stateType *);
double logBase2(int);
int generateMask(double);
bool existsInCache(int, int);
int loadFromCache(int, int, int);
void saveToCache(int, int, int, int);
int evictLRU(int, stateType *);

/*
 * Log the specifics of each cache action.
 *
 * address is the starting word address of the range of data being transferred.
 * size is the size of the range of data being transferred.
 * type specifies the source and destination of the data being transferred.
 *  -    cacheToProcessor: reading data from the cache to the processor
 *  -    processorToCache: writing data from the processor to the cache
 *  -    memoryToCache: reading data from the memory to the cache
 *  -    cacheToMemory: evicting cache data and writing it to the memory
 *  -    cacheToNowhere: evicting cache data and throwing it away
 */
void printAction(int address, int size, enum actionType type)
{
    printf("@@@ transferring word [%d-%d] ", address, address + size - 1);

    if (type == cacheToProcessor) {
        printf("from the cache to the processor\n");
    }
    else if (type == processorToCache) {
        printf("from the processor to the cache\n");
    }
    else if (type == memoryToCache) {
        printf("from the memory to the cache\n");
    }
    else if (type == cacheToMemory) {
        printf("from the cache to the memory\n");
    }
    else if (type == cacheToNowhere) {
        printf("from the cache to nowhere\n");
    }
}

/*
 * Prints the cache based on the configurations of the struct
 */
void printCache()
{
  printf("\n@@@\ncache:\n");

  for (int set = 0; set < cache.numSets; ++set) {
    printf("\tset %i:\n", set);
    for (int block = 0; block < cache.blocksPerSet; ++block) {
      printf("\t\t[ %i ]: {", block);
      for (int index = 0; index < cache.blockSize; ++index) {
        printf(" %i", cache.blocks[set * cache.blocksPerSet + block].data[index]);
      }
      printf(" }\n");
    }
  }

  printf("end cache\n");
}

void printState(stateType *statePtr) {
    int i;
    printf("\n@@@\nstate:\n");
    printf("\tpc %d\n", statePtr->pc);
    printf("\tmemory:\n");
    for (i=0; i<statePtr->numMemory; i++) {
        printf("\t\tmem[ %d ] %d\n", i, statePtr->mem[i]);
    }
    printf("\tregisters:\n");
    for (i=0; i<NUMREGS; i++) {
        printf("\t\treg[ %d ] %d\n", i, statePtr->reg[i]);
    }
    printf("end state\n");
}

int convertNum(int num) {
    /* convert a 16-bit number into a 32-bit Linux integer */
    if (num & (1<<15) ) {
        num -= (1<<16);
    }
    return(num);
}

void exitProgram(const char* message) {
    printf("\n%s\n", message);
    exit(1);
}

int isValidRegister(int reg) {
    return (reg >= 0 && reg < NUMREGS);
}

void initializeCache() {
    for (int i = 0; i < MAX_CACHE_SIZE; i++) {
        cache.blocks[i].lruLabel = 0;
        cache.blocks[i].set = 0;
        cache.blocks[i].tag = 0xdeadbeef;
        cache.blocks[i].isDirty = false;
        for (int j = 0; j < MAX_BLOCK_SIZE; j++) {
            cache.blocks[i].data[j] = 0;
        }
    }
}

int load(int addr, stateType *state) {
    return performCacheOperation(read, addr, 0, state);
}

void store(int addr, int val, stateType *state) {
    performCacheOperation(save, addr, val, state);
}

int getBlockHead(int addr) {
    return ((addr/cache.blockSize) * cache.blockSize);
}

int performCacheOperation(enum cacheOperation op, int addr, int val, stateType *state) {
    int blockOffset = addr & generateMask(logBase2(cache.blockSize));
    int setIndex = (addr >> (int)logBase2(cache.blockSize)) & generateMask(logBase2(cache.numSets));
    int tag = addr >> (int)(logBase2(cache.blockSize) + logBase2(cache.numSets));
    if (existsInCache(tag, setIndex)) {
        if (op == read) {
            printAction(addr, 1, cacheToProcessor);
            int data = loadFromCache(tag, setIndex, blockOffset);
            return data;
        } else {
            printAction(addr, 1, processorToCache);
            saveToCache(tag, val, setIndex, blockOffset);
            return val;
        }
    }
    bool foundValidBlock = false;
    int memBlockHead = getBlockHead(addr);
    while (!foundValidBlock) {
        int blockSetOffset = setIndex * cache.blocksPerSet;
        //printf("block set offset %d\n", blockSetOffset);
        for (int i = blockSetOffset; i < blockSetOffset + cache.blocksPerSet; i++) {
            if (cache.blocks[i].tag == 0xdeadbeef) {
                //printf("found deadbeef\n");

                printAction(memBlockHead, cache.blockSize, memoryToCache);
                int currentBlock = 0;
                for (int j = memBlockHead; j < memBlockHead + cache.blockSize; j++) {
                    cache.blocks[i].data[currentBlock++] = state->mem[j];
                }
                foundValidBlock = true;
                cache.blocks[i].tag = tag;
                cache.blocks[i].set = memBlockHead;
                blockAccessTimestamp++;
                cache.blocks[i].lruLabel = blockAccessTimestamp;
                if (op == read) {
                    printAction(addr, 1, cacheToProcessor);
                    return cache.blocks[i].data[blockOffset];
                } else {
                    printAction(addr, 1, processorToCache);
                    cache.blocks[i].data[blockOffset] = val;
                    cache.blocks[i].isDirty = true;
                    return val;
                }
            }
        }
        if (!foundValidBlock) {
            //printf("evicting");
            evictLRU(setIndex, state);
        }
    }
    return -1;
}

double logBase2(int n) {
    return log(n)/log(2);
}

int generateMask(double bits) {
    return (1 << (int)bits) - 1;
}

bool existsInCache(int tag, int setIndex) {
    int blockSetOffset = setIndex * cache.blocksPerSet;
    for (int i = blockSetOffset; i < blockSetOffset + cache.blocksPerSet; i++) {
        if (cache.blocks[i].tag == tag) {
            return true;
        }
    }
    return false;
}

int loadFromCache(int tag, int setIndex, int blockOffset) {
    int blockSetOffset = setIndex * cache.blocksPerSet;
    for (int i = blockSetOffset; i < blockSetOffset + cache.blocksPerSet; i++) {
        if (cache.blocks[i].tag == tag) {
            blockAccessTimestamp++;
            cache.blocks[i].lruLabel = blockAccessTimestamp;
            return cache.blocks[i].data[blockOffset];
        }
    }
    return -1;
}

void saveToCache(int tag, int value, int setIndex, int blockOffset) {
    int blockSetOffset = setIndex * cache.blocksPerSet;
    for (int i = blockSetOffset; i < blockSetOffset + cache.blocksPerSet; i++) {
        if (cache.blocks[i].tag == tag) {
            cache.blocks[i].data[blockOffset] = value;
            cache.blocks[i].isDirty = true;
            blockAccessTimestamp++;
            cache.blocks[i].lruLabel = blockAccessTimestamp;
            break;
        }
    }
}

int blockComparator(const void *block1, const void *block2) {
    blockStruct* a = (blockStruct*)block1;
    blockStruct* b = (blockStruct*)block2;
    return a->lruLabel - b->lruLabel;
}

int evictLRU(int setIndex, stateType *state) {
    int blockSetOffset = setIndex * cache.blocksPerSet;
    qsort(&cache.blocks[blockSetOffset], cache.blocksPerSet, sizeof(blockStruct), blockComparator);
    int memBlockHead = cache.blocks[blockSetOffset].set;
    cache.blocks[blockSetOffset].tag = 0xdeadbeef;
    if (cache.blocks[blockSetOffset].isDirty) {
        printAction(memBlockHead, cache.blockSize, cacheToMemory);
        int currentBlock = 0;
        for (int j = memBlockHead; j < memBlockHead + cache.blockSize; j++) {
            state->mem[j] = cache.blocks[blockSetOffset].data[currentBlock++];
        }
        cache.blocks[blockSetOffset].isDirty = false;
        return 0;
    } else {
        printAction(memBlockHead, cache.blockSize, cacheToNowhere);
        return 0;
    }
}


/*
 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 !!!!!!!!!!!!        MAIN FUNCTION       !!!!!!!!!!!!
 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 */


int main(int argc, char *argv[]) {
    char line[MAXLINELENGTH];
    stateType state = {0};
    FILE *filePtr;
    
    if (argc != 5) {
        printf("error: usage: %s <machine-code file> blockSizeInWords numberOfSets blocksPerSet\n", argv[0]);
        exit(1);
    }
    
    filePtr = fopen(argv[1], "r");
    if (filePtr == NULL) {
        printf("error: can't open file %s", argv[1]);
        perror("fopen");
        exit(1);
    }
    
    /* read the entire machine-code file into memory */
    for (state.numMemory = 0; fgets(line, MAXLINELENGTH, filePtr) != NULL;
         state.numMemory++) {
        
        if (sscanf(line, "%d", state.mem+state.numMemory) != 1) {
            printf("error in reading address %d\n", state.numMemory);
            exit(1);
        }
        //printf("memory[%d]=%d\n", state.numMemory, state.mem[state.numMemory]);
    }
    
    initializeCache();
    cache.blockSize = atoi(argv[2]);
    cache.numSets = atoi(argv[3]);
    cache.blocksPerSet = atoi(argv[4]);
    
    for (int i = 0; i < NUMREGS; i++) {
        state.reg[i] = 0;
    }
    state.pc = 0;
    int done = 0;
    int totalInstructions = 0;
    int haltInstruction = 0;
    instructionInfo instructionDetails;
    
    //printState(&state);
    
    while (!done) {
        //int value = state.mem[state.pc];
        int value = load(state.pc, &state);
        instructionDetails.opcode = value >> 22 & BITMASK_FOR_PARSING_MACHINE_CODE;
        instructionDetails.arg0 = value >> 19 & BITMASK_FOR_PARSING_MACHINE_CODE;
        instructionDetails.arg1 = value >> 16 & BITMASK_FOR_PARSING_MACHINE_CODE;
        instructionDetails.arg2 = value & BITMASK_BITS_ZERO_TO_FIFTEEN;
        totalInstructions++;
        state.pc++;
        if (state.pc >= NUMMEMORY) {
            exitProgram("Program counter out of bounds");
        }
        
        
        switch (instructionDetails.opcode) {
            case 0:
            case 1: {
                if (!isValidRegister(instructionDetails.arg0) || !isValidRegister(instructionDetails.arg1) ||
                    !isValidRegister(instructionDetails.arg2)) {
                    exitProgram("Invalid register");
                }
                if (instructionDetails.opcode == 0) {
                    state.reg[instructionDetails.arg2] = state.reg[instructionDetails.arg0] +
                    state.reg[instructionDetails.arg1];
                } else if (instructionDetails.opcode == 1) {
                    state.reg[instructionDetails.arg2] = ~(state.reg[instructionDetails.arg0] |
                                                           state.reg[instructionDetails.arg1]);
                } else {
                    exitProgram("Invalid RType opcode");
                }
                break;
            }
            case 2:
            case 3:
            case 4: {
                int offset = convertNum(instructionDetails.arg2);
                
                if (offset < MIN_OFFSET || offset > MAX_OFFSET) {
                    exitProgram("Offset out of bounds");
                }
                if (!isValidRegister(instructionDetails.arg0) || !isValidRegister(instructionDetails.arg1)) {
                    exitProgram("Invalid register");
                }
                if (instructionDetails.opcode == 2) {
                    //state.reg[instructionDetails.arg1] = state.mem[state.reg[instructionDetails.arg0] + offset];
                    int data = load(state.reg[instructionDetails.arg0] + offset, &state);
                    state.reg[instructionDetails.arg1] = data;
                } else if (instructionDetails.opcode == 3) {
                    //state.mem[state.reg[instructionDetails.arg0] + offset] = state.reg[instructionDetails.arg1];
                    store(state.reg[instructionDetails.arg0] + offset, state.reg[instructionDetails.arg1], &state);
                } else if (instructionDetails.opcode == 4) {
                    if (state.reg[instructionDetails.arg0] == state.reg[instructionDetails.arg1]) {
                        state.pc += offset;
                    }
                } else {
                    exitProgram("Invalid IType opcode");
                }
                break;
            }
            case 5: {
                if (instructionDetails.opcode == 5) {
                    state.reg[instructionDetails.arg1] = state.pc;
                    state.pc = state.reg[instructionDetails.arg0];
                } else {
                    exitProgram("Invalid JType opcode");
                }
                break;
            }
            case 6: {
                haltInstruction = 1;
                done = 1;
                break;
            }
            case 7: {
                break;
            }
                
            default:
                exitProgram("Unsupported opcode");
        }
        
        
        if (!haltInstruction) {
            //printState(&state);
        }
    }
    
    /*printf("machine halted\n");
    printf("total of %d instructions executed\n", totalInstructions);
    printf("final state of machine:\n");
    printState(&state);*/
    
    return(0);
}

