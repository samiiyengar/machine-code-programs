/**
 * Project 2
 * LC-2K Linker
 */

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAXSIZE 300
#define MAXLINELENGTH 1000
#define MAXFILES 6

#define BITMASK_FOR_PARSING_MACHINE_CODE 0x00000007
#define BITMASK_BITS_ZERO_TO_FIFTEEN 0xFFFF

typedef struct FileData FileData;
typedef struct SymbolTableEntry SymbolTableEntry;
typedef struct RelocationTableEntry RelocationTableEntry;
typedef struct CombinedFiles CombinedFiles;

struct SymbolTableEntry {
    char label[7];
    char location;
    int offset;
};

struct RelocationTableEntry {
    int offset;
    char inst[7];
    char label[7];
    int file;
};

struct FileData {
    int textSize;
    int dataSize;
    int symbolTableSize;
    int relocationTableSize;
    int textStartingLine; // in final executible
    int dataStartingLine; // in final executible
    int text[MAXSIZE];
    int data[MAXSIZE];
    SymbolTableEntry symbolTable[MAXSIZE];
    RelocationTableEntry relocTable[MAXSIZE];
};

struct CombinedFiles {
    int text[MAXSIZE];
    int data[MAXSIZE];
    SymbolTableEntry     symTable[MAXSIZE];
    RelocationTableEntry relocTable[MAXSIZE];
    int textSize;
    int dataSize;
    int symTableSize;
    int relocTableSize;
};

int validateDuplicates(struct FileData* fileData, int totalFiles);
int checkDuplicateGlobalLabels(struct FileData* fileData1, struct FileData* fileData2);
int validateReserved(struct FileData* fileData, int totalFiles, const char* reservedLabel);
int isGlobalLabel(char* label);
int needsResolution(const char* label, struct FileData* fileData, int objectFileIndex);
int resolveGlobalLabel(const char* label, struct FileData* files, int totalFiles,
    int objectFileIndex, struct CombinedFiles* combined);
void printOutput(struct CombinedFiles* combined, FILE *outputFile);

int main(int argc, char *argv[])
{
    char *inFileString, *outFileString;
    FILE *inFilePtr, *outFilePtr;
    int i, j;

    if (argc <= 2) {
        printf("error: usage: %s <obj file> ... <output-exe-file>\n",
            argv[0]);
        exit(1);
    }

    outFileString = argv[argc - 1];

    outFilePtr = fopen(outFileString, "w");
    if (outFilePtr == NULL) {
        printf("error in opening %s\n", outFileString);
        exit(1);
    }

    FileData files[MAXFILES];

    //Reads in all files and combines into master
    for (i = 0; i < argc - 2; i++) {
        inFileString = argv[i + 1];

        inFilePtr = fopen(inFileString, "r");

        if (inFilePtr == NULL) {
            printf("error in opening %s\n", inFileString);
            exit(1);
        }

        char line[MAXLINELENGTH];
        int sizeText, sizeData, sizeSymbol, sizeReloc;

        // parse first line
        fgets(line, MAXSIZE, inFilePtr);
        sscanf(line, "%d %d %d %d",
            &sizeText, &sizeData, &sizeSymbol, &sizeReloc);

        files[i].textSize = sizeText;
        files[i].dataSize = sizeData;
        files[i].symbolTableSize = sizeSymbol;
        files[i].relocationTableSize = sizeReloc;

        // read in text
        int instr;
        for (j = 0; j < sizeText; j++) {
            fgets(line, MAXLINELENGTH, inFilePtr);
            instr = atoi(line);
            files[i].text[j] = instr;
        }

        // read in data
        int data;
        for (j = 0; j < sizeData; j++) {
            fgets(line, MAXLINELENGTH, inFilePtr);
            data = atoi(line);
            files[i].data[j] = data;
        }

        // read in the symbol table
        char label[7];
        char type;
        int addr;
        for (j = 0; j < sizeSymbol; j++) {
            fgets(line, MAXLINELENGTH, inFilePtr);
            sscanf(line, "%s %c %d",
                label, &type, &addr);
            files[i].symbolTable[j].offset = addr;
            strcpy(files[i].symbolTable[j].label, label);
            files[i].symbolTable[j].location = type;
        }

        // read in relocation table
        char opcode[7];
        for (j = 0; j < sizeReloc; j++) {
            fgets(line, MAXLINELENGTH, inFilePtr);
            sscanf(line, "%d %s %s",
                &addr, opcode, label);
            files[i].relocTable[j].offset = addr;
            strcpy(files[i].relocTable[j].inst, opcode);
            strcpy(files[i].relocTable[j].label, label);
            files[i].relocTable[j].file = i;
        }
        fclose(inFilePtr);
    } // end reading files

    // *** INSERT YOUR CODE BELOW ***
    //    Begin the linking process
    //    Happy coding!!!

    struct CombinedFiles combined = { 0 };
    int totalFiles = argc - 2;

    if (!validateDuplicates(files, totalFiles)) {
        printf("error: duplicate global labels found \n");
        exit(1);
    }

    if (!validateReserved(files, totalFiles, "Stack")) {
        printf("error: reserved label Stack used \n");
        exit(1);
    }

    combined.textSize = 0;
    combined.dataSize = 0;

    for (int i = 0; i < totalFiles; ++i) {
        combined.textSize += files[i].textSize;
        combined.dataSize += files[i].dataSize;
    }

    int textOffset = 0;
    int dataOffset = 0;
    for (int i = 0; i < totalFiles; ++i) {
        memcpy(combined.text + textOffset, files[i].text, files[i].textSize * sizeof(int));
        textOffset += files[i].textSize;

        memcpy(combined.data + dataOffset, files[i].data, files[i].dataSize * sizeof(int));
        dataOffset += files[i].dataSize;
    }

    textOffset = 0;
    dataOffset = 0;

    int startDataOffsetObj = combined.textSize;

    for (int i = 0; i < totalFiles; ++i) {
        for (int j = 0; j < files[i].relocationTableSize; j++) {
            if (strcmp(files[i].relocTable[j].inst, ".fill")) {
                int instruction = combined.text[textOffset + files[i].relocTable[j].offset];
                int opcode = instruction >> 22 & BITMASK_FOR_PARSING_MACHINE_CODE;
                int arg0 = instruction >> 19 & BITMASK_FOR_PARSING_MACHINE_CODE;
                int arg1 = instruction >> 16 & BITMASK_FOR_PARSING_MACHINE_CODE;
                int arg2 = instruction & BITMASK_BITS_ZERO_TO_FIFTEEN;
                //printf("instruction 0 %d\n", instruction);
                if (!isGlobalLabel(files[i].relocTable[j].label) || (!needsResolution(files[i].relocTable[j].label, files, i))) {
                    if (arg2 >= files[i].textSize) {
                        int offsetObjectFile = arg2 - files[i].textSize;
                        int labelOffset = startDataOffsetObj + offsetObjectFile;
                        labelOffset &= 0xFFFF;
                        instruction = opcode << 22 | arg0 << 19 | arg1 << 16 | labelOffset;
                        combined.text[textOffset + files[i].relocTable[j].offset] = instruction;
                    } else {
                        int offsetObjectFile = arg2 + textOffset;
                        offsetObjectFile &= 0xFFFF;
                        instruction = opcode << 22 | arg0 << 19 | arg1 << 16 | offsetObjectFile;
                        combined.text[textOffset + files[i].relocTable[j].offset] = instruction;
                    }
                } else {
                    //printf("resolving %s\n", files[i].relocTable[j].label);
                    int globalLabelOffset = resolveGlobalLabel(files[i].relocTable[j].label, files, totalFiles, i, &combined);
                    //printf("offset %d\n", globalLabelOffset);
                    if (globalLabelOffset == -1) {
                        printf("error resolving global label %s\n", files[i].relocTable[j].label);
                        exit(1);
                    }
                    globalLabelOffset &= 0xFFFF;
                    //printf("instruction %d\n", instruction);
                    instruction = opcode << 22 | arg0 << 19 | arg1 << 16 | globalLabelOffset;
                    //printf("instruction %d\n", instruction);
                    combined.text[textOffset + files[i].relocTable[j].offset] = instruction;
                }
            } else {
                int offset = files[i].relocTable[j].offset;
                if (!isGlobalLabel(files[i].relocTable[j].label) || !needsResolution(files[i].relocTable[j].label, files, i)) {
                    int fillValue = combined.data[dataOffset + offset];
                    if (fillValue >= files[i].textSize) {
                        int offsetObjectFile = fillValue - files[i].textSize + startDataOffsetObj;
                        combined.data[dataOffset + offset] = offsetObjectFile;
                    } else {
                        combined.data[dataOffset + offset] += textOffset;
                    }
                } else {
                    int globalLabelOffset = resolveGlobalLabel(files[i].relocTable[j].label, files, totalFiles, i, &combined);
                    if (globalLabelOffset == -1) {
                        printf("error resolving global label %s\n", files[i].relocTable[j].label);
                        exit(1);
                    }
                    globalLabelOffset &= 0xFFFF;
                    combined.data[dataOffset + offset] = globalLabelOffset;
                }
            }
        }
        textOffset += files[i].textSize;
        dataOffset += files[i].dataSize;
        startDataOffsetObj += files[i].dataSize;
    }
    printOutput(&combined, outFilePtr);
} // end main

int validateDuplicates(struct FileData* fileData, int totalFiles) {
    for (int i = 0; i < totalFiles; ++i) {
        for (int j = i + 1; j < totalFiles; ++j) {
            //dupe found
            if (!checkDuplicateGlobalLabels(&fileData[i], &fileData[j])) {
                return 0;
            }
        }
    }
    return 1;
}

int checkDuplicateGlobalLabels(struct FileData* fileData1, struct FileData* fileData2) {
    for (int i = 0; i < fileData1->symbolTableSize; ++i) {
        if (fileData1->symbolTable[i].location == 'U') {
            continue;
        }
        for (int j = 0; j < fileData2->symbolTableSize; ++j) {
            if (fileData2->symbolTable[j].location == 'U') {
                continue;
            }

            // duplicate found
            if (!strcmp(fileData1->symbolTable[i].label, fileData2->symbolTable[j].label)) {
                return 0;
            }
        }
    }
    //none
    return 1;
}

int validateReserved(struct FileData* fileData, int totalFiles, const char* reservedLabel) {
    for (int i = 0; i < totalFiles; ++i) {
        for (int j = 0; j < fileData[i].symbolTableSize; ++j) {
            if (fileData[i].symbolTable[j].location == 'U') {
                continue;
            }
            //reserved found
            if (!strcmp(fileData[i].symbolTable[j].label, reservedLabel)) {
                return 0;
            }
        }
    }
    //none found
    return 1;
}

int isGlobalLabel(char* label) {
    return isupper(label[0]);
}

int needsResolution(const char* label, struct FileData* fileData, int objectFileIndex) {
    for (int i = 0; i < fileData[objectFileIndex].symbolTableSize; ++i) {
        if (!strcmp(fileData[objectFileIndex].symbolTable[i].label, label)) {
            if (fileData[objectFileIndex].symbolTable[i].location == 'U') {
                return 1;
            }
            else {
                return 0;
            }
        }
    }
    return -1;
}

int resolveGlobalLabel(const char* label, struct FileData* files, int totalFiles,
    int objectFileIndex, struct CombinedFiles* combined) {
    int foundLabelObjIndex = -1;
    int foundSymbolTableIndex = -1;
    for (int i = 0; i < totalFiles; ++i) {
        if (i == objectFileIndex) {
            continue;
        }
        for (int j = 0; j < files[i].symbolTableSize; ++j) {
            if (files[i].symbolTable[j].location != 'U' && !strcmp(files[i].symbolTable[j].label, label)) {
                foundLabelObjIndex = i;
                foundSymbolTableIndex = j;
                break;
            }
        }
        if (foundLabelObjIndex != -1) {
            break;
        }
    }
    if (foundLabelObjIndex == -1) {
        if (!strcmp(label, "Stack")) {
            return combined->textSize + combined->dataSize;
        }
        return -1;
    }
    if (files[foundLabelObjIndex].symbolTable[foundSymbolTableIndex].location == 'D') {
        int startDataOffsetObj = combined->textSize;
        for (int i = 0; i < totalFiles; ++i) {
            if (i == foundLabelObjIndex) {
                return startDataOffsetObj + files[foundLabelObjIndex].symbolTable[foundSymbolTableIndex].offset;
            }
            startDataOffsetObj += files[i].dataSize;
        }
    } else if (files[foundLabelObjIndex].symbolTable[foundSymbolTableIndex].location == 'T') {
        int codeOffset = 0;
        for (int i = 0; i < totalFiles; ++i) {
            if (i == foundLabelObjIndex) {
                return codeOffset + files[foundLabelObjIndex].symbolTable[foundSymbolTableIndex].offset;
            } else {
                codeOffset += files[i].textSize;
            }
        }
    }
    return -1;
}

void printOutput(struct CombinedFiles* combined, FILE *outputFile) {
    for (int i = 0; i < combined->textSize; ++i) {
        fprintf(outputFile, "%d\n", combined->text[i]);
    }

    for (int i = 0; i < combined->dataSize; ++i) {
        fprintf(outputFile, "%d\n", combined->data[i]);
    }
    fclose(outputFile);
}
