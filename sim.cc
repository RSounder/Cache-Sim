#include <bits/stdc++.h>
#include <bitset>
#include <inttypes.h>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>

using namespace std;
int addressLen = 32;
int ct = 0;

#include "sim.h"

/*  "argc" holds the number of command-line arguments.
    "argv[]" holds the arguments themselves.

    Example:
    ./sim 32 8192 4 262144 8 3 10 gcc_trace.txt
    argc = 9
    argv[0] = "./sim"
    argv[1] = "32"
    argv[2] = "8192"
    ... and so on

    ./sim 16 512 2 4096 1 0 0 sample_ex1.txt
*/
int main(int argc, char *argv[]) {

  FILE *fp;              // File pointer.
  char *trace_file;      // This variable holds the trace file name.
  cache_params_t params; // Look at the sim.h header file for the definition of
                         // struct cache_params_t.
  char rw; // This variable holds the request's type (read or write) obtained
           // from the trace.
  uint32_t addr; // This variable holds the request's address obtained from the
                 // trace. The header file <inttypes.h> above defines signed and
                 // unsigned integers of various sizes in a machine-agnostic
                 // way.  "uint32_t" is an unsigned integer of 32 bits.

  // Exit with an error if the number of command-line arguments is incorrect.
  if (argc != 9) {
    printf("Error: Expected 8 command-line arguments but was provided %d.\n",
           (argc - 1));
    exit(EXIT_FAILURE);
  }

  // "atoi()" (included by <stdlib.h>) converts a string (char *) to an integer
  // (int).
  params.BLOCKSIZE = (uint32_t)atoi(argv[1]);
  params.L1_SIZE = (uint32_t)atoi(argv[2]);
  params.L1_ASSOC = (uint32_t)atoi(argv[3]);
  params.L2_SIZE = (uint32_t)atoi(argv[4]);
  params.L2_ASSOC = (uint32_t)atoi(argv[5]);
  params.PREF_N = (uint32_t)atoi(argv[6]);
  params.PREF_M = (uint32_t)atoi(argv[7]);
  trace_file = argv[8];

  // Open the trace file for reading.
  fp = fopen(trace_file, "r");
  if (fp == (FILE *)NULL) {
    // Exit with an error if file open failed.
    printf("Error: Unable to open file %s\n", trace_file);
    exit(EXIT_FAILURE);
  }

  // Print simulator configuration.
  printf("===== Simulator configuration =====\n");
  printf("BLOCKSIZE:  %u\n", params.BLOCKSIZE);
  printf("L1_SIZE:    %u\n", params.L1_SIZE);
  printf("L1_ASSOC:   %u\n", params.L1_ASSOC);
  printf("L2_SIZE:    %u\n", params.L2_SIZE);
  printf("L2_ASSOC:   %u\n", params.L2_ASSOC);
  printf("PREF_N:     %u\n", params.PREF_N);
  printf("PREF_M:     %u\n", params.PREF_M);
  printf("trace_file: %s\n", trace_file);

  // create empty caches
  genericCache L1;
  genericCache L2;
  prefetcher preFet;
  int isL2presentFlag = 0;
  int isPrefetcherPresent = 0;

  // initialise valid caches
  if (params.L1_SIZE > 0) {

    L1 = genericCache(params.L1_SIZE, params.L1_ASSOC, params.BLOCKSIZE);

    // initializing lru count values
    for (int i = 0; i < L1.sets; i++) {
      for (int j = 0; j < params.L1_ASSOC; j++) {
        L1.setArr[i].blockArr[j].lruCount = ct++ % params.L1_ASSOC;
      }
    }
  }
  if (params.L2_SIZE > 0) {
    isL2presentFlag = 1;
    L2 = genericCache(params.L2_SIZE, params.L2_ASSOC, params.BLOCKSIZE);

    // initializing lru count values
    for (int i = 0; i < L2.sets; i++) {
      for (int j = 0; j < params.L2_ASSOC; j++) {
        L2.setArr[i].blockArr[j].lruCount = ct++ % params.L2_ASSOC;
      }
    }
  }

  if (params.PREF_N > 0) {
    isPrefetcherPresent = 1;
    if (isL2presentFlag) {
      // cout << "params bs" << params.BLOCKSIZE << endl;
      preFet = prefetcher(params.PREF_N, params.PREF_M, params.BLOCKSIZE,
                          (L2.tagLen + L2.indexLen));
    } else {
      // cout << "params bs" << params.BLOCKSIZE << endl;
      preFet = prefetcher(params.PREF_N, params.PREF_M, params.BLOCKSIZE,
                          (L1.tagLen + L1.indexLen));
    }
  }
  long int whileItr = 0;
  // // Read requests from the trace file and echo them back.
  while (fscanf(fp, "%c %x\n", &rw, &addr) ==
         2) { // Stay in the loop if fscanf() successfully parsed two tokens as
              // specified.
    whileItr ++;
    int *L1HitorMissArray = L1.hitOrMiss(addr);
    
    if (rw == 'r') {
      L1.reads++;
      if (L1HitorMissArray[0]) {
        // read hit in L1
        // cout<<"L1 read hit"<<endl;
        L1.updateLRUcountWithinSet(L1HitorMissArray[1], L1HitorMissArray[2]);
        if (isPrefetcherPresent && !isL2presentFlag) {
          bool result = preFet.hitOrMissPf(addr, false);
          if (result) {
            //cout<< "L1 read hit + PF hit"<<endl;
            preFet.hitOrMissPf(addr, true);
            L1.prefetches += preFet.hitRet;
            L1.memoryTraffic += preFet.hitRet; // block brought from memory
          }
        }
      } else {
        // read miss in L1
        // if read miss in L1, search L2 (if present, else place L1)
        // cout<<"L1 read miss"<<endl;
        L1.readMisses++;
        if (isL2presentFlag) {
          // read miss. so one block must come into L1. search if there is space
          // for it. else evict. first evict, then search next hierarchy
          string *evictOrPlaceL1 = L1.evictOrPlace(addr);
          uint32_t L1addrToEvict =
              static_cast<std::uint32_t>(std::stoul(evictOrPlaceL1[2]));
          if (evictOrPlaceL1[0] == "evict" && evictOrPlaceL1[1] == "1") {
            // place in L2 and set dirty
            int *L2HitorMissArray = L2.hitOrMiss(L1addrToEvict);
            L2.writes++;
            L1.writeBacks++;
            if (L2HitorMissArray[0]) {
              // L2 write hit
              // cout<<"L2 write hit with dirty bit "<< evictOrPlaceL1[1]<<endl;
              // update lru count in L2 set
              if (evictOrPlaceL1[1] == "1") {
                L2.setDirtyBit(1, L2HitorMissArray[1], L2HitorMissArray[2]);
              }
              L2.updateLRUcountWithinSet(L2HitorMissArray[1],
                                         L2HitorMissArray[2]);
              
              if(isPrefetcherPresent && preFet.hitOrMissPf(L1addrToEvict, false)) {
                  //hit hit
                  preFet.hitOrMissPf(L1addrToEvict); //TODO: L2 Pf                  
                        L2.memoryTraffic += preFet.hitRet; // block brought from memory
                        L2.prefetches += preFet.hitRet; ////
              }

            } else {
                    //dummy
                    L2.writeMisses++;
                  if(isPrefetcherPresent) {
                      //miss
                      if(preFet.hitOrMissPf(L1addrToEvict, false)) { //TODO: L2 Pf                  
                        //L2 miss prefetcher hit
                        L2.writeMisses--;
                        
                      } else {
                          //L2 miss prefetcher miss
                          L2.memoryTraffic++; //for cache
                      }
                      preFet.hitOrMissPf(L1addrToEvict);
                        L2.prefetches += preFet.hitRet;
                        L2.memoryTraffic += preFet.hitRet; // block brought from memory
                  }
            
              // L2 write miss
              
              // cout<<"L2 write miss"<<endl;
              // is L2 set full or empty. evict to mem if full (and dirty) else
              // place
              string *evictOrPlaceL2 = L2.evictOrPlace(addr);
              if ((evictOrPlaceL2[0] == "evict") &&
                  (evictOrPlaceL2[1] == "1")) {
                // dirty block eviction from L2; update memory write count
                L2.memoryTraffic++;
                L2.writeBacks++;
              }
              // placing in L2 after eviction
              (evictOrPlaceL1[1] == "1")
                  ? L2.placeMissedBlockInCacheSet(addr, "dirty")
                  : L2.placeMissedBlockInCacheSet(addr);
            }
          }
          // now space will be free in L1 for placing read block. search L2 for
          // it's existance
          int *L2HitorMissArray = L2.hitOrMiss(addr);
          L2.reads++;
          if (L2HitorMissArray[0]) {
            // L2 read hit
            // set lruCount, copy L2 block to L1
            L2.updateLRUcountWithinSet(L2HitorMissArray[1],
                                       L2HitorMissArray[2]);
            // check for dirty bit and place in L1 accordingly
            L1.placeMissedBlockInCacheSet(addr);
                  if(isPrefetcherPresent && preFet.hitOrMissPf(addr,false)) {
                      //hit hit
                      preFet.hitOrMissPf(addr); //TODO: L2 Pf                  
                      L2.memoryTraffic += preFet.hitRet; // block brought from memory
                      L2.prefetches += preFet.hitRet; ////
                  }
          } else {
            // L2 read miss, i.e, no addr in L2
                  L2.readMisses++;
                  if(isPrefetcherPresent) {
                  if(!preFet.hitOrMissPf(addr,false)) {
                      L2.memoryTraffic++;
                  } else {
                      // L2 miss Pf hit
                      L2.readMisses--;
                  }
                      preFet.hitOrMissPf(addr); //TODO: L2 Pf                  
                        L2.prefetches += preFet.hitRet;
                        L2.memoryTraffic += preFet.hitRet; // block brought from memory
                  }


            // cout<<"new block miss in L2"<<endl;
            // evict L2 lru block if full and dirty, then place block into L2
            // from memory
            string *evictOrPlaceL2 = L2.evictOrPlace(addr);
            if ((evictOrPlaceL2[0] == "evict") && (evictOrPlaceL2[1] == "1")) {
              // dirty block eviction from L2; update memory write count
              L2.memoryTraffic++;
              L2.writeBacks++;
            }
            L2.memoryTraffic++;
            L2.placeMissedBlockInCacheSet(addr);
            // cout<<"L2 write clean"<<endl;
            // after placing block in L2, place block in L1
            L1.placeMissedBlockInCacheSet(
                addr); // since coming from memory, it should be clean
          }
        } else {

          if (isPrefetcherPresent) {
            bool result = preFet.hitOrMissPf(addr, false);
            if (result) {
              // cout<< "L1 read miss + PF hit"<<endl;
              // hit in stream buffer; L1 prefetches
              preFet.hitOrMissPf(addr, true);
              L1.prefetches += preFet.hitRet;
              L1.memoryTraffic += preFet.hitRet; // block brought from memory
              L1.readMisses--; // hit in SB. thus not considered as L1 readMiss.
            } else {
              //cout<< "L1 read miss + PF miss"<<endl;
              // missed in SB, bring block from memory and update SB
              preFet.hitOrMissPf(addr, true);
              L1.prefetches += preFet.hitRet;
              L1.memoryTraffic += preFet.hitRet;  // block brought from memory for SB
              L1.memoryTraffic++; // for cache
            }
          } else {
            // if prefetcher is not present, L1 missed block to be brought from
            // memory
            L1.memoryTraffic++; // block brought from memory
          }

          // if L2 isnt present, place block in L1 directly
          string *evictOrPlaceL1 = L1.evictOrPlace(addr);
          if (evictOrPlaceL1[0] == "evict" && evictOrPlaceL1[1] == "1") {
            // evict L1 block to memory if dirty, else replace
            L1.memoryTraffic++;
            L1.writeBacks++;
          }
          L1.placeMissedBlockInCacheSet(
              addr); // since coming from memory, it should be clean
        }
      }
    } else if (rw == 'w') {
      L1.writes++;
      if (L1HitorMissArray[0]) {
        // write hit in L1
        // cout<<"L1 write hit"<<endl;
        L1.updateLRUcountWithinSet(L1HitorMissArray[1], L1HitorMissArray[2]);
        L1.setDirtyBit(1, L1HitorMissArray[1], L1HitorMissArray[2]);
        if (!isL2presentFlag && isPrefetcherPresent) {
          if (isPrefetcherPresent && !isL2presentFlag) {
            bool result = preFet.hitOrMissPf(addr, false);
            if (result) {
              // cout<< "L1 read hit + PF hit"<<endl;
              preFet.hitOrMissPf(addr, true);
              L1.prefetches += preFet.hitRet;
              L1.memoryTraffic += preFet.hitRet; // block brought from memory
            }
            // L1.prefetches++;
          }
        }
      } else {
        L1.writeMisses++;
        // write miss in L1. so a block must be brought into L1 and written
        // upon. so free a space for it.
        if (isL2presentFlag) {
          // first evict, then search next hierarchy
          string *evictOrPlaceL1 = L1.evictOrPlace(addr);
          uint32_t L1addrToEvict =
              static_cast<std::uint32_t>(std::stoul(evictOrPlaceL1[2]));
          if (evictOrPlaceL1[0] == "evict" && evictOrPlaceL1[1] == "1") {

            // search if block exits in L2, if so set dirty, else free block in
            // L2, then place in L2
            int *L2HitorMissArray = L2.hitOrMiss(L1addrToEvict);
            L2.writes++;
            L1.writeBacks++;
            if (L2HitorMissArray[0]) {
            
                if(isPrefetcherPresent && preFet.hitOrMissPf(L1addrToEvict,false)) {
                      //hit hit
                      preFet.hitOrMissPf(L1addrToEvict); //TODO: L2 Pf                  
                        L2.memoryTraffic += preFet.hitRet; // block brought from memory
                        L2.prefetches += preFet.hitRet;
                  }

              // L2 write hit
              // cout<<"L2 write hit"<<endl;
              // update lru count in L2 set
              if (evictOrPlaceL1[1] == "1") {
                L2.setDirtyBit(1, L2HitorMissArray[1], L2HitorMissArray[2]);
              }
              L2.updateLRUcountWithinSet(L2HitorMissArray[1],
                                         L2HitorMissArray[2]);
            } else {
              // L2 write miss
              L2.writeMisses++;            
                if(isPrefetcherPresent) {
                        if(!preFet.hitOrMissPf(L1addrToEvict,false)) { L2.memoryTraffic++; }
                        else {L2.writeMisses--;}
                        preFet.hitOrMissPf(L1addrToEvict); //TODO: L2 Pf                  
                        L2.prefetches += preFet.hitRet;
                        L2.memoryTraffic += preFet.hitRet; // block brought from memory
                  }

              // cout<<"L2 write miss"<<endl;
              // is L2 set full or empty. evict to mem if full (and dirty) else
              // place
              string *evictOrPlaceL2 = L2.evictOrPlace(addr);
              if ((evictOrPlaceL2[0] == "evict") &&
                  (evictOrPlaceL2[1] == "1")) {
                // dirty block eviction from L2; update memory write count
                L2.memoryTraffic++;
                L2.writeBacks++;
              }
              // placing in L2 after eviction
              (evictOrPlaceL1[1] == "1")
                  ? L2.placeMissedBlockInCacheSet(addr, "dirty")
                  : L2.placeMissedBlockInCacheSet(addr);
            }
          } else {
            // dont have to evict.
          }
          // now there is free space in L1
          // search for req. block in L2, if hit, set lruCount and copy block to
          // L1 as dirty if not hit, evict L2, place in L2, then place in L1 as
          // dirty
          int *L2HitorMissArray = L2.hitOrMiss(addr);
          L2.reads++;
          if (L2HitorMissArray[0]) {
                  if(isPrefetcherPresent && preFet.hitOrMissPf(addr,false)) {
                      preFet.hitOrMissPf(addr); //TODO: L2 Pf                  
                        L2.memoryTraffic += preFet.hitRet; // block brought from memory
                        L2.prefetches += preFet.hitRet; ////
                  }
            // read hit in L2 -> update lruCount in L2 and move block to L1
            L2.updateLRUcountWithinSet(L2HitorMissArray[1],
                                       L2HitorMissArray[2]);
            L1.placeMissedBlockInCacheSet(
                addr, "dirty"); // block is to be written in L1 so set as dirty
          } else {              // read miss in L2 -> evict L2 block if
                                // L2 set is full & dirty -> place in L2, then
                                // in L1 then set L1 as dirty
            L2.readMisses++;
                  if(isPrefetcherPresent) {
                  if(!preFet.hitOrMissPf(addr,false)) {L2.memoryTraffic++;}
                  else {L2.readMisses--;}
                        preFet.hitOrMissPf(addr);
                        L2.prefetches += preFet.hitRet;
                        L2.memoryTraffic += preFet.hitRet; // block brought from memory
                       //TODO: L2 Pf                  
                  }
            string *evictOrPlaceL2 = L2.evictOrPlace(addr);
            if ((evictOrPlaceL2[0] == "evict") && (evictOrPlaceL2[1] == "1")) {
              // dirty block eviction from L2; update memory write count
              L2.memoryTraffic++;
              L2.writeBacks++;
            }
            L2.memoryTraffic++;
            L2.placeMissedBlockInCacheSet(addr);
            // cout<<"L2 write clean"<<endl;
            // after placing block in L2, place block in L1
            L1.placeMissedBlockInCacheSet(addr,
                                          "dirty"); // write request. so dirty.
          }
        } else {
          // L2 isnt there
          // L1 write miss. so bring block into L1 and set dirty bit
          string *evictOrPlaceL1 = L1.evictOrPlace(addr);
          if (evictOrPlaceL1[0] == "evict" && evictOrPlaceL1[1] == "1") {
            // evict L1 block to memory if dirty, else replace
            L1.memoryTraffic++;
            L1.writeBacks++;
          }

          if (isPrefetcherPresent) {
            bool result = preFet.hitOrMissPf(addr, false);
            if (result) {
              // cout<< "L1 read miss + PF hit"<<endl;
              // L1 prefetches
              preFet.hitOrMissPf(addr, true);
              L1.prefetches += preFet.hitRet;
              L1.memoryTraffic += preFet.hitRet; // block brought from memory
              L1.writeMisses--; // hit in SB. thus not considered as L1
                                // writeMiss.
            } else {
              // cout<< "L1 read miss + PF miss"<<endl;
              preFet.hitOrMissPf(addr, true);
              L1.prefetches += preFet.hitRet;
              L1.memoryTraffic +=
                  preFet.hitRet;  // block brought from memory for SB
              L1.memoryTraffic++; // for cache
            }
          } else {
            // prefetcher not present. so L1 missed block must be brought from
            // memory
            L1.memoryTraffic++;
          }

          L1.placeMissedBlockInCacheSet(addr, "dirty");
        }
      }
    }

    else {
      cout << "Error: Unknown request type %c.\n" << rw;
      exit(EXIT_FAILURE);
    }
  }

  cout << "\n===== L1 contents =====" << endl;
  L1.printSimpleContentsMru2Lru();
  if (isL2presentFlag) {
    cout << "\n===== L2 contents =====" << endl;
    L2.printSimpleContentsMru2Lru();
  }
  if (isPrefetcherPresent) {
    cout << "\n===== Stream Buffer(s) contents =====" << endl;
    preFet.printAll();
  }

  // L1.print
  cout << "\n===== Measurements =====" << endl;
  cout << "a. L1 reads:                   " << L1.reads << endl;
  cout << "b. L1 read misses:             " << L1.readMisses << endl;
  cout << "c. L1 writes:                  " << L1.writes << endl;
  cout << "d. L1 write misses:            " << L1.writeMisses << endl;
  double temp =
      (double(L1.readMisses + L1.writeMisses) / double(L1.reads + L1.writes));
  cout << "e. L1 miss rate:               " << fixed << setprecision(4) << temp
       << endl;
  cout << "f. L1 writebacks:              " << L1.writeBacks << endl;
  cout << "g. L1 prefetches:              " << L1.prefetches << endl;
  cout << "h. L2 reads (demand):          " << L2.reads << endl;
  cout << "i. L2 read misses (demand):    " << L2.readMisses << endl;
  cout << "j. L2 reads (prefetch):        " << L2.readsPrefetches << endl;
  cout << "k. L2 read misses (prefetch):  " << L2.readMissesPrefetches << endl;
  cout << "l. L2 writes:                  " << L2.writes << endl;
  cout << "m. L2 write misses:            " << L2.writeMisses << endl;
  temp =
      (isL2presentFlag) ? (double(L2.readMisses) / double(L2.reads)) : 0.0000;
  cout << "n. L2 miss rate:               " << fixed << setprecision(4) << temp
       << endl;
  cout << "o. L2 writebacks:              " << L2.writeBacks << endl;
  cout << "p. L2 prefetches:              " << L2.prefetches << endl;
  // cout << "q. memory traffic:             "
  //      << L1.memoryTraffic + L2.memoryTraffic << endl;
  if(isL2presentFlag) {
    cout << "q. memory traffic:             "
           << L2.readMisses + L2.readMissesPrefetches + L2.writeMisses + L2.writeBacks + L2.prefetches << endl;
  } else {
    cout << "q. memory traffic:             "
           << L1.readMisses + L1.writeMisses + L1.writeBacks + L1.prefetches << endl;
  }
  
  return (0);
}
