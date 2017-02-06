#ifndef PARSER_HPP
#define PARSER_HPP

#include <vector>
#include <fstream>

#include "ChunkModel.hpp" 

using namespace std;

int parseStdIn(vector<Chunk *> &res);
int parseFile(char *path, vector<Chunk *> &res);
int parseLine(string line, vector<Chunk *> &res);

#endif /* PARSER_HPP */
