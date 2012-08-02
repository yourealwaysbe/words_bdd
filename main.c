

#include <stdio.h>

#include "util.h"
#include "cudd.h"

#define MAX_WORD_SIZE 30

DdNode *addWord(char *word, DdManager *manager, DdNode *dict);
DdNode *getWord(char* word, DdManager *manager);
DdNode *addChar(char c, int i, DdManager *manager, DdNode *bddWord);
void writeDict(DdManager *manager, DdNode *dict, char *outFile);
DdNode *loadWords(DdManager *manager, char *wordsFile);
void writeSummary(DdManager *manager, DdNode *dict);


int totalChars = 0;
int totalWords = 0;

int main(int argc, char** argv) {

    if (argc < 3) {
        printf("Usage: ./words_bdd words_file output_dot_file\n");
        exit(-1);
    }

    DdManager *manager = Cudd_Init(0,0,CUDD_UNIQUE_SLOTS,CUDD_CACHE_SLOTS,0);

    DdNode *dict = loadWords(manager, argv[1]);
    writeDict(manager, dict, argv[2]);
    writeSummary(manager, dict);

    Cudd_Quit(manager);
}


DdNode *addWord(char *word, DdManager *manager, DdNode *dict) {
    DdNode *bddWord = getWord(word, manager);

    DdNode *newDict = Cudd_bddOr(manager, dict, bddWord);
    Cudd_Ref(newDict);
    Cudd_RecursiveDeref(manager, bddWord);

    return newDict;
}


DdNode *getWord(char* word, DdManager *manager) {
    DdNode *bddWord = Cudd_ReadOne(manager);
    Cudd_Ref(bddWord);

    DdNode *tmp;
    int i;

    for (i = 0; word[i] != 0x00; ++i) {
        tmp = addChar(word[i], i, manager, bddWord);
        Cudd_RecursiveDeref(manager, bddWord);
        bddWord = tmp;
    }

    tmp = addChar(0x00, i, manager, bddWord);
    Cudd_RecursiveDeref(manager, bddWord);
    bddWord = tmp;

    return bddWord;
}

DdNode *addChar(char c, int i, DdManager *manager, DdNode *bddWord) {
    char mask = 1;
    DdNode *b, *newWord; 

    newWord = bddWord;
    Cudd_Ref(newWord);

    for (int bit = 0; bit < 8; ++bit) {
        b = Cudd_bddIthVar(manager, 8*i + bit);

        DdNode *tmp;
        if (c & mask)
            tmp = Cudd_bddAnd(manager, b, newWord);               
        else 
            tmp = Cudd_bddAnd(manager, Cudd_Not(b), newWord);
        Cudd_Ref(tmp);
        Cudd_RecursiveDeref(manager, newWord);
        newWord = tmp;

        mask <<= 1;
    }
    
    return newWord;
}

void writeDict(DdManager *manager, DdNode *dict, char *outFile) {
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

        DdNode *tmp = addWord(word, manager, dict);
        Cudd_RecursiveDeref(manager, dict);
        dict = tmp;
    }

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
