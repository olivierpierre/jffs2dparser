#include <assert.h>
#include <string.h>

#include "File.hpp"

#define JFFS2_MAX_DATANODE_DATA_SIZE		4096
#define JFFS2_DATANODE_METADATA_SIZE		68
#define JFFS2_MAX_DATANODE_SIZE			(JFFS2_MAX_DATANODE_DATA_SIZE+JFFS2_DATANODE_METADATA_SIZE)
#define LINUX_PAGE_SIZE				4096

bool addToArrayIfNotAlreadyPresent(int val, vector<int> &vec);
bool addToArrayIfDifferentFromLastElement(int val, vector<int> &vec);

/**************************** File ************************************/

File::File(uint64_t inode_num)
{
  _inode_num = inode_num;
  _is_final = false;
  _was_deleted = false;
  _sequential_cost = -1;
  
  _valid_dirent_node = NULL;
}

/**
 * Return the number of linux pages constituting the file (according to its size)
 */
int File::getLinuxPagesNum()
{
  int res;
  uint32_t file_size = getSize();
  assert(_is_final);
  assert(file_size > 0);
  
  res = file_size / LINUX_PAGE_SIZE;
  if (file_size % LINUX_PAGE_SIZE != 0)
    res++;
    
  return res;
}

/**
 * Return the cost of reading the file sequentially for each linux page
 * taking into account the mtd buffer (if the last flash page read for linux 
 * page i is equal to the first of page i+1, the second flash page read is
 * not taken into account
 */
void File::printSequentialPerPageReadCost()
{
  int linux_pages_num = getLinuxPagesNum();
  int prev_last_flash_page_index = -1;
  
  for(int i=0; i<linux_pages_num; i++)
  {
    int number_of_flash_pages_read = 0;
    vector<int> flash_pages_read = getFlashPagesReadForLinuxPage(i);
    
    number_of_flash_pages_read = flash_pages_read.size();
    if(flash_pages_read[0] == prev_last_flash_page_index)
      number_of_flash_pages_read--;
      
    cout << "  readpage[" << i << "] = " << number_of_flash_pages_read << endl;
    
    prev_last_flash_page_index = flash_pages_read[flash_pages_read.size()-1];
  }
}

/**
 * Return the number of flash pages read triggered by the read of one linux 
 * flash page
 */
int File::getLinuxPageReadCost(int page_index)
{
  int res = -1;
  
  res = getFlashPagesReadForLinuxPage(page_index).size();
  
  assert(res != -1);
  return res;
}

/**
 * Return the indexes of all the flash pages read when reading a linux page
 */
vector<int> File::getFlashPagesReadForLinuxPage(int linux_page_index)
{
  vector<int> pages;
  DataNode *dn, *prev_dn;
  uint32_t start_offset_in_file = (uint32_t)linux_page_index * LINUX_PAGE_SIZE;
  dn = prev_dn = NULL;
  
  if(start_offset_in_file >= getSize())
  {
    cerr << "Error, calling getFlashPagesReadForLinuxPage on page index (" 
    << linux_page_index << ") > max file size (" << getSize() << ")" << endl;
    return pages;
  }
  
  for(uint32_t i=start_offset_in_file; (i<(start_offset_in_file+LINUX_PAGE_SIZE) && i<getSize()); i++)
  {
    dn = getValidDataNodeAtOffset(i);
    if(dn == prev_dn)
      continue;
      
    vector<int> pages_indexes = dn->getConcernedPagesIndexes();
    for(int j=0; j<(int)pages_indexes.size(); j++)
      addToArrayIfDifferentFromLastElement(pages_indexes[j], pages);
    prev_dn = dn;
  }
  
  assert(pages.size() > 0);
  return pages;
}

/**
 * Return the list of pages containing the valid nodes for that file
 */
vector<int> File::getConcernedPagesIndexes()
{
  vector<int> res;
  
  assert(_is_final);
  
  for(int i=0; i<(int)_valid_data_nodes.size(); i++)
  {
    vector<int> tmp = _valid_data_nodes[i]->getConcernedPagesIndexes();
    for(int j=0; j<(int)tmp.size(); j++)
      addToArrayIfNotAlreadyPresent(tmp[j], res);
  }
  
  return res;
}

/**
 * The frag. factor is computed as the actual number of
 * pages containing the valid nodes divided by the minimal
 * number of pages needed to store the valid data nodes 
 */
double File::getFragmentationFactor()
{
  double res = 0.0;
  int actual_page_num = getConcernedPagesIndexes().size();
  int theoritical_page_num = getTheoriticalPageNum();
  
  res = (double)actual_page_num / (double)theoritical_page_num;
  
  return res;
}

/**
 * Return the minimal number of pages needed to store the
 * valid data nodes.
 * Here we assume the file is divided into a minimal number of data nodes
 * wich are then maximal sized nodes of 4096 bytes + 68 bytes of metadata
 */
int File::getTheoriticalPageNum()
{
  int total_nodes_needed = 0;
  int res = 0;
  int flash_page_size = FlashAddr::getFlashPageSize();
  
  // for(int i=0; i<(int)_valid_data_nodes.size(); i++)
    // total_size += _valid_data_nodes[i]->getFlashSize();
    // 
  // res = total_size / flash_page_size;
  // if(total_size % flash_page_size != 0)
    // res += 1;
    
  total_nodes_needed = getSize() / (JFFS2_MAX_DATANODE_SIZE);
  if(getSize() % (JFFS2_MAX_DATANODE_SIZE) != 0)
    total_nodes_needed += 1;
  
  res = (total_nodes_needed*(JFFS2_MAX_DATANODE_SIZE)) / flash_page_size;
  if((total_nodes_needed*(JFFS2_MAX_DATANODE_SIZE)) % flash_page_size != 0)
    res += 1;
    
  return res;
}

/**
 * Here we must check if a node following another (in logical order)
 * starts on the same page as the end offset for the previous node.
 * This will basically tell us if the system will be able to benefit 
 * from the MTD page buffer during sequential reads.
 */ 
double File::getContiguousFactor()
{
  double res = 0.0;
  vector<int> pages;
  int total_pages_jumps, non_seq_pages_jumps;
  DataNode *dn, *prev_dn;
  dn = prev_dn = NULL;
  
  for(uint32_t i=0; i<getSize(); i++)
  {
    dn = getValidDataNodeAtOffset(i);
    if(dn == prev_dn)
      continue;
      
    vector<int> pages_indexes = dn->getConcernedPagesIndexes();
    for(int j=0; j<(int)pages_indexes.size(); j++)
      addToArrayIfDifferentFromLastElement(pages_indexes[j], pages);
    prev_dn = dn;
  }
  
  _sequential_cost = pages.size();
  
  total_pages_jumps = (int)pages.size() -1;
  non_seq_pages_jumps = 0;
  for(int i=1; i<(int)pages.size(); i++)
    if(pages[i] != (pages[i-1]+1))
      non_seq_pages_jumps++;
  
  res = (double)non_seq_pages_jumps / (double)total_pages_jumps;
  
  return res;
}

/**
 * Return the number of pages read if we read sequentially this file
 * It does take into account that we may read the same flash page 
 * several times because it contains data from multiple data nodes
 * that are not read one after another
 */
int File::getSequentialReadCost()
{
  if(_sequential_cost == -1)
    getContiguousFactor();
  return _sequential_cost;
}

/**
 * Return 1 if we must discard the file (lost datanode
 */
int File::finalize(vector<Chunk *> &chunk_list)
{
  int ret;
  
  if(_inode_num != 1)
  {
    ret = set_valid_dirent(chunk_list);
    if(ret < 0)
    {
      cerr << "Error finalizing (dirent) file " << getInodeNum() << endl;
      return -1;
    }
    if(ret == 1)
      return ret;
    
    if(set_valid_datanodes())
    {
      cerr << "Error finalizing (datanodes) file " << getInodeNum() << endl;
      return -1;
    }
  }
  
  _is_final = true;
  
  return 0;
}

/**
 * Returns 1 if we must discard the file because we are searching for a
 * dirent node that can't be linked to a data node
 */
int File::set_valid_dirent(vector<Chunk *> &chunk_list)
{
  uint32_t last_version = 0;
  
  // slash particular case
  if(_inode_num == 1)
    return 0;
    
  for(int i=0; i<(int)_all_dirent_nodes.size(); i++)
  {
    DirentNode *dn = _all_dirent_nodes[i];
    if(dn->getVersionNum() > last_version)
    {
      last_version = dn->getVersionNum();
      _valid_dirent_node = dn;
    }
  }
  
  if(last_version == 0)
    return 1;
  
  // okay now we must check if the file was not erased (dirent with 
  // #ino == 0 but the same name as the file and same parent ino
  for(int i=0; i<(int)chunk_list.size(); i++)
    if(chunk_list[i]->getType() == DIRENT_NODE)
    {
      DirentNode *dn = static_cast<DirentNode *>(chunk_list[i]);
      if(dn->getInodeNum() == 0 && last_version < dn->getVersionNum() && !strcmp(_valid_dirent_node->getName().c_str(), dn->getName().c_str()))
      {
	last_version = dn->getVersionNum();
	_valid_dirent_node = dn;
	_was_deleted = true;
      }
    }
    
  assert(last_version != 0);
  
  return 0;
}

/**
 * ultra ugly
 * TODO change here :
 * Now that the array of datanodes is sorted by version, starting from 
 * the most recent ones fill the holes in the file between @0 -> @size
 * when the entire file is found we're done
 */
int File::set_valid_datanodes()
{
  uint32_t size;
  
  // check if file was deleted
  assert(_valid_dirent_node != NULL);
  
  if(_valid_dirent_node->getInodeNum() == 0)
    return 0;
    
   size = getSize();
  
  /* look for each byte of the file which datanode contains it */
  for(uint32_t i=0; i<size; i++)
  {
    double process_state = double((double(i)*100)/(double(size)-1));
    cerr << "\r" << process_state << std::flush;
    
    DataNode *cur = getValidDataNodeAtOffset(i);
    addValidDataNodeIfNotAlreadyPresent(cur);
  }
  
  cout << endl;
  
  return 0;
}

int File::addValidDataNodeIfNotAlreadyPresent(DataNode *dn)
{
  bool already_present = false;
  
  for(int i=0; i<(int)_valid_data_nodes.size(); i++)
  {
    DataNode *cur = _valid_data_nodes[i];
    if(cur->getVersionNum() == dn->getVersionNum())
      already_present = true;
  }
  
  if(!already_present)
    _valid_data_nodes.push_back(dn);
  
  return 0;
}

/**
 * _all_data_nodes is sorted by version num so we just have to
 * find the first data node containing the needed offset
 */
DataNode * File::getValidDataNodeAtOffset(uint32_t offset)
{
  DataNode *res = NULL;
  uint32_t last_version = 0;
  
  assert(offset < getSize());
  
  // Old version on non sorted array
  // for(int i=0; i<(int)_all_data_nodes.size(); i++)
  // {
    // DataNode *cur = _all_data_nodes[i];
    // 
    // if(cur->getVersionNum() > last_version)
    // {
      // uint64_t data_node_start_offset = cur->getDataOffset();
      // uint64_t data_node_end_offset = data_node_start_offset + cur->getDataSize() -1;
      // if(offset >= data_node_start_offset && offset <= data_node_end_offset)
      // {
	// last_version = cur->getVersionNum();
	// res = cur;
      // }
    // }
  // }
  
  for(int i=0; i<(int)_all_data_nodes.size(); i++)
  {
    DataNode *cur = _all_data_nodes[i];
    uint64_t data_node_start_offset = cur->getDataOffset();
    uint64_t data_node_end_offset = data_node_start_offset + cur->getDataSize() -1;
    if(offset >= data_node_start_offset && offset <= data_node_end_offset)
    {
      last_version = cur->getVersionNum();
      res = cur;
      break;
    }
  }
  
  assert(res != NULL && last_version != 0);
  return res;
}

ostream& operator<<(ostream& os, File& f)
{
  if(f.getInodeNum() == 1)
    os << "  F: \"slash\"";
  else
  {
    os << "  F: \"" << f.getName() << "\"" << ((f._was_deleted) ? " [DELETED]" : "") 
      << ", size: " << f.getSize() << " ino:" << f._inode_num << ", size:" <<
      f.getSize() << ", pino:" << f.getParentInodeNum() << endl;
      
    cout << "    Data nodes versions : ";
    cout << f._all_data_nodes[0]->getVersionNum() << endl;
    
    cout << "    Valid data node versions : " << f._valid_data_nodes.size() << endl;
    // for(int i=0; i<(int)f._valid_data_nodes.size(); i++)
      // cout << f._valid_data_nodes[i]->getVersionNum() << ", ";
    // cout << endl;
    
    vector<int> flash_pages_indexes = f.getConcernedPagesIndexes();
    cout << "    Concerned flash pages indexes (" << flash_pages_indexes.size() << ") : " << endl;
    // for(int i=0; i<(int)flash_pages_indexes.size(); i++)
      // cout << flash_pages_indexes[i] << ", ";
    // cout << endl;
    // 
    cout << "    Fragmentation factor : " << f.getFragmentationFactor() 
      << endl;
      
    cout << "    Contiguous factor : " << f.getContiguousFactor() << endl;
    
    if(!f._was_deleted)
    {
      cout << "Seq. read cost : " << endl;
      f.printSequentialPerPageReadCost();
    }
  }
        
  return os;
}

uint32_t File::getSize()
{
  DataNode *most_recent = NULL;
  most_recent = getMostRecentDataNode();
  
  if(most_recent == NULL)	// File is deleted
    return 0;
  
  return most_recent->getFileSize();
}

uint64_t File::getInodeNum()
{
  return _inode_num;
}

uint64_t File::getParentInodeNum()
{
  if(!_is_final)
  {
    cerr << "Error calling getParentInodeNum on not final file" << endl;
    return 0;
  }
  return _valid_dirent_node->getParentInodeNum();
}

string File::getName()
{
  if(!_is_final)
  {
    cerr << "Error calling getName on not final File" << endl;
    return "";
  }
  return _valid_dirent_node->getName();
}

int File::addNode(DataNode &dn)
{
  _all_data_nodes.push_back(&dn);
  return 0;
}

int File::addNode(DirentNode &dn)
{
  _all_dirent_nodes.push_back(&dn);
  return 0;
}

/**
 * May return null if the file is deleted
 */
DataNode * File::getMostRecentDataNode()
{
  uint32_t last_version = 0;
  DataNode *res = NULL;
  
  // If the file is deleted
  if(_was_deleted)
    return NULL;
  
  for(int i=0; i<(int)_all_data_nodes.size(); i++)
  {
    DataNode *dn = _all_data_nodes[i];
    if(dn->getVersionNum() > last_version)
    {
      last_version = dn->getVersionNum();
      res = dn;
      break;			// _all_data_node is sorted by version
    }
  }
  
  assert(last_version > 0);
  assert(res != NULL);
  
  return res;
}

/****************************** FileSet *******************************/

FileSet::FileSet(vector<Chunk *> &chunk_list)
{
  // First add slash
  File slash(1);
  _files.push_back(slash);
  
  sortChunkArray(chunk_list);
  
  // next add the nodes present in chunk_list
  // the data nodes are sorted in that list so they are also sorted 
  // in the _all_data_node array
  for(int i=0; i<(int)chunk_list.size(); i++)
  {
    switch(chunk_list[i]->getType())
    {
      case DATA_NODE:
      {
	addNode(*(static_cast<DataNode *>(chunk_list[i])));
	break;
      }
      
      case DIRENT_NODE:
      {
	DirentNode *dn = static_cast<DirentNode *>(chunk_list[i]);
	if(dn->getInodeNum() != 0)
	  addNode(*dn);
	break;
      }
      
      default:
	break;
    }
  }
  
  for(int i=0; i<(int)_files.size(); i++)
  {
    cerr << "Processing file " << i+1 << "/" << (int)_files.size() << ": " << endl;
    if(_files[i].finalize(chunk_list) == 1)
    {
      _files.erase(_files.begin()+i);
      i--;
    }
  }
    
}

int FileSet::addNode(DataNode &dn)
{
  File *f = NULL;
  uint64_t inode_num = dn.getInodeNum();
  
  if(findFile(inode_num, &f))
  {
    File f2(inode_num);
    _files.push_back(f2);
    assert(findFile(inode_num, &f) == 0);
  }
  
  f->addNode(dn);
  
  return 0;
}

int FileSet::addNode(DirentNode &dn)
{
  File *f = NULL;
  uint64_t inode_num = dn.getInodeNum();
  
  if(findFile(inode_num, &f))
  {
    File f2(inode_num);
    _files.push_back(f2);
    assert(findFile(inode_num, &f) == 0);
  }
  
  f->addNode(dn);
  
  return 0;
}

/**
 * returns 0 if file found, -1 if not
 */
int FileSet::findFile(uint64_t inode_num, File **file)
{
  for(int i=0; i<(int)_files.size(); i++)
    if(_files[i]._inode_num == inode_num)
    {
      *file = &(_files[i]);
      return 0;
    }
    
  return -1;
}

ostream& operator<<(ostream& os, FileSet& f)
{
  os << "FileSet with " << f._files.size() << " files :" << endl;
    
  for(vector<File>::iterator it = f._files.begin(); it != f._files.end(); ++it)
    os << *it << endl;
    
  return os;
}

/****************************** Tools *********************************/
/**
 * Return false if the element is already present thus was not added
 * to the array
 */
bool addToArrayIfNotAlreadyPresent(int val, vector<int> &vec)
{
  bool present = false;
  
  for(int i=0; i<(int)vec.size(); i++)
  {
    if(vec[i] == val)
      present = true;
  }
     
  if(!present)
    vec.push_back(val);
    
  return present;
}

/**
 * Return false if the last element of the array is the same as val
 * thus was not added to the array
 */
bool addToArrayIfDifferentFromLastElement(int val, vector<int> &vec)
{
  bool need_add = true;
  
  if(!vec.empty())
    need_add = (vec[vec.size()-1] != val);
  
  if(need_add)
    vec.push_back(val);
  
  return need_add;
}
