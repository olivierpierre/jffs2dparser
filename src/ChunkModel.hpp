#ifndef CHUNK_MODEL_HPP
#define CHUNK_MODEL_HPP

#include <iostream>
#include <string>
#include <vector>

#include "FlashAddr.hpp"

using namespace std;

typedef unsigned long long int 		uint64_t;
typedef unsigned int 			uint32_t;

typedef enum {FREE_SPACE, DATA_NODE, DIRENT_NODE} chunk_type;

class Chunk
{
  public:
    Chunk();
    int build(string line);
    chunk_type getType();
    
  private:
    chunk_type _type;
};

class FreeSpaceChunk : public Chunk
{
  public:
    FreeSpaceChunk();
    int build(string line);
    
  private:
    // valid after parsing
    FlashAddr 	_start;		// start offset for the free space chunk
    FlashAddr	_end;			// end
    
  friend ostream& operator<<(ostream& os, FreeSpaceChunk& fsc );
};

class Node : public Chunk
{
  public:
    Node();
    int build(string line);
    uint64_t getInodeNum();
    uint32_t getVersionNum();
    vector<int> getConcernedPagesIndexes();
    uint32_t getFlashSize();
    
  protected:
    // valid after parsing
    FlashAddr 	_flash_offset;		// location on flash
    uint32_t 	_flash_size;			// size on flash for the node
    uint64_t 	_inode_num;			// corresponding file inode
    uint32_t 	_version_num;			// version
};

class DataNode : public Node
{
  public:
    DataNode();
    int build(string line);
    uint32_t getFileSize();
    uint32_t getDataOffset();
    uint32_t getDataSize();
    int getConcernedPageAtOffset(uint32_t offset);
    vector<int> getContainingPages();
    
  private:
    // valid after parsing
    uint32_t 	_file_size;			// file size at the moment this datanode was created
    uint32_t 	_compressed_size;		// compressed data size for this datanode
    uint32_t 	_data_size;			// uncompressed data size
    uint32_t 	_offset;				// location of the data in the corresponding file
    
  friend ostream& operator<<(ostream& os, DataNode& dn );
};

class DirentNode : public Node
{
  public:
    DirentNode();
    int build(string line);
    uint64_t getParentInodeNum();
    string getName();
    
  private:
    // valid after parsing
    uint64_t	_parent_inode_num;		// inode num of parent dir at the moment this node was created
    int		_name_size;			// size of the name in bytes at the moment this node was created
    string 	_name;				// name of the corresponding file at the moment this node was created
    
  friend ostream& operator<<(ostream& os, DirentNode& dn );
};

int sortChunkArray(vector<Chunk *> &vec);

#endif /* CHUNK_MODEL_HPP */
