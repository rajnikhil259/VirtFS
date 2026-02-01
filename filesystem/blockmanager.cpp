#include "blockmanager.hpp"
#include <fstream>
#include <iostream>
using namespace std;

BlockManager::BlockManager(
    const string &diskPath,
    const string &metaPath,
    int blockSize,
    int totalBlocks
) : diskPath(diskPath), metaPath(metaPath),
    blockSize(blockSize), totalBlocks(totalBlocks)
{
    freeBlockBitmap.resize(totalBlocks, true);
    freeBlockBitmap[0] = false;  // Block 0 is reserved for directory listing
}

void BlockManager::init() {
    // If meta file exists → load bitmap
    ifstream meta(metaPath, ios::binary);
    if (meta.good()) {
        // Verify meta file is the expected size; if not, reinitialize
        meta.seekg(0, ios::end);
        auto msize = meta.tellg();
        meta.close();
        if (msize < totalBlocks) {
            // Corrupt or incomplete metadata — recreate it
            saveMeta();
            cout << "[WARN] Metadata file incomplete — reinitialized.\n";
        } else {
            loadMeta();
            cout << "[INFO] Metadata loaded.\n";
        }
    } else {
        // Create new metadata
        saveMeta();
        cout << "[INFO] Metadata initialized.\n";
    }

    // Ensure the disk file exists and has the expected size.
    // Opening with ios::in|ios::out won't create the file on its own,
    // so create/resize it if missing or too small.
    fstream disk(diskPath, ios::in | ios::out | ios::binary);
    if (!disk.good()) {
        // Create a new disk file of size blockSize * totalBlocks
        ofstream dcreate(diskPath, ios::binary);
        // Seek to the final byte and write a single zero to allocate space
        long long fullSize = (long long)blockSize * (long long)totalBlocks;
        if (fullSize > 0) {
            dcreate.seekp(fullSize - 1);
            char zero = 0;
            dcreate.write(&zero, 1);
        }
        dcreate.close();
        cout << "[INFO] Disk file created/resized.\n";
    } else {
        // Optionally, ensure disk is at least the expected size
        disk.seekg(0, ios::end);
        auto size = disk.tellg();
        long long expected = (long long)blockSize * (long long)totalBlocks;
        disk.close();
        if (size < expected) {
            // Expand the file to the expected size
            ofstream dcreate(diskPath, ios::binary | ios::app);
            long long need = expected - size;
            const int chunk = 4096;
            vector<char> zeros(min<long long>(need, chunk), 0);
            while (need > 0) {
                int toWrite = (int)min<long long>(need, zeros.size());
                dcreate.write(zeros.data(), toWrite);
                need -= toWrite;
            }
            dcreate.close();
            cout << "[INFO] Disk file expanded to expected size.\n";
        }
    }
}

void BlockManager::loadMeta() {
    ifstream meta(metaPath, ios::binary);

    for (int i = 0; i < totalBlocks; i++) {
        char bit;
        meta.read(&bit, 1);
        freeBlockBitmap[i] = (bit == '1');
    }

    meta.close();
}

// restoreMeta removed — resting on manual recovery tools if needed

void BlockManager::saveMeta() {
    ofstream meta(metaPath, ios::binary);

    for (bool bit : freeBlockBitmap) {
        char c = bit ? '1' : '0';
        meta.write(&c, 1);
    }

    meta.close();
}

bool BlockManager::readBlock(int index, vector<char> &buffer) {
    if (index < 0 || index >= totalBlocks) return false;

    ifstream disk(diskPath, ios::binary);
    if (!disk.good()) return false;

    buffer.resize(blockSize);
    disk.seekg(index * blockSize);
    disk.read(buffer.data(), blockSize);

    disk.close();
    return true;
}

bool BlockManager::writeBlock(int index, const vector<char> &buffer) {
    if (index < 0 || index >= totalBlocks) return false;

    fstream disk(diskPath, ios::binary | ios::in | ios::out);
    if (!disk.good()) return false;

    disk.seekp(index * blockSize);
    disk.write(buffer.data(), blockSize);
    disk.flush();

    disk.close();
    return true;
}

int BlockManager::allocateBlock() {
    for (int i = 1; i < totalBlocks; i++) {  // Start from block 1, reserve block 0 for directory
        if (freeBlockBitmap[i]) {
            freeBlockBitmap[i] = false;
            saveMeta();
            return i;
        }
    }
    return -1; // no free block
}

void BlockManager::freeBlock(int index) {
    if (index < 0 || index >= totalBlocks) return;
    freeBlockBitmap[index] = true;
    saveMeta();
}

void BlockManager::markBlockUsed(int index) {
    if (index < 0 || index >= totalBlocks) return;
    freeBlockBitmap[index] = false;
    saveMeta();
}

bool BlockManager::isBlockFree(int index) {
    return freeBlockBitmap[index];
}

int BlockManager::getBlockSize() const {
    return blockSize;
}

int BlockManager::getTotalBlocks() const {
    return totalBlocks;
}
