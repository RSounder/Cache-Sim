#ifndef SIM_CACHE_H
#define SIM_CACHE_H

typedef struct
{
  uint32_t BLOCKSIZE;
  uint32_t L1_SIZE;
  uint32_t L1_ASSOC;
  uint32_t L2_SIZE;
  uint32_t L2_ASSOC;
  uint32_t PREF_N;
  uint32_t PREF_M;
} cache_params_t;

class streamBuffer
{
public:
  vector<uint32_t> sB;
  streamBuffer(uint32_t m) { sB = vector<uint32_t>(m, 0); }
};

class prefetcher
{
public:
  vector<streamBuffer> pF;
  vector<pair<int, int>> pFlru;
  uint32_t n;
  uint32_t m;
  int bs;
  int tagIndLen;
  int hitRet;

  prefetcher(uint32_t n_num = 0, uint32_t m_num = 0, int BS = 0,
             int tagInd_len = 0)
  {
    n = n_num;
    m = m_num;
    tagIndLen = tagInd_len;
    bs = 1;
    hitRet = 0;
    pF = vector<streamBuffer>(n_num, streamBuffer(m_num));
    pFlru = vector<pair<int, int>>(n_num, {0, 0});
    for (int i = 0; i < n; i++)
    {
      pFlru[i] = {i, i};
    }
  }

  string getRightPaddingString(string const &str, int n = 32,
                               char paddedChar = '0')
  {
    ostringstream ss;
    ss << left << setfill(paddedChar) << setw(n) << str;
    return ss.str();
  }

  uint32_t addrDecodePf(uint32_t addr)
  {
    string addrStr = (bitset<32>(addr)).to_string();
    string addrTillInd = addrStr.substr(0, tagIndLen);
    // padd with zeros
    // getRightPaddingString(addrTillInd);
    uint32_t addrBlock = stoi(addrTillInd, 0, 2);
    return addrBlock;
  }
  void updateLRU(uint32_t val)
  {

    for (int i = 0; i < n; i++)
    {
      if (pFlru[i].first < val)
      {
        pFlru[i].first++;
      }
      else if (pFlru[i].first == val)
      {
        pFlru[i].first = 0;
      }
    }
  }
  bool hitOrMissPf(uint32_t addr, bool change = true)
  {
    hitRet = 0;

    uint32_t searchAddr = addrDecodePf(addr);
    sort(pFlru.begin(), pFlru.end());

    for (int i = 0; i < n; i++)
    {
      pair<int, int> x = pFlru[i];
      long long int idx = x.second;
      streamBuffer tempbuf(0);
      for (int j = 0; j < m; j++)
        if (searchAddr == pF[idx].sB[j])
        {

          if (!change)
          {
            return true;
          }
          for (int i = 0; i < m; i++)
          {
            tempbuf.sB.push_back(searchAddr + (bs * (i + 1)));
          }
          if (change)
          {
            updateLRU(x.first);
            pF[idx] = tempbuf;
          }
          assert(tempbuf.sB.size() == m);
          hitRet = j + 1;
          sort(pFlru.begin(), pFlru.end());
          return true;
        }
    }
    int mx = pFlru.back().first, mxi = pFlru.back().second;
    streamBuffer tempbuf(0);
    for (int i = 0; i < m; i++)
    {
      tempbuf.sB.push_back(searchAddr + bs * (i + 1));
    }
    assert(tempbuf.sB.size() == m);

    if (!change)
    {
      sort(pFlru.begin(), pFlru.end());
      return false;
    }
    pF[mxi] = tempbuf;
    assert(mx == n - 1);
    hitRet = m;
    updateLRU(pFlru.back().first);
    sort(pFlru.begin(), pFlru.end());
    return false;
  }

  void printAll()
  {
    sort(pFlru.begin(), pFlru.end());

    for (int i = 0; i < n; i++)
    {
      pair<int, int> x = pFlru[i];
      int idx = x.second;
      for (int j = 0; j < m; j++)
        cout << setw(8) << hex << pF[idx].sB[j] << dec << "\t";
      cout << endl;
    }
  }
};

class block
{
public:
  bool validBit;
  bool dirtyBit;
  int lruCount;
  string tag;

  block(uint32_t assoc)
  {
    validBit = 0;
    dirtyBit = 0;
    lruCount = 0;
    tag = "";
  }
};

class Set
{
public:
  vector<block> blockArr;

  Set(uint32_t assoc) { blockArr = vector<block>(assoc, block(assoc)); }
};

class genericCache
{
public:
  vector<Set> setArr;
  uint32_t size;
  uint32_t assoc;
  uint32_t blockSize;
  int sets; // finding number of sets

  int blockOffsetLen;
  int indexLen;
  int tagLen;

  unsigned int reads;
  unsigned int readMisses;
  unsigned int writes;
  unsigned int writeMisses;
  unsigned int writeBacks;
  unsigned int prefetches;
  unsigned int memoryTraffic;
  unsigned int readsPrefetches;
  unsigned int readMissesPrefetches;

  genericCache(uint32_t s = 0, uint32_t a = 0, uint32_t bs = 0)
  {
    ct = 0; // restarting global count for lru counter
    size = s;
    assoc = a;
    blockSize = bs;

    if (bs > 0 && a > 0)
    {
      sets = s / (bs * a);
    }
    else
    {
      sets = 0;
    }

    setArr = vector<Set>(sets, Set(a));

    blockOffsetLen = log2(bs);                       // log base 2 of blockSize
    indexLen = log2(sets);                           // log base 2 of numSets
    tagLen = addressLen - indexLen - blockOffsetLen; // number of tag bits

    reads = 0;
    readMisses = 0;
    writes = 0;
    writeMisses = 0;
    writeBacks = 0;
    prefetches = 0;
    memoryTraffic = 0;
    readsPrefetches = 0;
    readMissesPrefetches = 0;
  }

  struct addrDecode
  {
    string addrStr;
    string tag;
    string index;
    int indexInt;
  };

  struct addrDecode addrDecodeFunc(uint32_t addr)
  {
    struct addrDecode x;

    x.addrStr = (bitset<32>(addr)).to_string();
    x.tag = x.addrStr.substr(
        0, tagLen);
    x.index = x.addrStr.substr(
        tagLen,
        indexLen);
    if (indexLen > 0)
      x.indexInt = stoi(
          x.index, 0,
          2);
    else
    {
      x.indexInt = 0;
    }
    x.indexInt = max(0, x.indexInt);

    return x;
  }

  unsigned int addrEncode(const string tag, const string index)
  {
    unsigned int val = 0;
    int i = 0;
    string tempStr = tag + index;
    for (int i = 0; i < blockOffsetLen; i++)
    {
      tempStr = tempStr + "0";
    }

    if (tempStr == "")
      return 0;

    while (tempStr[i] == '0' || tempStr[i] == '1')
    { // Found another digit.
      val <<= 1;
      val += tempStr[i] - '0';
      i++;
    }
    return val;
  }

  int *hitOrMiss(uint32_t addr)
  {
    // returns array [hit/miss?, index hit, blockoffset hit]
    // split address into tag,index,offset

    struct addrDecode x = addrDecodeFunc(addr);
    static int tempArr[3];
    tempArr[0] = 0;
    tempArr[1] = 0;
    tempArr[2] = 0;
    for (int i = 0; i < assoc; i++)
    {

      if (setArr[x.indexInt].blockArr[i].tag == x.tag)
      {
        tempArr[0] = 1;
        tempArr[1] = x.indexInt;
        tempArr[2] = i;

        break;
      }
    }

    return tempArr;
  }

  void updateLRUcountWithinSet(int index, int blockOffset)
  {
    /*
    every block within a set has a lruCount by default. we place addresses
    within blocks based on the order. if we are to place a block within a set,
    how many scenarios are there when we need to reorder the lruCount within a
    set?

    */

    for (int i = 0; i < assoc; i++)
    {
      if ((i != blockOffset) &&
          (setArr[index].blockArr[i].lruCount <
           setArr[index].blockArr[blockOffset].lruCount))
      {
        int temp = setArr[index].blockArr[i].lruCount;
        setArr[index].blockArr[i].lruCount = temp + 1;
      }
    }

    setArr[index].blockArr[blockOffset].lruCount = 0;
  }

  void setDirtyBit(int setBit, int index, int blockOffset)
  {

    if ((setBit == 0) || (setBit == 1))
    {
      setArr[index].blockArr[blockOffset].dirtyBit = setBit;
    }
  }

  bool getDirtyBit(int index, int blockOffset)
  {

    return setArr[index].blockArr[blockOffset].dirtyBit;
  }

  string *evictOrPlace(uint32_t addr)
  {
    // this function helps us decide weather we must evict a block to lower
    // level, or replace the full set's lru block or just place the block in the
    // empty space available in cache set search if set is full of valid blocks
    // if yes, return 1, dirty bit, addr stored in lru block
    // if no, return 0, 0, 0

    struct addrDecode x = addrDecodeFunc(addr);

    static string tempArr[3];
    for (int i = 0; i < assoc; i++)
    {
      if (setArr[(x.indexInt)].blockArr[i].lruCount == (assoc - 1))
      {
        // we assume that if lruCount max block is valid then set is full
        if (setArr[(x.indexInt)].blockArr[i].validBit)
        {
          tempArr[0] = "evict";
          tempArr[1] = to_string(setArr[(x.indexInt)].blockArr[i].dirtyBit);
          tempArr[2] = to_string(
              addrEncode(setArr[(x.indexInt)].blockArr[i].tag, x.index));
        }
        else
        {
          tempArr[0] = "place";
          tempArr[1] = "0";
          tempArr[2] = "0";
        }
        return tempArr;
      }
    }
  }

  void placeMissedBlockInCacheSet(uint32_t addr,
                                  string dirtyOrClean = "clean")
  {
    // write the address to set as per index and to the block that has the
    // largest lruCount.
    struct addrDecode x = addrDecodeFunc(addr);

    for (int i = 0; i < assoc; i++)
    {
      if (setArr[(x.indexInt)].blockArr[i].lruCount == (assoc - 1))
      {
        setArr[(x.indexInt)].blockArr[i].validBit = 1;
        setArr[(x.indexInt)].blockArr[i].tag = x.tag;
        updateLRUcountWithinSet((x.indexInt), i);
        if (dirtyOrClean == "dirty")
        {
          setDirtyBit(1, (x.indexInt), i);
        }
        else
        {
          setDirtyBit(0, (x.indexInt), i);
        }
        break;
      }
    }
  }

  void printBlockContents(int index, int blockOffset)
  {
    cout << "V" << setArr[index].blockArr[blockOffset].validBit << "D"
         << setArr[index].blockArr[blockOffset].dirtyBit << "C"
         << setArr[index].blockArr[blockOffset].lruCount << "T"
         << setArr[index].blockArr[blockOffset].tag;
  }

  void printSetContents(int index)
  {
    cout << "set " << index << ": ";
    for (int i = 0; i < assoc; i++)
    {
      printBlockContents(index, i);
      cout << "\t";
    }
    cout << endl;
  }

  void printCacheContents(string selector)
  {
    for (int i = 0; i < sets; i++)
    {
      cout << "index: " << i << "\t";
      for (int j = 0; j < assoc; j++)
      {
        if (selector == "all")
        {
          printBlockContents(i, j);
          cout << "\t";
        }
        else if (selector == "lruCount")
        {
          cout << setArr[i].blockArr[j].lruCount << "\t";
        }
      }
      cout << endl;
    }
  }
  uint32_t convertBinStr2Uint(string str1)
  {
    unsigned int val = 0;
    int i = 0;

    while (str1[i] == '0' || str1[i] == '1')
    {
      // Found another digit.
      val <<= 1;
      val += str1[i] - '0';
      i++;
    }
    return val;
  }
  void printCacheContentsMRU2LRU()
  {
    // if a block is invalid, dont print that block. if all blocks are invalid
    // dont print that set print blocks in a set in MRU to LRU fashion
    for (int i = 0; i < sets; i++)
    {
      vector<block> myarr(assoc, block(assoc));
      int validSetFlag = 0;
      int setPrintFlag = 1;
      for (int j = 0; j < assoc; j++)
      {
        if (setArr[i].blockArr[j].validBit)
        {
          validSetFlag = 1;
          myarr.at(setArr[i].blockArr[j].lruCount) = setArr[i].blockArr[j];
        }
      }
      if (validSetFlag)
      {
        if (setPrintFlag)
        {
          cout << "set   " << i << ":  ";
          setPrintFlag = 0;
        }
        for (int k = 0; k < assoc; k++)
        {
          cout << setw(8) << hex << convertBinStr2Uint(myarr.at(k).tag)
               << dec; // printing in hex and reverting back to dec as default
          myarr.at(k).dirtyBit ? cout << " D " : cout << " ";
          cout << "C" << myarr.at(k).lruCount << "  ";
        }
        cout << endl;
      }
    }
  }

  void printSimpleContentsMru2Lru()
  {
    for (int i = 0; i < sets; i++)
    {
      int setPrintFlag = 1;
      for (int k = 0; k < assoc; k++)
      {
        for (int j = 0; j < assoc; j++)
        {
          if (setArr[i].blockArr[j].validBit)
          {
            if (setPrintFlag)
            {
              setPrintFlag = 0;
              cout << "set   " << i << ":   ";
            }

            if (k == setArr[i].blockArr[j].lruCount)
            {
              cout << setw(8) << hex
                   << convertBinStr2Uint(setArr[i].blockArr[j].tag)
                   << dec; // printing in hex and reverting back to dec as
                           // default
              setArr[i].blockArr[j].dirtyBit ? cout << " D  " : cout << "    ";
            }
          }
        }
      }
      if (!setPrintFlag)
      {
        cout << endl;
      }
    }
  }
};

#endif
