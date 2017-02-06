#ifndef FLASH_ADDR_HPP
#define FLASH_ADDR_HPP

#include <iostream>
#include <cstdlib>

using namespace std;

typedef unsigned long long int 		uint64_t;
typedef unsigned int 			uint32_t;

class FlashAddr
{
  public:
    FlashAddr();
    FlashAddr(uint64_t offset);
    uint64_t getFlashOffset();
    uint32_t getFlashPage();
    uint32_t getFlashBlock();
    int getOffsetOfPageInBlock();
    int getByteOffsetInPage();
    bool isStartOfAPage();
    bool isStartOfABlock();
    
    static int getFlashPageSize();
    static int getNumPagesPerBlock();
    static int getPartitionOffset();
    static void init(int page_size_in_bytes, int pages_per_block);
    static void init(int page_size_in_bytes, int pages_per_block, int partition_offset);
  
  private:
    uint64_t _flash_offset;
    bool _valid;
    static int _page_size_in_bytes;
    static int _pages_per_block;
    static int _partition_offset;
    
  friend ostream& operator<<(ostream& os, FlashAddr& fa );
    
};

#endif /* FLASH_ADDR_HPP */
