#include <string.h>

#include "Parser.hpp"

int insertDataNodeInVector(DataNode *dn, vector<Chunk *> &vec);

int parseStdIn(vector<Chunk *> &res)
{
  char line[256];
  
  while(cin.getline(line, 256))
    if(line[0] != '#' && line[0] != 'W')
      if(parseLine(line, res) < 0)
      {
	cerr << "Error parsing this line :" << endl;
	cerr << "  \"" << line << "\"" << endl;
	return -1;
      }
  return 0;
}

int parseFile(char *path, vector<Chunk *> &res)
{
  char line[256];
  ifstream parsed_file(path);
  
  if(!parsed_file)
  {
    cerr << "Can't open " << path << endl;
    return -1;
  }

    while(parsed_file.getline(line, 256))
      if(line[0] != '#' && line[0] != 'W')
	if(parseLine(line, res) < 0)
	{
	  cout << line[0] << endl;
	  cerr << "Error parsing this line :" << endl;
	  cerr << "  \"" << line << "\"" << endl;
	  return -1;
	}
  
  parsed_file.close();
  
  return 0;
}

int parseLine(string line, vector<Chunk *> &res)
{
  string free_space_start = "Empty space";
  string data_node_start = "         Inode";
  string dirent_node_start = "         Dirent";
  
  if (!line.compare(0, free_space_start.size(), free_space_start))
  {
    FreeSpaceChunk *fsc = new FreeSpaceChunk();
    if(fsc->build(line) < 0)
      return -1;
    res.push_back(fsc);
  }
  else if (!line.compare(0, data_node_start.size(), data_node_start))
  {
    DataNode *dn = new DataNode;
    if(dn->build(line) < 0)
      return -1;
    if(insertDataNodeInVector(dn, res))
      delete(dn);
  }
  else if (!line.compare(0, dirent_node_start.size(), dirent_node_start))
  {
    DirentNode *dn = new DirentNode;
    if(dn->build(line) < 0)
      return -1;
    res.push_back(dn);
  }
  else
  {
    cerr << "Error cant determine line type for :" << endl;
    cerr << "  \"" << line << "\"" << endl;
    return -1;
  }
  
  return 0;
}

/**
 * We may have multiple datanodes with the same version and ino. Is this
 * a jffs2dump bug ?
 * only keep one
 * Return 0 if the data node was inserted, 1 if it wasnt because a 
 * datanode with same version and number is already present
 */
int insertDataNodeInVector(DataNode *dn, vector<Chunk *> &vec)
{
  for(int i=0; i<(int)vec.size(); i++)
    if(vec[i]->getType() == DATA_NODE)
    {
      DataNode *dn2 = static_cast<DataNode *>(vec[i]);
      if(dn->getInodeNum() == dn2->getInodeNum() && dn->getVersionNum() == dn2->getVersionNum())
	return 1;
    }
  
  vec.push_back(dn);
  return 0;
}
