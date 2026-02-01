#ifndef BLOCK_MANAGER_HPP
#define BLOCK_MANAGER_HPP

#include <string>
#include <vector>

class BlockManager {
private:
    std::string diskPath;
    std::string metaPath;

    int blockSize;
    int totalBlocks;

    std::vector<bool> freeBlockBitmap;

    void loadMeta();

public:
    BlockManager(
        const std::string &diskPath,
        const std::string &metaPath,
        int blockSize,
        int totalBlocks
    );

    void init();                   // Create disk if missing
    int allocateBlock();           // Returns block index
    void freeBlock(int index);     // Marks block free
    void markBlockUsed(int index); // Mark block as used without allocation
    bool readBlock(int index, std::vector<char>& buffer);
    bool writeBlock(int index, const std::vector<char>& buffer);
    bool isBlockFree(int index);
    void saveMeta();               // Save bitmap to meta.bin
    // Accessors
    int getBlockSize() const;
    int getTotalBlocks() const;
};

#endif
