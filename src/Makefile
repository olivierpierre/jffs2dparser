all: .depends Jffs2DParser

SRC=ChunkModel.cpp  File.cpp  FlashAddr.cpp  Jffs2DParser.cpp  Parser.cpp

Jffs2DParser: $(SRC)
	$(CXX) $(CFLAGS) $^ -o $@
  
clean:
	rm -rf *.o Jffs2DParser
  
depends: .depends
.depends:
	$(CXX) -MM $(SRC) > .depends

-include .depends
