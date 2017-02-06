#include <iomanip>

#include "FlashAddr.hpp"

/**************************** Flash address ***************************/
int FlashAddr::_pages_per_block = -1;
int FlashAddr::_page_size_in_bytes = -1;
int FlashAddr::_partition_offset = -1;

void FlashAddr::init(int page_size_in_bytes, int pages_per_block)
{
  _pages_per_block = pages_per_block;
  _page_size_in_bytes = page_size_in_bytes;
  _partition_offset = 0;
}

void FlashAddr::init(int page_size_in_bytes, int pages_per_block, int partition_offset)
{
  _pages_per_block = pages_per_block;
  _page_size_in_bytes = page_size_in_bytes;
  _partition_offset = partition_offset;
}

int FlashAddr::getFlashPageSize()
{
  return _page_size_in_bytes;
}

int FlashAddr::getPartitionOffset()
{
  return _partition_offset;
}

int FlashAddr::getNumPagesPerBlock()
{
  return _pages_per_block;
}

FlashAddr::FlashAddr()
{
  _valid = false;
}

FlashAddr::FlashAddr(uint64_t offset)
{
  if(FlashAddr::_page_size_in_bytes == -1 || FlashAddr::_pages_per_block == -1)
  {
    cerr << "Error, number of pages per block & page size in bytes are" 
      " not initialized." << endl;
    exit(-1);
  }
  
  _flash_offset = offset;
  
  // cout << offset << endl;
}

uint64_t FlashAddr::getFlashOffset()
{
  return _flash_offset;
}

uint32_t FlashAddr::getFlashPage()
{
  return _flash_offset/_page_size_in_bytes;
}

uint32_t FlashAddr::getFlashBlock()
{
  return ((_flash_offset/_page_size_in_bytes)/_pages_per_block);
}

int FlashAddr::getOffsetOfPageInBlock()
{
  return ((_flash_offset/_page_size_in_bytes)%_pages_per_block);
}

int FlashAddr::getByteOffsetInPage()
{
  return _flash_offset%_page_size_in_bytes;
}

bool FlashAddr::isStartOfAPage()
{
  return (_flash_offset%_page_size_in_bytes == 0);
}

bool FlashAddr::isStartOfABlock()
{
  return (_flash_offset%(_pages_per_block*_page_size_in_bytes) == 0);
}

ostream& operator<<(ostream& os, FlashAddr& fa )
{
  os << "@" << fa._flash_offset << "|" << fa.getByteOffsetInPage() << " [" << fa.getFlashPage() << ";" 
    << fa.getFlashBlock() << "|" << fa.getOffsetOfPageInBlock() << "]";
  return os;
}
