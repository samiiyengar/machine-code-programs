/**
 * Project 2
 * Assembler code fragment for LC-2K
 */

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAXLINELENGTH 1000
#define MAX_LABEL_LENGTH 7
#define MAX_LABEL_VALUE_LENGTH 100
#define MIN_FILL_BOUNDS (-2147483647 - 1)
#define MAX_FILL_BOUNDS 2147483647
#define MAX_OPCODE_LENGTH 20

// identifies the type of the opcode
enum OPCODE_TYPE {
    RTYPE,
    ITYPE,
    JTYPE,
    OTYPE,
    FILL,
    UNDEFINED,
};

// struct that contains all the characteristics needed when keeping track
// of the labels, including a pointer to the next label
struct LabelInformation {
    char labelName[MAX_LABEL_LENGTH];
    char labelValue[MAX_LABEL_VALUE_LENGTH];
    int lineNumber;
    struct LabelInformation* next;
};

// head in label list is null
struct LabelInformation* labels = NULL;

// details about supported opcode
struct OpcodeInfo {
    char name[MAX_OPCODE_LENGTH];
    int opcode;
    enum OPCODE_TYPE opcodeType;
};

// all different opcodes
struct OpcodeInfo opcodeList[] = {
    {"add", 0, RTYPE},
    {"nor", (1 << 22), RTYPE},
    {"lw", (2 << 22), ITYPE},
    {"sw", (3 << 22), ITYPE},
    {"beq", (4 << 22), ITYPE},
    {"jalr", (5 << 22), JTYPE},
    {"halt", (6 << 22), OTYPE},
    {"noop", (7 << 22), OTYPE},
    {".fill", -1, FILL}
};

// int all opcodes
size_t totalOpcodes = sizeof(opcodeList) / sizeof(opcodeList[0]);

struct SymbolTableEntry {
    char symbolName[MAX_LABEL_LENGTH];
    char entryType;
    int lineOffset;
    struct SymbolTableEntry *next;
};

int symbolLength = 0;

struct SymbolTableEntry* symbolTableEntry = NULL;

struct TextEntry {
    int machineCode;
    struct TextEntry* next;
};

int textLength = 0;

struct TextEntry* textEntry = NULL;

struct DataEntry {
    int value;
    struct DataEntry* next;
};

int dataLength = 0;

struct DataEntry* dataEntry = NULL;

struct RelocationEntry {
    int lineOffset;
    char opcodeName[MAX_OPCODE_LENGTH];
    char label[MAX_LABEL_LENGTH];
    struct RelocationEntry* next;
};

int relocationLength = 0;

struct RelocationEntry* relocationEntry = NULL;

// functions we were given
int readAndParse(FILE *, char *, char *, char *, char *, char *);
int isNumber(char *);

// first pass over input file
void assemblerPass1(FILE *inputFile);

// second pass over input file
void assemblerPass2(FILE *inputFile, FILE *outputFile);

// helper functions
int isDuplicate(char *label);
int isValid(char *label);
int isGlobalLabel(char* label);
//void addLabelToList(char *label, char *opcode, char *arg0, int lineNumber);
int getOpcodeDetails(const char* opcode, struct OpcodeInfo* opcode_info);
int formatOpcodeBasedOnType(struct OpcodeInfo* opcodeInfo, char *label, char *opcode, char *arg0, char *arg1, char *arg2, int lineNumber);
int isValidRegister(char* reg);
int lookupLabelAddress(char* labelName);
void addOpcodeToTextSection(int opcode);
void addEntryToDataSection(int entry);
void addEntryToRelocationSection(int lineOffset, char* opcodeName, char* labelName);
void addEntryToSymbolTableSection(int lineOffset, char* symbolName, char symbolType);
int isAlreadyAdded(char* symbolName);
void exitProgram(char* message);

/*
 * Read and parse a line of the assembly-language file.  Fields are returned
 * in label, opcode, arg0, arg1, arg2 (these strings must have memory already
 * allocated to them).
 *
 * Return values:
 *     0 if reached end of file
 *     1 if successfully read
 *
 * exit(1) if line is too long.
 */
int readAndParse(FILE *inFilePtr, char *label, char *opcode, char *arg0,
        char *arg1, char *arg2) {
    char line[MAXLINELENGTH];

    /* delete prior values */
    label[0] = opcode[0] = arg0[0] = arg1[0] = arg2[0] = '\0';

    /* read the line from the assembly-language file */
    if (fgets(line, MAXLINELENGTH, inFilePtr) == NULL) {
        /* reached end of file */
        return(0);
    }

    /* check for line too long (by looking for a \n) */
    if (strchr(line, '\n') == NULL) {
        /* line too long */
        printf("error: line too long\n");
        exit(1);
    }

    /* is there a label? */
    char *ptr = line;
    if (sscanf(ptr, "%[^\t\n\r ]", label)) {
        /* successfully read label; advance pointer over the label */
        ptr += strlen(label);
    }

    /*
     * Parse the rest of the line.  Would be nice to have real regular
     * expressions, but scanf will suffice.
     */
    sscanf(ptr, "%*[\t\n\r ]%[^\t\n\r ]%*[\t\n\r ]%[^\t\n\r ]%*[\t\n\r ]%[^\t\n\r ]%*[\t\n\r ]%[^\t\n\r ]",
        opcode, arg0, arg1, arg2);
    return(1);
}

int isNumber(char *string) {
    /* return 1 if string is a number */
    int i;
    return( (sscanf(string, "%d", &i)) == 1);
}

// first pass
// iterate through entire input file
// if you encounter a label, make note of it and store it
void assemblerPass1(FILE* inputFile) {
    char label[MAXLINELENGTH], opcode[MAXLINELENGTH], arg0[MAXLINELENGTH],
    arg1[MAXLINELENGTH], arg2[MAXLINELENGTH];
    int lineNumber = 0;
    while (readAndParse(inputFile, label, opcode, arg0, arg1, arg2)) {
        // there's no label so we just increment line number and continue
        if (strlen(label) == 0) {
            lineNumber++;
            continue;
        }
        // check if label is duplicate and valid
        // throw an error here
        if (!isValid(label)) {
            printf("%s\n", label);
            exitProgram("Invalid label");
        }
        if (isDuplicate(label)) {
            exitProgram("Duplicate label");
        }
        struct LabelInformation* newLabel = (struct LabelInformation*) malloc(sizeof(struct LabelInformation));
        strcpy(newLabel->labelName, label);
        newLabel->lineNumber = lineNumber;
        newLabel->next = NULL;
        if (!strcmp(opcode, ".fill")) {
            if (isNumber(arg0)) {
                char *end = NULL;
                if (strtoll(arg0, &end, 10) <= MIN_FILL_BOUNDS || strtoll(arg0, &end, 10) >= MAX_FILL_BOUNDS) {
                    exitProgram(".fill overflow");
                }
            }
            strcpy(newLabel->labelValue, arg0);
        }
        if (labels == NULL) {
            labels = newLabel;
        } else {
            struct LabelInformation* current = labels;
            while (current->next) {
                current = current->next;
            }
            current->next = newLabel;
        }
        // increment line number so we can do this again for each line
        lineNumber++;
    }
}

void assemblerPass2(FILE *inputFile, FILE *outputFile) {
    char label[MAXLINELENGTH], opcode[MAXLINELENGTH], arg0[MAXLINELENGTH],
    arg1[MAXLINELENGTH], arg2[MAXLINELENGTH];
    int lineNumber = 0;
    while (readAndParse(inputFile, label, opcode, arg0, arg1, arg2)) {
        int machineInstruction = 0;
        struct OpcodeInfo opcodeInfo;
        if (!getOpcodeDetails(opcode, &opcodeInfo)) {
            //printf("%s\n", opcode);
            exitProgram("Unsupported opcode");
        }
        if (opcodeInfo.opcodeType == UNDEFINED) {
            //printf("%s\n", opcode);
            exitProgram("Unsupported opcode");
        }
        if (opcodeInfo.opcodeType != FILL) {
            machineInstruction = formatOpcodeBasedOnType(&opcodeInfo, label, opcode, arg0, arg1, arg2, lineNumber);
            addOpcodeToTextSection(machineInstruction);
        } else {
            if (isNumber(arg0)) {
                int value = atoi(arg0);
                if (isGlobalLabel(label)) {
                    addEntryToSymbolTableSection(dataLength, label, 'D');
                }
                addEntryToDataSection(value);
            } else {
                int address = lookupLabelAddress(arg0);
                if (address == -1) {
                    if (isGlobalLabel(arg0)) {
                        if (!isAlreadyAdded(arg0)) {
                            addEntryToSymbolTableSection(0, arg0, 'U');
                        }
                        if (isGlobalLabel(label)) {
                            addEntryToSymbolTableSection(dataLength, label, 'D');
                        }
                        addEntryToRelocationSection(dataLength, ".fill", arg0);
                        addEntryToDataSection(0); // don't have value so we use dummy val of 0, resolve during linking
                    } else {
                        printf("%s\n", arg0);
                        exitProgram("Invalid label");
                    }
                } else {
                    addEntryToRelocationSection(dataLength, ".fill", arg0);
                    if (isGlobalLabel(label)) {
                        addEntryToSymbolTableSection(dataLength, label, 'D');
                    }
                    addEntryToDataSection(address);
                }
            }
        }
        ++lineNumber;
    }
    fprintf(outputFile, "%d %d %d %d\n", textLength, dataLength, symbolLength, relocationLength);
    struct TextEntry* current = textEntry;
    while (current) {
        fprintf(outputFile, "%d\n", current->machineCode);
        current = current->next;
    }

    struct DataEntry* current1 = dataEntry;
    while (current1) {
        fprintf(outputFile, "%d\n", current1->value);
        current1 = current1->next;
    }

    struct SymbolTableEntry* current2 = symbolTableEntry;
    while (current2) {
        fprintf(outputFile, "%s %c %d\n", current2->symbolName, current2->entryType, current2->lineOffset);
        current2 = current2->next;
    }

    struct RelocationEntry* current3 = relocationEntry;
    while (current3) {
        fprintf(outputFile, "%d %s %s\n", current3->lineOffset, current3->opcodeName, current3->label);
        current3 = current3->next;
    }
}

int isDuplicate(char *labelNameIn) {
    // labels list is empty
    if (!labels) {
        return 0;
    }
    
    struct LabelInformation* currentLabel = labels;
    while (currentLabel != NULL) {
        if (!strcmp(currentLabel->labelName, labelNameIn)) {
            return 1;
        }
        currentLabel = currentLabel->next;
    }
    return 0;
}

int isValid(char *labelNameIn) {
    // label can contain at most 6 characters
    if (labelNameIn == NULL || strlen(labelNameIn) > MAX_LABEL_LENGTH - 1) {
        return 0;
    }
    // has to start with a character
    if (!isalpha(labelNameIn[0])) {
        return 0;
    }
    // rest of the label can contain a mixture of digits/alphabets
    for (size_t i = 1; i < strlen(labelNameIn); ++i) {
        if (!isalnum(labelNameIn[i])) {
            return 0;
        }
    }
    /*if (!strcmp(labelNameIn, "Stack")) {
        return 0;
    }*/
    return 1;
}

int isGlobalLabel(char* label) {
    return isupper(label[0]);
}

/*
void addLabelToList(char *label, char *opcode, char *arg0, int lineNumber) {
    struct LabelInformation *info = (struct LabelInformation*) malloc(sizeof(struct LabelInformation));
    strcpy(info->labelName, label);
    info->lineNumber = lineNumber;
    info->next = NULL;
    if (!strcmp(opcode, ".fill")) {
        if (isNumber(arg0)) {
            char *end = NULL;
            long long val = strtoll(arg0, &end, 10);
            // The bounds of the numeric value for .fill instructions are -2^31 to +2^31-1
            // (-2147483648 to 2147483647).
            if (val <= MIN_FILL_BOUNDS || val >= MAX_FILL_BOUNDS) {
                exitProgram("Fill overflow");
            }
        }
        strcpy(info->labelValue, arg0);
    }
    if (labels == NULL) {
        labels = info;
    } else {
        struct LabelInformation* current = labels;
        while (current->next) {
            current = current->next;
        }
        current->next = info;
    }
}
*/
// returns 1 if the opcode is found in the list of supported opcodes
int getOpcodeDetails(const char* opcode, struct OpcodeInfo* opcode_info) {
    for (size_t i = 0; i < totalOpcodes; ++i) {
        if (!strcmp(opcode, opcodeList[i].name)) {
            *opcode_info = opcodeList[i];
            return 1;
        }
    }
    return 0;
}

int formatOpcodeBasedOnType(struct OpcodeInfo* opcodeInfo, char *label, char *opcode, char *arg0, char *arg1, char *arg2, int lineNumber) {
    int machineInstruction = opcodeInfo->opcode;
    switch (opcodeInfo->opcodeType) {
        case RTYPE: {
            if (!isValidRegister(arg0) ||
                !isValidRegister(arg1) ||
                !isValidRegister(arg2)) {
                exitProgram("Invalid registers");
            }
            if (isGlobalLabel(label)) {
                addEntryToSymbolTableSection(textLength, label, 'T');
            }
            machineInstruction |= atoi(arg0) << 19;
            machineInstruction |= atoi(arg1) << 16;
            machineInstruction |= atoi(arg2) << 0;
            break;
        }
        case ITYPE: {
            if (!isValidRegister(arg0) ||
                !isValidRegister(arg1)) {
                exitProgram("Invalid registers");
            }
            int offset = 0;
            machineInstruction |= atoi(arg0) << 19;
            machineInstruction |= atoi(arg1) << 16;
            if (isGlobalLabel(label)) {
                addEntryToSymbolTableSection(textLength, label, 'T');
            }
            if (isNumber(arg2)) {
                offset = atoi(arg2);
                if (offset < -32768 || offset > 32767) {
                    exitProgram("Offset out of range");
                }
            } else {
                offset = lookupLabelAddress(arg2);
                if (offset == -1) {
                    if (isGlobalLabel(arg2)) {
                        if (!strcmp(opcode, "beq")) {
                            exitProgram("Undefined label");
                        }
                        offset = 0;
                        if (!isAlreadyAdded(arg2)) {
                            addEntryToSymbolTableSection(0, arg2, 'U');
                        }
                    } else {
                        printf("%s\n", arg2);
                        exitProgram("Invalid label");
                    }
                }
            }
            if (strcmp(opcode, "beq") && !isNumber(arg2)) {
                addEntryToRelocationSection(textLength, opcodeInfo->name, arg2);
            }
            if (!strcmp(opcode, "beq") && !isNumber(arg2)) {
                offset = offset - lineNumber - 1;
                if (offset < -32768 || offset > 32767) {
                    exitProgram("Offset out of range");
                }
            }
            offset &= 0xFFFF;
            machineInstruction |= offset;
            break;
        }
        case JTYPE: {
            if (!isValidRegister(arg0) ||
                !isValidRegister(arg1)) {
                exitProgram("Invalid registers");
            }
            if (isGlobalLabel(label)) {
                addEntryToSymbolTableSection(textLength, label, 'T');
            }
            machineInstruction |= atoi(arg0) << 19;
            machineInstruction |= atoi(arg1) << 16;
            break;
        }
        case OTYPE: {
            if (isGlobalLabel(label)) {
                addEntryToSymbolTableSection(textLength, label, 'T');
            }
            break;
        }
        default: {
            printf("%s\n", opcode);
            exitProgram("Unsupported opcode");
        }
    }
    return machineInstruction;
}



int isValidRegister(char* reg) {
    if (!isNumber(reg)) {
        return 0;
    }
    int num = atoi(reg);
    if (num < 0 || num >= 8) {
        return 0;
    }
    return 1;
}

int lookupLabelAddress(char* labelName) {
    struct LabelInformation* current = labels;
    while (current != NULL) {
        if (!strcmp(current->labelName, labelName)) {
            return current->lineNumber;
        }
        current = current->next;
    }
    return -1;
}

void addOpcodeToTextSection(int opcode) {
    struct TextEntry* entry = malloc(sizeof(struct TextEntry));
    entry->machineCode = opcode;
    entry->next = NULL;

    textLength++;

    if (textEntry == NULL) {
        textEntry = entry;
        return;
    }

    struct TextEntry* current = textEntry;
    while (current->next) {
        current = current->next;
    }
    current->next = entry;
}

void addEntryToDataSection(int value) {
    struct DataEntry* entry = malloc(sizeof(struct DataEntry));
    entry->value = value;
    entry->next = NULL;

    dataLength++;

    if (dataEntry == NULL) {
        dataEntry = entry;
        return;
    }

    struct DataEntry* current = dataEntry;
    while (current->next) {
        current = current->next;
    }
    current->next = entry;
}

void addEntryToRelocationSection(int lineOffset, char* opcodeName, char* labelName) {
    struct RelocationEntry* entry = malloc(sizeof(struct RelocationEntry));
    entry->lineOffset = lineOffset;
    strcpy(entry->opcodeName, opcodeName);
    strcpy(entry->label, labelName);
    entry->next = NULL;

    relocationLength++;

    if (relocationEntry == NULL) {
        relocationEntry = entry;
        return;
    }

    struct RelocationEntry* current = relocationEntry;
    while (current->next) {
        current = current->next;
    }
    current->next = entry;
}

void addEntryToSymbolTableSection(int lineOffset, char* symbolName, char entryType) {
    struct SymbolTableEntry* entry = malloc(sizeof(struct SymbolTableEntry));
    entry->lineOffset = lineOffset;
    strcpy(entry->symbolName, symbolName);
    entry->entryType = entryType;
    entry->next = NULL;

    symbolLength++;

    if (symbolTableEntry == NULL) {
        symbolTableEntry = entry;
        return;
    }
    struct SymbolTableEntry* current = symbolTableEntry;
    while (current->next) {
        current = current->next;
    }
    current->next = entry;
}

int isAlreadyAdded(char* symbolName) {
    struct SymbolTableEntry* current = symbolTableEntry;
    if (symbolTableEntry == NULL) {
        return 0;
    }
    while (current) {
        if (current->entryType == 'U' && !strcmp(current->symbolName, symbolName)) {
            // don't add duplicates
            return 1;
        }
        current = current->next;
    }
    return 0;
}

void exitProgram(char* message) {
    printf("\n%s\n", message);
    exit(1);
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
    char *inFileString, *outFileString;
    FILE *inFilePtr, *outFilePtr;
    /*char label[MAXLINELENGTH], opcode[MAXLINELENGTH], arg0[MAXLINELENGTH],
            arg1[MAXLINELENGTH], arg2[MAXLINELENGTH]; */

    if (argc != 3) {
        printf("error: usage: %s <assembly-code-file> <machine-code-file>\n",
            argv[0]);
        exit(1);
    }

    inFileString = argv[1];
    outFileString = argv[2];

    inFilePtr = fopen(inFileString, "r");
    if (inFilePtr == NULL) {
        printf("error in opening %s\n", inFileString);
        exit(1);
    }
    outFilePtr = fopen(outFileString, "w");
    if (outFilePtr == NULL) {
        printf("error in opening %s\n", outFileString);
        exit(1);
    }
    
    assemblerPass1(inFilePtr);
    rewind(inFilePtr);
    assemblerPass2(inFilePtr, outFilePtr);

    return(0);
}



