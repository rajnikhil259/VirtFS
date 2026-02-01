#ifndef SERIALIZER_HPP
#define SERIALIZER_HPP

#include "filemeta.hpp"
#include "blockmanager.hpp"
#include "directory.hpp"
#include <map>
#include <vector>
#include <istream>
#include <ostream>

class Serializer {
public:
    // Backwards-compatible (not used) signature
    static void saveDirectory(BlockManager& bm, std::map<std::string, FileMeta>& files);
    static void loadDirectory(BlockManager& bm, std::map<std::string, FileMeta>& files);

    // New recursive directory tree serialization
    static void saveDirectory(BlockManager& bm, Directory* dir);
    static Directory* loadDirectory(BlockManager& bm);

    static void writeIndexBlock(BlockManager& bm, FileMeta& fm);
};

#endif
