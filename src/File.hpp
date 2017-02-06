#ifndef FILE_HPP
#define FILE_HPP

#include <iostream>
#include <vector>
#include <string>

#include "ChunkModel.hpp"

using namespace std;

class File
{
  public:
    File(uint64_t inode_num);
    
    uint32_t getSize();
    uint64_t getInodeNum();
    uint64_t getParentInodeNum();
    string getName();
    vector<int> getConcernedPagesIndexes();
    double getFragmentationFactor();
    double getContiguousFactor();
    int getSequentialReadCost();
    int getLinuxPageReadCost(int page_index);
    int getLinuxPagesNum();
    void printSequentialPerPageReadCost();
    
  private:
    uint64_t _inode_num;
    vector<DataNode *> _all_data_nodes;
    vector<DataNode *> _valid_data_nodes;
    DirentNode *_valid_dirent_node;
    vector<DirentNode *> _all_dirent_nodes;
    bool _was_deleted;
    bool _is_final;
    int _sequential_cost;
    
    int addNode(DataNode &dn);
    int addNode(DirentNode &dn);
    int set_valid_dirent(vector<Chunk *> &chunk_list);
    int set_valid_datanodes();
    DataNode *getMostRecentDataNode();
    DataNode *getValidDataNodeAtOffset(uint32_t offset);
    int addValidDataNodeIfNotAlreadyPresent(DataNode *dn);
    int getTheoriticalPageNum();
    int finalize(vector<Chunk *> &chunk_list);
    vector<int> getFlashPagesReadForLinuxPage(int linux_page_index);
    
  friend class FileSet;
  friend ostream& operator<<(ostream& os, File& f);
};

class FileSet
{
  public:
    FileSet(vector<Chunk *> &chunk_list);

  private:
    vector<File> _files;
    
    int addNode(DataNode &dn);
    int addNode(DirentNode &dn);
    int findFile(uint64_t inode_num, File **file);
    uint32_t getMostRecentDirentVersion(uint64_t inode_num);
    
  friend ostream& operator<<(ostream& os, FileSet& fs);
};

#endif /* FILE_HPP */
