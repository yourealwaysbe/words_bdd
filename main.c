

#include <stdio.h>

#include "util.h"
#include "cudd.h"
#include "dddmp.h"

#define MAX_WORD_SIZE 25

#define ANY_CHAR '*'

DdNode *addWord(DdManager *manager, DdNode *dict, char *word);
DdNode *getWord(DdManager *manager, char* word);
DdNode *getWordWildcards(DdManager *manager, char* word);
DdNode *addChar(DdManager *manager, DdNode *bddWord, char c, int i);
DdNode *addNonNull(DdManager *manager, DdNode *bddWord, int i);
void writeDotDict(DdManager *manager, DdNode *dict, char *outFile);
DdNode *loadWords(DdManager *manager, char *wordsFile);
void writeSummary(DdManager *manager, DdNode *dict);
DdNode *matchPattern(DdManager *manager, DdNode *dict, char *pattern);
DdNode *matchCrossPattern(DdManager *manager, DdNode *dict, char *pattern);
void printDictionary(DdManager *manager, DdNode *dict);
void instantiateAndPrintCube(int *cube, char *buf, int i, int size);
void printMatches(DdManager *manager, DdNode *dict, int *vars, int size);
void instantiateAndPrintWord(int *cube, char *buf, int i, int *vars, int size);
int setBit(int orig, int bit, int val);
int getBit(int i, int bit);
void processCommandLine(int argc, char **argv);
DdNode *loadBdd(DdManager *manager, char *bddInFile);
void writeBddDict(DdManager *manager, DdNode *dict, char *bddOutFile);
int bddIsEmpty(DdManager *manager, DdNode *bdd);

int totalChars = 0;
int totalWords = 0;


char *bddInFile = 0x00;
char *bddOutFile = 0x00;
char *wordFile = 0x00;
char *dotFile = 0x00;
char *pattern = 0x00;

int main(int argc, char **argv) {
    processCommandLine(argc, argv);

    DdManager *manager = Cudd_Init(0,0,CUDD_UNIQUE_SLOTS,CUDD_CACHE_SLOTS,0);

    DdNode *dict = 0x00;
    
    if (wordFile)
        dict = loadWords(manager, wordFile);
    else if (bddInFile)
        dict = loadBdd(manager, bddInFile);

    if (dict == 0x00) {
        printf("No dictionary, did you specify one on the command line?\n");
        exit(-1);
    }

    if (bddOutFile)
        writeBddDict(manager, dict, bddOutFile);

    if (dotFile)
        writeDotDict(manager, dict, dotFile);

    if (pattern) {
        DdNode *tmp = matchCrossPattern(manager, dict, pattern);
        Cudd_RecursiveDeref(manager, dict);
        dict = tmp;
        // printDictionary(manager, dict);
    }

    Cudd_Quit(manager);
}


void processCommandLine(int argc, char **argv) {
    int i = 1;
    int help = 0;
    int error = 0;

    while (i < argc && !error && !help) {
        if (strcmp(argv[i], "-h") == 0) {
            help = 1;
            ++i;
        } else if (i == argc - 1) {
            // then we have an option that can't have an argument
            // (and all remaining args need an argument)
            error = 1;
        } else if (strcmp(argv[i], "-ib") == 0) {
            bddInFile = argv[i+1];
            i += 2;
        } else if (strcmp(argv[i], "-ob") == 0) {
            bddOutFile = argv[i+1];
            i += 2;
        } else if (strcmp(argv[i], "-w") == 0) {
            wordFile = argv[i+1];
            i += 2;
        } else if (strcmp(argv[i], "-d") == 0) {
            dotFile = argv[i+1];
            i += 2;
        } else if (strcmp(argv[i], "-p") == 0) {
            pattern = argv[i+1];
            i += 2;
        } else {
            ++i;
        }
    }

    if (error || help || (bddInFile == 0x00 && wordFile == 0x00)) {
        printf("%d, %d, %d\n", error, help, (bddInFile == 0x00 && wordFile == 0x00));
        printf("Usage: ./words_bdd [options]\n");
        printf("\n");
        printf("Must specify -ib or -w.\n");
        printf("\n");
        printf("options:\n");
        printf("    -ib <file> : read language bdd from file\n");
        printf("    -ob <file> : write language bdd to file\n");
        printf("    -w <file>  : read language from word file\n");
        printf("    -d <file>  : write dot image to file\n");
        printf("    -p pattern : pattern to match (* is wildcard)\n");
        printf("    -h         : this help\n");
        exit(-1);
    }
}


DdNode *addWord(DdManager *manager, DdNode *dict, char *word) {
    DdNode *bddWord = getWord(manager, word);

    DdNode *newDict = Cudd_bddOr(manager, dict, bddWord);
    Cudd_Ref(newDict);
    Cudd_RecursiveDeref(manager, bddWord);

    return newDict;
}


DdNode *getWord(DdManager *manager, char* word) {
    DdNode *bddWord = Cudd_ReadOne(manager);
    Cudd_Ref(bddWord);

    DdNode *tmp;
    int i;

    for (i = 0; word[i] != 0x00; ++i) {
        tmp = addChar(manager, bddWord, word[i], i);
        Cudd_RecursiveDeref(manager, bddWord);
        bddWord = tmp;
    }

    tmp = addChar(manager, bddWord, 0x00, i);
    Cudd_RecursiveDeref(manager, bddWord);
    bddWord = tmp;

    return bddWord;
}

DdNode *getWordWildcards(DdManager *manager, char* word) {
    DdNode *bddWord = Cudd_ReadOne(manager);
    Cudd_Ref(bddWord);

    DdNode *tmp;
    int i;

    for (i = 0; word[i] != 0x00; ++i) {
        if (word[i] != ANY_CHAR) {
            tmp = addChar(manager, bddWord, word[i], i);
            Cudd_RecursiveDeref(manager, bddWord);
            bddWord = tmp;
        } else {
            tmp = addNonNull(manager, bddWord, i);
            Cudd_RecursiveDeref(manager, bddWord);
            bddWord = tmp;
        }
    }

    tmp = addChar(manager, bddWord, 0x00, i);
    Cudd_RecursiveDeref(manager, bddWord);
    bddWord = tmp;

    return bddWord;
}

DdNode *addChar(DdManager *manager, DdNode *bddWord, char c, int i) {
    DdNode *b, *newWord; 

    newWord = bddWord;
    Cudd_Ref(newWord);

    for (int bit = 0; bit < 8; ++bit) {
        b = Cudd_bddIthVar(manager, 8*i + bit);

        DdNode *tmp;
        if (c & (1<<bit))
            tmp = Cudd_bddAnd(manager, b, newWord);               
        else 
            tmp = Cudd_bddAnd(manager, Cudd_Not(b), newWord);
        Cudd_Ref(tmp);
        Cudd_RecursiveDeref(manager, newWord);
        newWord = tmp;
    }
    
    return newWord;
}

DdNode *addNonNull(DdManager *manager, DdNode *bddWord, int i) {
    DdNode *b, *anyChar, *newWord; 

    anyChar = Cudd_ReadLogicZero(manager);
    Cudd_Ref(anyChar);

    for (int bit = 0; bit < 8; ++bit) {
        b = Cudd_bddIthVar(manager, 8*i + bit);

        DdNode *tmp = Cudd_bddOr(manager, b, anyChar);
        Cudd_Ref(tmp);
        Cudd_RecursiveDeref(manager, anyChar);
        anyChar = tmp;
    }

    newWord = Cudd_bddAnd(manager, bddWord, anyChar);
    Cudd_Ref(newWord);
    
    return newWord;
}


void writeDotDict(DdManager *manager, DdNode *dict, char *outFile) {
    DdNode *outputs[] = { dict };
    FILE *f = fopen(outFile, "w");
    if (!f) {
        printf("Error opening %s for writing.\n", outFile);
        exit(-1);
    }
    Cudd_DumpDot(manager, 1, outputs, NULL, NULL, f);
    fclose(f);
}


DdNode *loadWords(DdManager *manager, char *wordsFile) {
    DdNode *dict = Cudd_ReadLogicZero(manager);
    Cudd_Ref(dict);

    FILE *f = fopen(wordsFile, "r");
    if (!f) {
        printf("Could not open %s for reading.\n", wordsFile);
        exit(-1);
    }

    char word[MAX_WORD_SIZE];

    while (fscanf(f, "%s", word) != EOF) {

        totalChars += strlen(word);
        totalWords++;

        DdNode *tmp = addWord(manager, dict, word);
        Cudd_RecursiveDeref(manager, dict);
        dict = tmp;
    }

    writeSummary(manager, dict);

    return dict;
}


void writeSummary(DdManager *manager, DdNode *dict) {
    long unsigned int numNodes = Cudd_ReadNodeCount(manager);
    int numVars = Cudd_ReadSize(manager);
    
    // includes null termination
    int totalBytes = totalChars + totalWords; 
    int totalBits = 8*totalBytes;
    double compression = (double)numNodes / (double)totalBits;

    printf("%d words read.\n", totalWords);
    printf("%d characters read.\n", totalChars);
    printf("%d total bits.\n", totalBits);
    printf("\n");
    printf("BDD has %lu nodes.\n", numNodes);
    printf("BDD has %d variables.\n", numVars);
    printf("\n");
    printf("total bits / num nodes =  %f.\n", compression);
}


DdNode *matchPattern(DdManager *manager, DdNode *dict, char *pattern) {
    DdNode *bddPat = getWordWildcards(manager, pattern);

    DdNode *newDict = Cudd_bddAnd(manager, dict, bddPat);
    Cudd_Ref(newDict);
    Cudd_RecursiveDeref(manager, bddPat);

    return newDict;
}

DdNode *matchCrossPattern(DdManager *manager, DdNode *dict, char *pattern) {
    DdNode *w1 = matchPattern(manager, dict, pattern);
    Cudd_Ref(w1);
    DdNode *w2 = matchPattern(manager, dict, pattern);
    Cudd_Ref(w2);

    int base = Cudd_ReadSize(manager) + 1;
    int size = 8 * (strlen(pattern) + 1);

    int *vars1 = (int*)malloc(size * sizeof(int));
    for (int i = 0; i < size; ++i) {
        int v = base + i;

        vars1[i] = v;

        DdNode *b = Cudd_bddIthVar(manager, v);
        DdNode *tmp = Cudd_bddCompose(manager, w1, b, i);
        Cudd_Ref(tmp);
        Cudd_RecursiveDeref(manager, w1);
        w1 = tmp;
    }

    int *vars2 = (int*)malloc(size * sizeof(int));
    for (int i = 0; i < size; ++i) {
        int v = (i <= 8 ? base + i : base + size + i);

        vars2[i] = v;

        DdNode *b = Cudd_bddIthVar(manager, v);
        DdNode *tmp = Cudd_bddCompose(manager, w2, b, i);
        Cudd_Ref(tmp);
        Cudd_RecursiveDeref(manager, w2);
        w2 = tmp;
    }

    printf("w1 : %d\n", bddIsEmpty(manager, w1));
    printf("w2 : %d\n", bddIsEmpty(manager, w2));

    DdNode *newDict = Cudd_bddAnd(manager, w1, w2);
    Cudd_Ref(newDict);
    Cudd_RecursiveDeref(manager, w1);
    Cudd_RecursiveDeref(manager, w2);

    printf("newDict : %d\n", bddIsEmpty(manager, newDict));


    printf("number 1\n");
    printMatches(manager, newDict, vars1, size);
    printf("number 2\n");
    printMatches(manager, newDict, vars2, size);

    free(vars1);
    free(vars2);

    return newDict;
}

void printDictionary(DdManager *manager, DdNode *dict) {
    DdGen *gen;
    int *cube;
    CUDD_VALUE_TYPE val;
    char buf[MAX_WORD_SIZE];
    
    int nvars = Cudd_ReadSize(manager);

    Cudd_ForeachCube(manager, dict, gen, cube, val) {
        instantiateAndPrintCube(cube, buf, 0, nvars);
    }
}

/* instantiateAndPrintCube(cube, buf, i , size)
 *
 * Given a cube of length given by size, instantiate all cube values of 2 to
 * first 0 and then 1, printing out the results.  i is the index we've recursed
 * down to, buf is the buffer to write the words to print.
 */
void instantiateAndPrintCube(int *cube, char *buf, int i, int size) {
    if (i == size) {
        printf("%s.\n", buf);
    } else {
        if (cube[i] != 2) {
            int bit = i%8;
            int cpos = i / 8;
            buf[cpos] = setBit(buf[cpos], bit, cube[i]);
            // if we just added a null byte, we're done
            if (bit == 7 && buf[cpos] == 0x00) 
                instantiateAndPrintCube(cube, buf, size, size);
            else 
                instantiateAndPrintCube(cube, buf, i+1, size);
        } else {
            cube[i] = 0;
            instantiateAndPrintCube(cube, buf, i, size);
            cube[i] = 1;
            instantiateAndPrintCube(cube, buf, i, size);
            cube[i] = 2;
        }
    }
}



/**
 * printMatches(manager, dict, vars, size)
 *
 * manager is the DdManager
 * dict is the bdd from which to read the word matches
 * vars is the list of variables that make up the word (including a set of
 * variables to hold the terminating null)
 * size is the size of vars
 *
 * prints all words (assignments) in the dict for the given variables
 */
void printMatches(DdManager *manager, DdNode *dict, int *vars, int size) {
    DdGen *gen;
    int *cube;
    CUDD_VALUE_TYPE val;
    char *buf = (char*)malloc(size * sizeof(char));
   
    // kind of assumes here that the bdd only has the vars in vars (otherwise we
    // may get repeated solutions)
    Cudd_ForeachCube(manager, dict, gen, cube, val) {
        instantiateAndPrintWord(cube, buf, 0, vars, size);
    }

    free(buf);
}

/* instantiateAndPrintWord(cube, buf, i, vars, size)
 *
 * Given a cube of length given by size, instantiate all cube values of 2 to
 * first 0 and then 1, printing out the results.  i is the index we've recursed
 * down to, buf is the buffer to write the words to print.
 */
void instantiateAndPrintWord(int *cube, char *buf, int i, int *vars, int size) {
    if (i == size) {
        printf("%s.\n", buf);
    } else {
        int cv = cube[vars[i]];
        if (cv != 2) {
            int bit = i%8;
            int cpos = i / 8;
            buf[cpos] = setBit(buf[cpos], bit, cv);
            // if we just added a null byte, we're done
            if (bit == 7 && buf[cpos] == 0x00) 
                instantiateAndPrintWord(cube, buf, size, vars, size);
            else 
                instantiateAndPrintWord(cube, buf, i+1, vars, size);
        } else {
            cube[vars[i]] = 0;
            instantiateAndPrintWord(cube, buf, i, vars, size);
            cube[vars[i]] = 1;
            instantiateAndPrintWord(cube, buf, i, vars, size);
            cube[vars[i]] = 2;
        }
    }
}




int setBit(int orig, int bit, int val) {
    return (orig & ~(1<<bit)) | val<<bit;
}

int getBit(int i, int bit) {
    return (i & (1<<bit)) ? 1 : 0;
}


DdNode *loadBdd(DdManager *manager, char *bddInFile) {
    FILE *f = fopen(bddInFile, "r");
    if (!f) {
        printf("Error opening %s for reading.\n", bddInFile);
        exit(-1);
    }

    DdNode *dict = Dddmp_cuddBddLoad(manager, 
                                     DDDMP_VAR_MATCHIDS, 
                                     0x00, 
                                     0x00, 
                                     0x00, 
                                     DDDMP_MODE_BINARY, 
                                     bddInFile, 
                                     f);

    fclose(f);

    return dict;
}

void writeBddDict(DdManager *manager, DdNode *dict, char *bddOutFile) {
    FILE *f = fopen(bddOutFile, "w");
    if (!f) {
        printf("Error opening %s for writing.\n", bddOutFile);
        exit(-1);
    }

    Dddmp_cuddBddStore(manager, 
                       bddOutFile, 
                       dict, 
                       0x00, 
                       0x00, 
                       DDDMP_MODE_BINARY, 
                       DDDMP_VARIDS, 
                       bddOutFile, 
                       f);

    fclose(f);
}


int bddIsEmpty(DdManager *manager, DdNode *bdd) {
    DdNode *zero = Cudd_ReadLogicZero(manager);
    return bdd == zero;
}

