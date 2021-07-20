/*=========================================================================================================
 * Assignment 6 - CS2610 - Designing a cache simulator
 * Authors: Ravisri Valluri, Vineeth Kada, Sriram Goutam
 * Roll No: CS19B081, CS19B080, CS19B073
 * =======================================================================================================*/

/* == IMPORTANT: PLEASE USE C++11 OR HIGHER == */

#include<bits/stdc++.h>
using namespace std;

/*=========================================================================================================
 *  cache class:
 *      Implements cache based on random, LRU, or pseudo LRU replacement policy
 *      Allows user to customize cache size, block size, and associativity
 *      Given memory trace, finds counts on number of accesses, misses and evictions
 * =======================================================================================================*/
class cache {
    /* Description of a cache block */
    struct cacheBlock {
        int tag;                /* a kind of identifier */
        bool valid;             /* whether block is empty or not */
        bool dirty;             /* whether a update needs to happen upon eviction (write has happened) */
        cacheBlock(int t=0, bool d=false, bool v=false): tag(t), dirty(d), valid(v) {}
    };

    /* Fundamental features of cache */
    int cacheSize, blockSize, associativity, replacementPolicy;
    int blocks, ways, sets;

    int addressSize = 32;
    /* Description of fields of token from memory trace */
    int blockOffsetSize, setIndexSize, tagSize;

    /* Summary of cache events */
    int cacheAccesses=0, readAccesses=0, writeAccesses=0;
    int cacheMisses=0, compulsoryMisses=0, capacityMisses=0, conflictMisses=0, readMisses=0, writeMisses=0;
    int evictedDirtyBlocks=0;

    /* Status of a particular token */
    bool read;
    long long setIndex, blockOffset, tag;

    /* Organisation of cache data - note that linked list is used for sets */
  	list<cacheBlock>* data;
    bool ** tree;
    cacheBlock ** treeData;
    set<string> accessed;

    /* Extract k bits starting from the position p, p is 1 for LSB */
    long long extractBits(long long number, int k, int p) {
        return (((1 << k) - 1) & (number >> (p - 1)));
    }

    public:
        /* Obtains information about cache type and fields of a token in the memory trace  */
        void scanDetailsAndPreprocess() {
            bool validInput = false;
            cout << "Enter Cache Size(power of 2): ";
            while(! validInput){
                cin  >>  cacheSize;
                if(__builtin_popcount(cacheSize) == 1) validInput = true;
                else cout << "Please enter a power of 2: ";
            }
            
            validInput = false;
            cout << "Enter Block Size(power of 2): ";
            while(! validInput){
                cin  >>  blockSize;
                if(__builtin_popcount(blockSize) == 1) validInput = true;
                else cout << "Please enter a power of 2: ";
            }

            /* associativity = 0 for fully associative, 1 for direct-mapped, 2/4/8/16/32 for set-associative */
            /* replacementPolicy = 0 for random, 1 for LRU, 2 for Pseudo-LRU */
            validInput = false;
            cout << "Enter associativity (0 for fully associative, 1 for direct-mapped, 2/4/8/16/32 for set-associative): ";
            while(! validInput){
                cin  >>  associativity;
                if(__builtin_popcount(associativity) == 1 && associativity <= 32) validInput = true;
                else cout << "Please enter a valid input: ";
            }
            validInput = false;
            cout << "Enter a type of replacement policy (0 for random, 1 for LRU, 2 for Pseudo-LRU): ";
            while(! validInput) {
                cin >> replacementPolicy;
                if(replacementPolicy == 0 || replacementPolicy == 1 || replacementPolicy == 2) validInput = true;
                else cout << "Please enter a value among 0, 1 or 2: ";
            }

            blocks = cacheSize/blockSize;
            ways = (associativity==0 ? blocks : associativity);
            sets = blocks/ways;

            /* Obtain sizes by finding number of trailing zeros */
            blockOffsetSize=__builtin_ctz(blockSize);
            setIndexSize=__builtin_ctz(sets);
            tagSize=addressSize-blockOffsetSize-setIndexSize;

            /* Decide how to organize cache data based on replacement policy */
            if(replacementPolicy == 2) {
                tree = new bool  * [sets];
                for(int i = 0; i < sets; i++) tree[i] = new bool [ways];
                treeData = new cacheBlock * [sets];
                for(int i = 0; i < sets; i++) treeData[i]=new cacheBlock[ways];
            } else {
                data = new list<cacheBlock>[sets];
                for(int i = 0; i < sets; i++) data[i].resize(ways);
            }
        }

        /* Provides tokens from memory trace to appropriate function based on replacement policy */
        void processTokens() {
            bool validInput = false;
            string fileName; 
            cout << "Enter the name of the input file: ";
            ifstream memoryTrace;
            while(! validInput) {
                cin >> fileName;
                memoryTrace.open(fileName);
                if(memoryTrace) {
                    validInput = true;
                }
                else {
                    cout << "The file doesn't exist. Kindly enter the name of a file that exists in the present directory: ";
                }
            }
            //ifstream memoryTrace(fileName);
            string token;
            while(memoryTrace >> token) {
                cacheAccesses++;

                /* Deleting the 0x part from the memory address */
                token.erase(token.begin(), token.begin() + 2);

                /* If the block is accessed for the first time */
                if(accessed.count(token) == 0){
                    compulsoryMisses++;
                    accessed.insert(token);
                }

                /* Converting the hexadecimal address to decimal */
                long long result = 0;
                for (int i=0; i<token.length(); i++) {
                    if (token[i]>='0'&& token[i]<='9') {
                        result += (token[i]-'0')*pow(16,token.length()-i-1);
                    } else if (token[i]>='A' && token[i]<='F') {
                        result += (token[i]-'A'+10)*pow(16,token.length( )-i-1);
                    } else if (token[i]>='a' && token[i]<='f') {
                        result += (token[i]-'a'+10)*pow(16,token.length()-i-1);
                    }
                }

                /* Read or write? */
                char ch; memoryTrace >> ch;
                read = (ch == 'r');
              	if(read) readAccesses++; else writeAccesses++;

                /* Extracting blockOffset, setIndex, and tag. Memory address is [32 31 .... 1], LSB is denoted with index 1 */
                /* extractBits(string, k, p) will extract the bits [p + k - 1, p] and return it as a decimal number */
              	blockOffset = extractBits(result, blockOffsetSize, 1);
                setIndex = extractBits(result, setIndexSize, 1+blockOffsetSize);
                tag = extractBits(result, tagSize, addressSize - tagSize + 1);

                /* Call appropriate function for the token */
                switch(replacementPolicy) {
                    case 0: random(); break;
                    case 1: LRU(); break;
                    case 2: psuedoLRU(); break;
                }
            }
            /* In a fully associative cache conflictMisses = 0 */
            if(associativity == 0) capacityMisses = cacheMisses - compulsoryMisses;
            /* Upon assuming that capacityMisses = 0 for direct mapped and set associative caches we find conflict misses */
            else conflictMisses = cacheMisses - compulsoryMisses;
        }

        /* Treats the input based on random replacement policy */
        void random(){
            list<cacheBlock>::iterator it;
            for (auto &x: data[setIndex]) {
                /*If the block is present, then the dirty bit is modified if write operation*/
                if(x.valid && x.tag == tag) {
                    if(!read) x.dirty = true;
                    return;
                }
            }
            /* If the requested block is not present then we bring it into the cache from the main memory and perform the
              requested operation and set the dirty bit accordingly */
            cacheMisses++;
            if(read) readMisses++; else writeMisses++;

            /* check if the set is full or not? */
            for(auto &x: data[setIndex]){
                if(! x.valid){
                    x = cacheBlock(tag, !read, true);
                    return;
                }
            }
            /* Decide which block to evict usind random number generator*/
            int randomBlock = rand()%ways + 1;

            /* Obtain iterator to that block */
            auto iter = data[setIndex].begin();
            for(int c = 1; c < randomBlock; c++, iter++);

            /* If it is a dirty block, update 'evictedDirtyBlocks' */
            if(iter->dirty == 1) evictedDirtyBlocks++;

            /* Obtain new block and put in position */
            *iter =  cacheBlock(tag,!read,true);
        }

        /* Treats the input based on 'least recently used' replacement policy */
        void LRU() {
            /*The for loop checks whether the requested block is present or not. If yes then the block is brought to
              the front*/
            for (auto iter = data[setIndex].begin(); iter != data[setIndex].end(); ++iter) {
                if(iter->valid && iter->tag == tag) {
                    if(!read) iter->dirty = true;
                    data[setIndex].push_front(*iter);
                    data[setIndex].erase(iter);
                    return;
                }
            }
            cacheMisses++;
            if(read) readMisses++; else writeMisses++;

            /*If the requested block is not present then we bring it into the cache from the main memory and perform the
              requested operation and set the dirty bit accordingly*/
            data[setIndex].push_front(cacheBlock(tag,!read,true));
            if(data[setIndex].back().dirty && data[setIndex].back().valid) evictedDirtyBlocks++;
            data[setIndex].pop_back();
        }

        /* Treats the input based on a tree based pseudo-'least recently used' replacement policy */
	    void psuedoLRU() {
            /* Check if block is already in cache- if so update the tree bits accordingly */
            for(int i = 0; i < ways; i++){
                /* If we find the block, we make sure that the tree bits point away from path from block to root */
                if(treeData[setIndex][i].valid && tag == treeData[setIndex][i].tag) {
                    int pos = (ways+i)/2, prevSide = (ways+i)%2;
                    while(pos >= 1){
                        tree[setIndex][pos] = ! prevSide;
                        prevSide = pos % 2;
                        pos /= 2;
                    }
                    /* If this is a write operation, set dirty bit to true */
                    if(!read) treeData[setIndex][i].dirty=true;
                    return;
                }
            }
            /* If not found increment miss count */
            cacheMisses++;
            if(read) readMisses++; else writeMisses++;

            /* Follow tree bits to find victim, and update bits along the way */
            int pos = 1;
            while(pos < ways){
                bool side = tree[setIndex][pos];
                tree[setIndex][pos] = ! tree[setIndex][pos];
                pos = 2*pos + side;
            }
            /* If victim is a valid dirty block, note it */
            if(treeData[setIndex][pos-ways].dirty && treeData[setIndex][pos-ways].valid) evictedDirtyBlocks++;

            /* Obtain new block and put in place */
            treeData[setIndex][pos-ways] = cacheBlock(tag, !read, true);
        }

        /* Prints all data obtained to standard output */
        void printData(){
            cout  <<  "Cache Size: "  <<  cacheSize  <<  endl;
            cout  <<  "Block Size: "  <<  blockSize  <<  endl;
            cout  <<  "Type of Cache: ";
            switch(associativity) {
                case 0: cout  <<  "Fully Associative"  <<  endl; break;
                case 1: cout  <<  "Direct Mapped"  <<  endl; break;
                default: cout  <<  "Set-Associative"  <<  endl; break;
            }
            cout  <<  "Replacement Policy: ";
            switch(replacementPolicy) {
                case 0: cout  <<  "Random"  <<  endl; break;
                case 1: cout  <<  "LRU"  <<  endl; break;
                case 2: cout  <<  "Pseudo-LRU"  <<  endl; break;
            }
            cout  <<  "Number of Cache Accesses: "  <<  cacheAccesses  <<  endl;
            cout  <<  "Number of Read Accesses: "  <<  readAccesses  <<  endl;
            cout  <<  "Number of Write Accesses: "  <<  writeAccesses  <<  endl;
            cout  <<  "Number of Cache Misses: "  <<  cacheMisses  <<  endl;
            cout  <<  "Number of Compulsory Misses: "  <<  compulsoryMisses  <<  endl;
            cout  <<  "Number of Capacity Misses: "  <<  capacityMisses  <<  endl;
            cout  <<  "Number of Conflict Misses: "  <<  conflictMisses  <<  endl;
            cout  <<  "Number of Read Misses: "  <<  readMisses  <<  endl;
            cout  <<  "Number of Write Misses: "  <<  writeMisses  <<  endl;
            cout  <<  "Number of Dirty Blocks Evicted: "  <<  evictedDirtyBlocks  <<  endl;
        }
};

int main() {
    cache L1;
    L1.scanDetailsAndPreprocess();
    L1.processTokens();
    L1.printData();
}
