#include "Directory.hpp"
#include "Serializer.hpp"
#include <iostream>
#include <cmath>
#include <cstring>
#include <ctime>
using namespace std;

// Helper: convert numeric permission to 'rwx' string
static string permToStr(int perm, bool isDir) {
    string s;
    s += (isDir ? 'd' : '-');
    s += (perm & 4) ? 'r' : '-';
    s += (perm & 2) ? 'w' : '-';
    s += (perm & 1) ? 'x' : '-';
    return s;
}

Directory::Directory(const string& name_, Directory* parent_, BlockManager* blockManager) {
    name = name_;
    parent = parent_;
    bm = blockManager;
    permissions = 7; // default to rwx for directories
}

bool Directory::createFile(const string& filename, int size) {
    // require write permission on this directory
    if ((permissions & 2) == 0) {
        cout << "[ERROR] Permission denied: cannot create file in this directory\n";
        return false;
    }
    if (files.find(filename) != files.end()) {
        cout << "[ERROR] File already exists!\n";
        return false;
    }

    int blockSize = bm->getBlockSize();
    int numBlocks = (int)ceil((double)size / blockSize);

    int idxBlock = bm->allocateBlock();
    if (idxBlock == -1) {
        cout << "[ERROR] No free blocks for index block.\n";
        return false;
    }

    FileMeta fm;
    fm.filename = filename;
    fm.fileSize = size;
    fm.indexBlock = idxBlock;
    fm.permissions = 6; // default file permissions: rw-

    for (int i = 0; i < numBlocks; i++) {
        int b = bm->allocateBlock();
        if (b == -1) {
            cout << "[ERROR] Not enough free blocks, rolling back...\n";
            for (int blk : fm.blocks) bm->freeBlock(blk);
            bm->freeBlock(idxBlock);
            return false;
        }
        fm.blocks.push_back(b);
    }

    Serializer::writeIndexBlock(*bm, fm);
    files[filename] = fm;
    saveDirectory();  // Auto-save directory after create
    cout << "[INFO] File created: " << filename << "\n";
    return true;
}

bool Directory::deleteFile(const std::string& filename) {
    // must have write permission on directory to delete file
    if ((permissions & 2) == 0) {
        cout << "[ERROR] Permission denied: cannot delete file in this directory\n";
        return false;
    }
    auto it = files.find(filename);
    if (it == files.end()) return false;

    FileMeta& fm = it->second;

    // Free data blocks
    for (int blk : fm.blocks) {
        bm->freeBlock(blk);
    }

    // Free index block
    bm->freeBlock(fm.indexBlock);

    // Remove from directory
    files.erase(it);

    saveDirectory();
    cout << "[INFO] File deleted: " << filename << endl;
    return true;
}


FileMeta Directory::getFile(const string& filename) {
    auto it = files.find(filename);
    if (it == files.end()) return FileMeta();  // Return default FileMeta if not found
    return it->second;  // Return by value
}

bool Directory::hasFile(const string& filename) {
    return files.find(filename) != files.end();
}

void Directory::listFiles() {
    if ((permissions & 4) == 0) {
        cout << "[ERROR] Permission denied: cannot list files in this directory\n";
        return;
    }
    if (files.empty()) {
        cout << "(empty directory)\n";
        return;
    }
    for (auto& pair : files) {
        cout << pair.first << " (" << pair.second.fileSize << " bytes)\n";
    }
}

Directory* Directory::findSubdir(const string& name) {
    for (auto& d : subdirs) {
        if (d->name == name) return d.get();
    }
    return nullptr;
}

bool Directory::addSubdir(const string& name) {
    if ((permissions & 2) == 0) {
        cout << "[ERROR] Permission denied: cannot create subdirectory\n";
        return false;
    }
    if (findSubdir(name) != nullptr) return false;
    subdirs.emplace_back(new Directory(name, this, bm));
    saveDirectory();
    cout << "[INFO] Directory created: " << name << "\n";
    return true;
}

bool Directory::removeSubdir(const string& name) {
    if ((permissions & 2) == 0) {
        cout << "[ERROR] Permission denied: cannot remove subdirectory\n";
        return false;
    }
    for (size_t i = 0; i < subdirs.size(); ++i) {
        if (subdirs[i]->name == name) {
            // only allow removal if empty
            if (!subdirs[i]->files.empty() || !subdirs[i]->subdirs.empty()) {
                cout << "[ERROR] Directory not empty: " << name << "\n";
                return false;
            }
            subdirs.erase(subdirs.begin() + i);
            saveDirectory();
            cout << "[INFO] Directory removed: " << name << "\n";
            return true;
        }
    }
    return false;
}

// Recursive helper to free blocks and delete contents
static void removeDirectoryRecursive(Directory* dir, BlockManager& bm) {
    // Free files
    for (auto& p : dir->files) {
        FileMeta& fm = const_cast<FileMeta&>(p.second);
        // free data blocks
        for (int blk : fm.blocks) bm.freeBlock(blk);
        // free index block
        if (fm.indexBlock != -1) bm.freeBlock(fm.indexBlock);
    }
    dir->files.clear();

    // Recurse into subdirs
    for (auto& sd : dir->subdirs) {
        removeDirectoryRecursive(sd.get(), bm);
    }
    // clear subdirs vector (unique_ptr destructors will run)
    dir->subdirs.clear();
}

bool Directory::removeDirectory(const string& name, BlockManager& bm) {
    if ((permissions & 2) == 0) {
        cout << "[ERROR] Permission denied: cannot remove directory\n";
        return false;
    }
    Directory* target = findSubdir(name);
    if (!target) return false;

    // Perform recursive deletion
    removeDirectoryRecursive(target, bm);

    // remove entry from subdirs
    for (size_t i = 0; i < subdirs.size(); ++i) {
        if (subdirs[i]->name == name) {
            subdirs.erase(subdirs.begin() + i);
            break;
        }
    }

    // persist changes
    saveDirectory();
    cout << "[INFO] Directory recursively removed: " << name << "\n";
    return true;
}

void Directory::listContents() {
    if ((permissions & 4) == 0) {
        cout << "[ERROR] Permission denied: cannot list directory contents\n";
        return;
    }
    // List subdirectories first
    for (auto& d : subdirs) {
        cout << permToStr(d->permissions, true) << "  " << "-" << "  " << d->name << "/\n";
    }
    // Then files
    for (auto& p : files) {
        cout << permToStr(p.second.permissions, false) << "  " << p.second.fileSize << "B  " << p.first << "\n";
    }
}

bool Directory::writeFile(const string& filename, const string& content) {
    if (!hasFile(filename)) {
        cout << "[ERROR] File not found!\n";
        return false;
    }
    FileMeta& fm = files[filename];  // Get reference directly from map
    if ((fm.permissions & 2) == 0) {
        cout << "[ERROR] Permission denied: cannot write file\n";
        return false;
    }

    int blockSize = bm->getBlockSize();
    int bytesLeft = content.size();
    int offset = 0;

    for (int blk : fm.blocks) {
        vector<char> buffer(blockSize, 0);
        int toWrite = min(bytesLeft, blockSize);
        if (offset < (int)content.size()) {
            int canCopy = min(toWrite, (int)content.size() - offset);
            memcpy(buffer.data(), content.data() + offset, canCopy);
        }
        bm->writeBlock(blk, buffer);

        offset += toWrite;
        bytesLeft -= toWrite;
        if (bytesLeft <= 0) break;
    }

    fm.fileSize = content.size();
    fm.modifiedAt = time(nullptr);  // Update modification time
    Serializer::writeIndexBlock(*bm, fm);
    saveDirectory();  // Persist updated file metadata
    cout << "[INFO] Wrote " << content.size() << " bytes to " << filename << "\n";
    return true;
}

string Directory::readFile(const string& filename) {
    if (!hasFile(filename)) {
        cout << "[ERROR] File not found!\n";
        return "";
    }
    FileMeta& fm = files[filename];  // Get reference directly from map
    if ((fm.permissions & 4) == 0) {
        cout << "[ERROR] Permission denied: cannot read file\n";
        return "";
    }

    int blockSize = bm->getBlockSize();
    string result;
    int bytesLeft = fm.fileSize;

    for (int blk : fm.blocks) {
        vector<char> buffer;
        bm->readBlock(blk, buffer);
        int toRead = min(bytesLeft, (int)buffer.size());
        if (toRead > 0) {
            result.append(buffer.begin(), buffer.begin() + toRead);
        }
        bytesLeft -= toRead;
        if (bytesLeft <= 0) break;
    }

    return result;
}

// Helper function to format timestamp
static string formatTimestamp(long timestamp) {
    if (timestamp == 0) return "Not set";
    time_t t = (time_t)timestamp;
    struct tm* timeinfo = localtime(&t);
    char buffer[100];
    strftime(buffer, sizeof(buffer), "%d-%m-%Y %H:%M:%S", timeinfo);
    return string(buffer);
}

void Directory::infoFile(const string& filename) {
    if (!hasFile(filename)) {
        cout << "[ERROR] File not found!\n";
        return;
    }
    FileMeta fm = getFile(filename);
    cout << "\n=== File Information ===\n";
    cout << "Name:             " << fm.filename << "\n";
    cout << "Size:             " << fm.fileSize << " bytes\n";
    cout << "Index Block:      " << fm.indexBlock << "\n";
    cout << "Data Blocks:      ";
    for (int i = 0; i < (int)fm.blocks.size(); i++) {
        if (i > 0) cout << ", ";
        cout << fm.blocks[i];
    }
    cout << "\n";
    cout << "Created:          " << formatTimestamp(fm.createdAt) << "\n";
    cout << "Modified:         " << formatTimestamp(fm.modifiedAt) << "\n";
    cout << "Permissions:      " << permToStr(fm.permissions, false) << " (" << fm.permissions << ")\n";
    cout << "========================\n\n";
}

bool Directory::appendFile(const string& filename, const string& data) {
    if (!hasFile(filename)) {
        cout << "[ERROR] File not found!\n";
        return false;
    }
    
    if (data.empty()) {
        cout << "[ERROR] Cannot append empty data!\n";
        return false;
    }
    
    FileMeta& fm = files[filename];
    if ((fm.permissions & 2) == 0) {
        cout << "[ERROR] Permission denied: cannot append file\n";
        return false;
    }
    int blockSize = bm->getBlockSize();
    int currentSize = fm.fileSize;
    int newSize = currentSize + data.size();
    int currentBlocks = (int)ceil((double)currentSize / blockSize);
    int requiredBlocks = (int)ceil((double)newSize / blockSize);
    int additionalBlocks = requiredBlocks - currentBlocks;
    
    // Allocate additional blocks if needed
    vector<int> newBlocks;
    if (additionalBlocks > 0) {
        for (int i = 0; i < additionalBlocks; i++) {
            int b = bm->allocateBlock();
            if (b == -1) {
                cout << "[ERROR] Not enough free blocks for append operation!\n";
                // Free any blocks we allocated
                for (int blk : newBlocks) bm->freeBlock(blk);
                return false;
            }
            newBlocks.push_back(b);
            fm.blocks.push_back(b);
        }
    }
    
    // Write data to the file
    int bytesWritten = 0;
    int dataOffset = 0;
    int blockIndex = currentBlocks - 1;  // Start from last existing block
    
    // Find the offset within the last existing block
    int offsetInLastBlock = currentSize % blockSize;
    if (offsetInLastBlock == 0 && currentSize > 0) {
        blockIndex++;
        offsetInLastBlock = 0;
    }
    
    while (dataOffset < (int)data.size()) {
        vector<char> buffer(blockSize, 0);
        
        // Read existing block if we're appending to a partially filled block
        if (blockIndex < currentBlocks && offsetInLastBlock > 0) {
            bm->readBlock(fm.blocks[blockIndex], buffer);
        }
        
        int bytesToWrite = min((int)data.size() - dataOffset, blockSize - offsetInLastBlock);
        memcpy(buffer.data() + offsetInLastBlock, data.data() + dataOffset, bytesToWrite);
        
        bm->writeBlock(fm.blocks[blockIndex], buffer);
        
        dataOffset += bytesToWrite;
        bytesWritten += bytesToWrite;
        offsetInLastBlock = 0;
        blockIndex++;
    }
    
    fm.fileSize = newSize;
    fm.modifiedAt = time(nullptr);
    Serializer::writeIndexBlock(*bm, fm);
    saveDirectory();
    cout << "[INFO] Appended " << data.size() << " bytes to " << filename 
         << " (total size: " << newSize << " bytes)\n";
    return true;
}

bool Directory::resizeFile(const string& filename, int newSize) {
    if (!hasFile(filename)) {
        cout << "[ERROR] File not found!\n";
        return false;
    }
    FileMeta& fm = files[filename];
    if ((fm.permissions & 2) == 0) {
        cout << "[ERROR] Permission denied: cannot resize file\n";
        return false;
    }
    
    if (newSize < 0) {
        cout << "[ERROR] Invalid size (must be >= 0)!\n";
        return false;
    }
    
    int blockSize = bm->getBlockSize();
    int currentSize = fm.fileSize;
    
    if (newSize == currentSize) {
        cout << "[INFO] File size unchanged.\n";
        return true;
    }
    
    if (newSize > currentSize) {
        // EXPAND: Allocate additional blocks and zero-fill
        int currentBlocks = (int)ceil((double)currentSize / blockSize);
        int requiredBlocks = (int)ceil((double)newSize / blockSize);
        int additionalBlocks = requiredBlocks - currentBlocks;
        
        // Allocate new blocks
        for (int i = 0; i < additionalBlocks; i++) {
            int b = bm->allocateBlock();
            if (b == -1) {
                cout << "[ERROR] Not enough free blocks to expand file!\n";
                // Free any blocks we allocated in this operation
                int blocksToFree = i;
                for (int j = 0; j < blocksToFree; j++) {
                    bm->freeBlock(fm.blocks[currentBlocks + j]);
                }
                fm.blocks.erase(fm.blocks.begin() + currentBlocks, fm.blocks.end());
                return false;
            }
            fm.blocks.push_back(b);
        }
        
        // Zero-fill the last block if necessary
        int offsetInLastBlock = newSize % blockSize;
        if (offsetInLastBlock != 0) {
            vector<char> buffer(blockSize, 0);
            int lastBlockIdx = fm.blocks.size() - 1;
            // If this isn't the first time we're writing to this block, read it first
            if (currentSize % blockSize != 0 && 
                lastBlockIdx == (int)ceil((double)currentSize / blockSize) - 1) {
                bm->readBlock(fm.blocks[lastBlockIdx], buffer);
            }
            bm->writeBlock(fm.blocks[lastBlockIdx], buffer);
        }
        
        fm.fileSize = newSize;
        fm.modifiedAt = time(nullptr);
        Serializer::writeIndexBlock(*bm, fm);
        saveDirectory();
        cout << "[INFO] File expanded to " << newSize << " bytes.\n";
        return true;
        
    } else {
        // SHRINK: Free blocks beyond the new size
        int currentBlocks = (int)ceil((double)currentSize / blockSize);
        int requiredBlocks = (int)ceil((double)newSize / blockSize);
        
        if (newSize == 0) {
            // Free all data blocks
            for (int blk : fm.blocks) {
                bm->freeBlock(blk);
            }
            fm.blocks.clear();
        } else {
            // Free only the blocks we don't need anymore
            for (int i = requiredBlocks; i < currentBlocks; i++) {
                bm->freeBlock(fm.blocks[i]);
            }
            fm.blocks.erase(fm.blocks.begin() + requiredBlocks, fm.blocks.end());
            
            // Truncate the last block if necessary
            int offsetInLastBlock = newSize % blockSize;
            if (offsetInLastBlock != 0) {
                vector<char> buffer(blockSize, 0);
                bm->readBlock(fm.blocks[requiredBlocks - 1], buffer);
                // Zero-fill the rest of the block after newSize
                for (int i = offsetInLastBlock; i < blockSize; i++) {
                    buffer[i] = 0;
                }
                bm->writeBlock(fm.blocks[requiredBlocks - 1], buffer);
            }
        }
        
        fm.fileSize = newSize;
        fm.modifiedAt = time(nullptr);
        Serializer::writeIndexBlock(*bm, fm);
        Serializer::saveDirectory(*bm, files);
        cout << "[INFO] File shrunk to " << newSize << " bytes.\n";
        return true;
    }
}

void Directory::saveDirectory() {
    // Always persist the entire tree starting from the root directory.
    Directory* top = this;
    while (top->parent) top = top->parent;
    Serializer::saveDirectory(*bm, top);
}

bool Directory::chmodEntry(const std::string& name, int mode) {
    // Change permission of subdir
    Directory* sd = findSubdir(name);
    if (sd) {
        sd->permissions = mode & 7;
        saveDirectory();
        cout << "[INFO] Directory permissions updated: " << name << " -> " << sd->permissions << "\n";
        return true;
    }
    // Change permission of file
    auto it = files.find(name);
    if (it != files.end()) {
        it->second.permissions = mode & 7;
        saveDirectory();
        cout << "[INFO] File permissions updated: " << name << " -> " << it->second.permissions << "\n";
        return true;
    }
    cout << "[ERROR] Entry not found: " << name << "\n";
    return false;
}

void Directory::loadDirectory() {
    // Loading handled at FileSystem level via Serializer::loadDirectory
}
