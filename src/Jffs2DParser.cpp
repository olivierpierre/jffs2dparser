#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <getopt.h>

#define NDEBUG

#include "Parser.hpp"
#include "ChunkModel.hpp"
#include "File.hpp"

using namespace std;

typedef enum {MODE_VIZ, MODE_CSV, MODE_FILEMAP} parser_mode_t;

typedef struct
{
  int flash_page_size;
  int pages_per_block;
  int partition_offset;
  parser_mode_t mode;
  char file_path[256];			// stdin if == "-"
} parser_config_t;

void print_help_and_exit(int argc, char **argv);
void print_all(vector<Chunk *> &res);
void set_default_options(parser_config_t &config);
void print_csv(vector<Chunk *> &res);
void print_filemap(vector<Chunk *> &res);
void print_config(parser_config_t &config);

int main(int argc, char **argv)
{
  parser_config_t config;
  vector<Chunk *> res;
  int c;
  
  // process options
  set_default_options(config);
  while ((c = getopt (argc, argv, "vcfp:b:")) != -1)
    switch (c)
    {
      case 'v':
	config.mode = MODE_VIZ;
	break;
      case 'c':
	config.mode = MODE_CSV;
	break;
      case 'f':
	config.mode = MODE_FILEMAP;
	break;
      case 'p':
	config.flash_page_size = atoi(optarg);
	break;
      case 'b':
	config.pages_per_block = atoi(optarg);
	break;
      case 'o':
	config.partition_offset = atoi(optarg);
	break;
      case 'h':
      default:
      print_help_and_exit(argc, argv);
    }
  if(argv[optind] != NULL)
    strcpy(config.file_path, argv[optind]);
  else
    print_help_and_exit(argc, argv);
  
  FlashAddr::init(config.flash_page_size, config.pages_per_block, config.partition_offset);
  
  if (!strcmp(config.file_path, "-"))
  {
    if (parseStdIn(res) < 0)
    {
      cerr << "Error parsing stdin" << endl;
      return EXIT_FAILURE;
    }
  }
  else
    if(parseFile(config.file_path, res) < 0)
    {
      cerr << "Error parsing " << argv[1] <<  endl;
      return EXIT_FAILURE;
    }
    
  print_config(config);
  if(config.mode == MODE_VIZ)
    print_all(res);
  else if(config.mode == MODE_CSV)
    print_csv(res);
  else if(config.mode == MODE_FILEMAP)
    print_filemap(res);
  else
  {
    cerr << "Invalid mode" << endl;
  }
  
  for(int i=0; i<(int)res.size(); i++)
    delete res[i];
    
  return EXIT_SUCCESS;
}

void print_help_and_exit(int argc, char **argv)
{
  cout << "Usage : " << argv[0] << " <input>" << endl;
  cout << "  <input> can be a file or '-' for std input" << endl;
  exit(-1);
}

void print_all(vector<Chunk *> &res)
{

  for(int i=0; i<(int)res.size(); i++)
  {
    switch(res[i]->getType())
    {
      case FREE_SPACE:
      {
	FreeSpaceChunk *fsc = static_cast<FreeSpaceChunk *>(res[i]);
	cout << *fsc << endl;
	break;
      }
	
      case DATA_NODE:
      {
	DataNode *dn = static_cast<DataNode *>(res[i]);
	cout << *dn << endl;
	break;
      }
      
      case DIRENT_NODE:
      {
	DirentNode *dn = static_cast<DirentNode *>(res[i]);
	cout << *dn << endl;
	break;
      }
	
      default:
	cerr << "Error unknown type ..." << endl;
    }
  }
}

void print_config(parser_config_t &config)
{
  cout << "/************************************/" << endl;
  cout << " JFFS2 dump parser configuration :" << endl;
  if(!strcmp(config.file_path, "-"))
    cout << " - Parsing StdIn" << endl;
  else
    cout << " - Parsing " << config.file_path << endl;
    
  switch(config.mode)
  {
    case MODE_CSV:
      cout << " - CSV mode" << endl;
      break;
    case MODE_FILEMAP:
      cout << " - Filemap mode" << endl;
      break;
    case MODE_VIZ:
      cout << " - Visualization mode" << endl;
      break;
    default:
      break;
  }
  
  cout << " - Flash page size : " << config.flash_page_size << endl;
  cout << " - Pages per block : " << config.pages_per_block << endl;
  cout << " - Partition offset : " << config.partition_offset << endl;
  
  cout << "/************************************/" << endl;
}

void print_csv(vector<Chunk *> &res)
{
  
}

void print_filemap(vector<Chunk *> &res)
{
  FileSet fs(res);

  cout << fs;
}

void set_default_options(parser_config_t &config)
{
  config.pages_per_block = 64;
  config.flash_page_size = 2048;
  config.mode = MODE_VIZ;
  strcpy(config.file_path, "");
  config.partition_offset = 7864320;		//TODO put 0 here
}
