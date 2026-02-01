#ifndef FILE_META_HPP
#define FILE_META_HPP

#include <string>
#include <vector>
#include <ctime>

struct FileMeta {
    std::string filename;    // File name
    int fileSize;            // Size in bytes
    int indexBlock;          // Block number storing the index
    std::vector<int> blocks; // List of data blocks (filled via index block)
    long createdAt;          // Creation timestamp
    long modifiedAt;         // Last modification timestamp
    int permissions;         // Unix-style permission bits (0-7)

    FileMeta() : fileSize(0), indexBlock(-1), createdAt(0), modifiedAt(0), permissions(6) {
        createdAt = time(nullptr);
        modifiedAt = createdAt;
    }
};

#endif
