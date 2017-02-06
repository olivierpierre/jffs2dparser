#include <regex.h>
#include <sstream>
#include <cstdlib>
#include <assert.h>

#include "ChunkModel.hpp"

/************************* Chunk **************************************/

Chunk::Chunk(){}
int Chunk::build(string line)
{
  // just get the type
  
  string free_space_start = "Empty space";
  string data_node_start = "         Inode";
  string dirent_node_start = "         Dirent";
  
  if(!line.compare(0, free_space_start.size(), free_space_start))
    _type = FREE_SPACE;
  else if(!line.compare(0, data_node_start.size(), data_node_start))
    _type = DATA_NODE;
  else if(!line.compare(0, dirent_node_start.size(), dirent_node_start))
    _type = DIRENT_NODE;
  else
  {
    cerr << "ERROR : cant build chunk from this line :" << endl;
    cerr << "  \"" << line << "\"" << endl;
    return -1;
  }
  return 0;
}

chunk_type Chunk::getType()
{
  return _type;
}

/************************* FreeSpaceChunk *****************************/

FreeSpaceChunk::FreeSpaceChunk() : Chunk(){}

int FreeSpaceChunk::build(string line)
{
  Chunk::build(line);
  // get on-flash start and end offsets
  regex_t exp;
  string fsc_regexp = "0x([0-9a-f]*) to 0x([0-9a-f]*)";
  string res[2];
  regmatch_t matches[3];
  
  // compile regex
  if(regcomp(&exp, fsc_regexp.c_str(), REG_EXTENDED))
  {
    cerr << "Error compiling regexp " << fsc_regexp << endl;
    regfree(&exp);
    return -1;
  }
  
  // execute regex
  if(regexec(&exp, line.c_str(), 3, matches, 0))
  {
    cerr << "Error executing free space chunk regexp " << fsc_regexp << " on :" << endl;
    cerr << line << endl;
    regfree(&exp);
    return -1;
  }

  res[0] = line.substr(matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
  res[1] = line.substr(matches[2].rm_so, matches[2].rm_eo - matches[2].rm_so);
  
  // convert hex to uint
  stringstream ss;
  uint64_t start_offset, end_offset;
  ss << std::hex << res[0];
  ss >> start_offset;
  ss.clear();
  ss << std::hex << res[1];
  ss >> end_offset;
  
  _start = FlashAddr(start_offset + FlashAddr::getPartitionOffset());
  _end = FlashAddr(end_offset + FlashAddr::getPartitionOffset());
  
   // cout << "Free space" << endl << "  start_offset : " << start_offset << endl <<
  				  // "  end_offset : " << end_offset << endl;
  
  regfree(&exp);
  return 0;

}

ostream& operator<<(ostream& os, FreeSpaceChunk& fsc )
{
  os << "Free space " << fsc._start << " -> " << fsc._end;
  return os;
}

/************************* Node ***************************************/

Node::Node() : Chunk(){}
int Node::build(string line)
{
  Chunk::build(line);
  // Get flash offset, flash size, inode & version num
  regex_t exp;
  string node_regex = "node at 0x([0-9a-f]*).*totlen 0x([0-9a-f]*).*#ino[ ]*([0-9]*)";
  string node_regex2 = "version[ ]*([0-9]*)";
  regmatch_t matches[4];
  regmatch_t matches2[2];
  string flash_offset, flash_size, inode_num, version_num;
  
  // compile regex
  if(regcomp(&exp, node_regex.c_str(), REG_EXTENDED))
  {
    cerr << "Error compiling regexp " << node_regex << endl;
    regfree(&exp);
    return -1;
  }
  
  // execute regex
  if(regexec(&exp, line.c_str(), 4, matches, 0))
  {
    cerr << "Error executing regexp " << node_regex << " on :" << endl;
    cerr << line << endl;
    regfree(&exp);
    return -1;
  }
  
  regfree(&exp);
  
  // compile regexp2
  if(regcomp(&exp, node_regex2.c_str(), REG_EXTENDED))
  {
    cerr << "Error compiling regexp " << node_regex2 << endl;
    regfree(&exp);
    return -1;
  }
  
  // execute regex2
  if(regexec(&exp, line.c_str(), 2, matches2, 0))
  {
    cerr << "Error executing regexp " << node_regex2 << " on :" << endl;
    cerr << line << endl;
    regfree(&exp);
    return -1;
  }
  
  flash_offset = line.substr(matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
  flash_size = line.substr(matches[2].rm_so, matches[2].rm_eo - matches[2].rm_so);
  inode_num = line.substr(matches[3].rm_so, matches[3].rm_eo - matches[3].rm_so);
  version_num = line.substr(matches2[1].rm_so, matches2[1].rm_eo - matches2[1].rm_so);
  
  // convert hex to uint when needed
  uint64_t flash_offset_tmp;
  stringstream ss;
  ss << std::hex << flash_offset;
  ss >> flash_offset_tmp;
  _flash_offset = FlashAddr(flash_offset_tmp + FlashAddr::getPartitionOffset());
  ss.clear();
  ss << std::hex << flash_size;
  ss >> _flash_size;
  _inode_num = atoi(inode_num.c_str());
  _version_num = atoi(version_num.c_str());
  
  // cout << "Node" << endl << "  flash_offset : " << _flash_offset << endl <<
			    // "  flash_size : " << _flash_size << endl << 
			    // "  inode_num : " << _inode_num << endl <<
			    // "  version num : " << _version_num << endl;
  
  regfree(&exp);
  return 0;
}

uint32_t Node::getFlashSize()
{
  return _flash_size;
}

uint64_t Node::getInodeNum()
{
  return _inode_num;
}

uint32_t Node::getVersionNum()
{
  return _version_num;
}

/**
 * TODO comm here
 */
vector<int> Node::getConcernedPagesIndexes()
{
  vector<int> res;
  
  assert(_flash_size != 0);
  
  int first_page = _flash_offset.getFlashOffset() / FlashAddr::getFlashPageSize();
  int last_page = (_flash_offset.getFlashOffset() + _flash_size-1) / FlashAddr::getFlashPageSize();
  int total_page_num = last_page - first_page + 1;
  
  assert(total_page_num > 0);
  
  for(int i=0; i<total_page_num; i++)
    res.push_back(first_page+i);
  
  return res;
}

/************************* DataNode************************************/

DataNode::DataNode() : Node(){}
int DataNode::build(string line)
{
  Node::build(line);
  string file_size, compressed_size, data_size, offset;
  regex_t exp;
  string datanode_regexp = "isize[ ]*([0-9]*).*csize[ ]*([0-9]*).*dsize[ ]*([0-9]*).*offset[ ]*([0-9]*)";
  regmatch_t matches[5];
  
  // compile regex
  if(regcomp(&exp, datanode_regexp.c_str(), REG_EXTENDED))
  {
    cerr << "Error compiling regexp " << datanode_regexp << endl;
    regfree(&exp);
    return -1;
  }
  
  // execute regex
  if(regexec(&exp, line.c_str(), 5, matches, 0))
  {
    cerr << "Error executing datanode regexp " << datanode_regexp << " on :" << endl;
    cerr << line << endl;
    regfree(&exp);
    return -1;
  }
  
  file_size = line.substr(matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
  compressed_size = line.substr(matches[2].rm_so, matches[2].rm_eo - matches[2].rm_so);
  data_size = line.substr(matches[3].rm_so, matches[3].rm_eo - matches[3].rm_so);
  offset = line.substr(matches[4].rm_so, matches[4].rm_eo - matches[4].rm_so);
  
  _file_size = atoi(file_size.c_str());
  _compressed_size = atoi(compressed_size.c_str());
  _data_size = atoi(data_size.c_str());
  _offset = atoi(offset.c_str());
  
  // cout << "Data Node : " << endl << "  file_size : " << _file_size << endl <<
				    // "  compressed_size : " << _compressed_size << endl <<
				    // "  data_size : " << _data_size << endl <<
				    // "  offset : " << _offset << endl;
  
  regfree(&exp);
  return 0;
}

/**
 * Return the index of the flash page containing the byte at offset
 * Note that offset is relative to the starting offset of the node in 
 * the corresponding file, and not to the start of the file itself
 */
int DataNode::getConcernedPageAtOffset(uint32_t offset)
{
  assert(offset < _data_size);
  return ((_flash_offset.getFlashOffset() + offset)/FlashAddr::getFlashPageSize());
}

uint32_t DataNode::getFileSize()
{
  return _file_size;
}

ostream& operator<<(ostream& os, DataNode& dn )
{
  FlashAddr end(dn._flash_offset.getFlashOffset() + dn._flash_size -1);
  
  os <<  "Data node " << dn._flash_offset << " -> " << end << " ";
  os << dn._inode_num << "v" << dn._version_num;
  if(dn._data_size == 0)
    os << "(no data)";
  else
    os << "(@" << dn._offset << "->" << (dn._offset + dn._data_size -1) << ")";
  os << " c:" << dn._data_size << "->" << dn._compressed_size;
  os << " f:" << dn._file_size;
  return os;
}

uint32_t DataNode::getDataOffset()
{
  return _offset;
}

uint32_t DataNode::getDataSize()
{
  return _data_size;
}

/************************* DirentNode *********************************/

DirentNode::DirentNode() : Node(){}
int DirentNode::build(string line)
{
  Node::build(line);
  
  string parent_inode_num, name_size, name;
  regex_t exp;
  string direntnode_regexp = "#pino[ ]*([0-9]*).*nsize[ ]*([0-9]*).*name (.*)$";
  regmatch_t matches[4];
  
  // compile regexp
  if(regcomp(&exp, direntnode_regexp.c_str(), REG_EXTENDED))
  {
    cerr << "Error compiling regexp " << direntnode_regexp << endl;
    regfree(&exp);
    return -1;
  }
  
  // execute regex
  if(regexec(&exp, line.c_str(), 4, matches, 0))
  {
    cerr << "Error executing regexp " << direntnode_regexp << " on :" << endl;
    cerr << line << endl;
    regfree(&exp);
    return -1;
  }
  
  parent_inode_num = line.substr(matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
  name_size = line.substr(matches[2].rm_so, matches[2].rm_eo - matches[2].rm_so);
  name = line.substr(matches[3].rm_so, matches[3].rm_eo - matches[3].rm_so);
  
  _parent_inode_num = atoi(parent_inode_num.c_str());
  _name_size = atoi(name_size.c_str());
  _name = name;
  
  // cout << "Dirent Node : " << endl << "  parent_inode_num : " << _parent_inode_num << endl <<
				      // "  name size : " << _name_size << endl <<
				      // "  name : " << _name << endl;
  
  regfree(&exp);
  return 0;
}

uint64_t DirentNode::getParentInodeNum()
{
  return _parent_inode_num;
}

string DirentNode::getName()
{
  return _name;
}

ostream& operator<<(ostream& os, DirentNode& dn )
{
  FlashAddr end(dn._flash_offset.getFlashOffset() + dn._flash_size -1);
  
  os << "Dirent node " << dn._flash_offset << " -> " << end ;
  os << " \"" << dn._name << "\"v" << dn._version_num;
  os << " p:" << dn._parent_inode_num;
  return os;
}

/**
 * Sort data nodes in the array by version num
 */
int sortChunkArray(vector<Chunk *> &vec)
{
  bool swap_done;
  do
  {
    swap_done = false;
    for(int i=0; i<((int)vec.size()-1); i++)
      if(vec[i]->getType() == DATA_NODE)
      {
	DataNode *first = static_cast<DataNode *>(vec[i]);
	
	// find next DN
	int j=i+1;
	while(j < (int)vec.size() && vec[j]->getType() != DATA_NODE)
	  j++;
	if(j == (int)vec.size())
	  continue;
	  
	DataNode *second = static_cast<DataNode *>(vec[j]);
	
	if(first->getVersionNum() < second->getVersionNum())
	{
	  vec[i] = second;
	  vec[j] = first;
	  swap_done = true;
	}
      }
  } while(swap_done == true);
      
  return 0;
}
