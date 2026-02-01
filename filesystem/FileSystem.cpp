#include "FileSystem.hpp"
#include "Serializer.hpp"
#include <iostream>
#include <set>
#include <cmath>
#include <cstring>
using namespace std;

FileSystem::FileSystem(BlockManager* blockManager) {
    bm = blockManager;
    root.reset(new Directory("root", nullptr, bm));
    currentDir = root.get();
}

void FileSystem::load() {
    Directory* loaded = Serializer::loadDirectory(*bm);
    if (loaded) {
        // Serializer returns a new tree with parent pointers set
        root.reset(loaded);
        currentDir = root.get();
    }
}

void FileSystem::save() {
    // Ensure index blocks are present for all files
    Serializer::saveDirectory(*bm, root.get());
}

bool FileSystem::mkdir(const std::string& name) {
    return currentDir->addSubdir(name);
}

bool FileSystem::removeDirectory(const std::string& name) {
    if (name.empty()) return false;
    // prevent removing root
    if (name == root->name) {
        cout << "[ERROR] Cannot remove root directory\n";
        return false;
    }
    Directory* target = currentDir->findSubdir(name);
    if (!target) {
        cout << "[ERROR] Directory not found: " << name << "\n";
        return false;
    }
    // prevent deleting current working dir or ancestor of it
    Directory* tmp = currentDir;
    while (tmp) {
        if (tmp == target) {
            cout << "[ERROR] Cannot remove current or parent directory\n";
            return false;
        }
        tmp = tmp->parent;
    }

    bool ok = currentDir->removeDirectory(name, *bm);
    if (!ok) cout << "[ERROR] Failed to remove directory: " << name << "\n";
    return ok;
}

bool FileSystem::chmodEntry(int mode, const std::string& name) {
    return currentDir->chmodEntry(name, mode);
}

bool FileSystem::checkMeta(bool repair) {
    int total = bm->getTotalBlocks();
    // gather referenced blocks from directory tree
    std::set<int> referenced;
    std::map<int, std::vector<std::string>> owners;
    std::function<void(Directory*)> walk = [&](Directory* d) {
        for (auto& p : d->files) {
            const FileMeta& fm = p.second;
            if (fm.indexBlock >= 0) referenced.insert(fm.indexBlock);
            owners[fm.indexBlock].push_back(d->name + "/" + fm.filename + " (index)");
            for (int b : fm.blocks) {
                referenced.insert(b);
                owners[b].push_back(d->name + "/" + fm.filename + " (data)");
            }
        }
        for (auto& sd : d->subdirs) walk(sd.get());
    };
    walk(root.get());

    std::vector<int> used, orphan, missing;
    std::vector<std::string> actions;
    for (int i = 0; i < total; ++i) {
        if (!bm->isBlockFree(i)) {
            used.push_back(i);
            if (i == 0) continue; // skip directory block (reserved)
            if (referenced.find(i) == referenced.end()) {
                // block used but not referenced: orphan
                orphan.push_back(i);
            }
        }
    }
    for (int r : referenced) {
        if (r < 0 || r >= total) continue;
        if (bm->isBlockFree(r)) missing.push_back(r);
    }

    cout << "fsck: total blocks=" << total << " used=" << used.size() << "\n";
    if (!orphan.empty()) {
        cout << "Orphaned blocks: \n";
        for (int b : orphan) {
            cout << "  + " << b;
            if (owners.find(b) != owners.end()) {
                cout << " referenced by: ";
                for (auto& s : owners[b]) cout << s << ", ";
            }
            // show a hex preview of first 16 bytes
            vector<char> buf; bm->readBlock(b, buf);
            int nonzero = 0; for (char c : buf) if (c != 0) nonzero++;
            cout << "  (nonzero bytes: " << nonzero << ")\n";
        }
        cout << "\n";
    } else cout << "No orphaned blocks found.\n";

    if (!missing.empty()) {
        cout << "Referenced but marked free: ";
        for (int b : missing) cout << b << " ";
        cout << "\n";
    } else cout << "No referenced-but-free blocks.\n";

    if (repair && !orphan.empty()) {
        for (int b : orphan) {
            bm->freeBlock(b);
            cout << "[fsck-repair] Freed block: " << b << "\n";
        }
    }
    // Fix referenced but free blocks by marking them used
    if (repair && !missing.empty()) {
        for (int b : missing) {
            cout << "[fsck-repair] Marking referenced-but-free block used: " << b << "\n";
            bm->markBlockUsed(b);
            actions.push_back("mark-used:" + to_string(b));
        }
    }

    // Now check per-file block list consistency and index block content
    int blockSize = bm->getBlockSize();
    std::function<void(Directory*)> repairWalk = [&](Directory* d) {
        for (auto &p : d->files) {
            FileMeta &fm = const_cast<FileMeta&>(p.second);
            // remove invalid block indices > total
            vector<int> validBlocks;
            for (int b : fm.blocks) {
                if (b >= 0 && b < total) validBlocks.push_back(b);
                else {
                    actions.push_back("remove-invalid-block:" + to_string(b) + " in " + d->name + "/" + fm.filename);
                    cout << "[fsck-repair] Removing invalid block index " << b << " from " << d->name << "/" << fm.filename << "\n";
                }
            }
            fm.blocks = validBlocks;

            int requiredBlocks = (fm.fileSize == 0) ? 0 : (int)ceil((double)fm.fileSize / blockSize);
            if ((int)fm.blocks.size() > requiredBlocks) {
                // free extra blocks
                for (int i = requiredBlocks; i < (int)fm.blocks.size(); ++i) {
                    int toFree = fm.blocks[i];
                    bm->freeBlock(toFree);
                    actions.push_back("freed-block:" + to_string(toFree) + " from " + d->name + "/" + fm.filename);
                    cout << "[fsck-repair] Freed extra block " << toFree << " from " << d->name << "/" << fm.filename << "\n";
                }
                fm.blocks.erase(fm.blocks.begin() + requiredBlocks, fm.blocks.end());
            } else if ((int)fm.blocks.size() < requiredBlocks) {
                // allocate missing blocks
                int need = requiredBlocks - (int)fm.blocks.size();
                for (int i = 0; i < need; ++i) {
                    int b = bm->allocateBlock();
                    if (b == -1) {
                        cout << "[fsck-repair] Not enough blocks to satisfy file size for " << d->name << "/" << fm.filename << "; shrinking file\n";
                        // adjust file size down
                        fm.fileSize = (int)fm.blocks.size() * blockSize;
                        break;
                    }
                    fm.blocks.push_back(b);
                    actions.push_back("alloc-block:" + to_string(b) + " for " + d->name + "/" + fm.filename);
                    cout << "[fsck-repair] Allocated block " << b << " for " << d->name << "/" << fm.filename << "\n";
                }
            }
            // ensure index block content is in sync
            if (fm.indexBlock >= 0) {
                // read index block and compare
                vector<char> ibuf;
                if (bm->readBlock(fm.indexBlock, ibuf)) {
                    // parse count and blocks
                    if ((int)ibuf.size() >= (int)sizeof(int)) {
                        int cnt;
                        memcpy(&cnt, ibuf.data(), sizeof(int));
                        vector<int> iblocks(cnt);
                        if (cnt > 0 && (int)ibuf.size() >= (int)(sizeof(int) + cnt * sizeof(int))) {
                            memcpy(iblocks.data(), ibuf.data() + sizeof(int), cnt * sizeof(int));
                        }
                        if (iblocks != fm.blocks) {
                            cout << "[fsck-repair] Index block mismatch in " << d->name << "/" << fm.filename << "; rewriting index block\n";
                            Serializer::writeIndexBlock(*bm, fm);
                            actions.push_back("rewrite-index:" + to_string(fm.indexBlock) + " for " + d->name + "/" + fm.filename);
                        }
                    }
                } else {
                    cout << "[fsck-repair] Failed to read index block " << fm.indexBlock << " for " << d->name << "/" << fm.filename << "\n";
                }
            } else if (fm.fileSize > 0 && fm.blocks.size() > 0) {
                // if there's file content but no indexBlock, allocate an index block
                int idx = bm->allocateBlock();
                if (idx != -1) {
                    fm.indexBlock = idx;
                    Serializer::writeIndexBlock(*bm, fm);
                    actions.push_back("create-index:" + to_string(idx) + " for " + d->name + "/" + fm.filename);
                    cout << "[fsck-repair] Created missing index block " << idx << " for " << d->name << "/" << fm.filename << "\n";
                }
            }
        }
        for (auto &sd : d->subdirs) repairWalk(sd.get());
    };
    repairWalk(root.get());

    // Persist any changes made to the tree
    if (repair) root->saveDirectory();

    if (!actions.empty()) cout << "fsck: actions taken: \n";
    for (auto &a : actions) cout << "  - " << a << "\n";
    return true;
}

bool FileSystem::cd(const std::string& name) {
    if (name == "..") {
        if (currentDir->parent) {
            currentDir = currentDir->parent;
            return true;
        }
        return false;
    }
    Directory* d = currentDir->findSubdir(name);
    if (!d) return false;
    // require execute permission on the target directory
    if ((d->permissions & 1) == 0) {
        cout << "[ERROR] Permission denied: cannot enter directory\n";
        return false;
    }
    currentDir = d;
    return true;
}

void FileSystem::ls() {
    currentDir->listContents();
}

string FileSystem::pwd() {
    string path;
    Directory* cur = currentDir;
    while (cur) {
        path = "/" + cur->name + path;
        cur = cur->parent;
    }
    return path;
}

bool FileSystem::createFile(const std::string& filename, int size) { return currentDir->createFile(filename, size); }
bool FileSystem::deleteFile(const std::string& filename) { return currentDir->deleteFile(filename); }
bool FileSystem::writeFile(const std::string& filename, const std::string& content) { return currentDir->writeFile(filename, content); }
string FileSystem::readFile(const std::string& filename) { return currentDir->readFile(filename); }
void FileSystem::listFiles() { currentDir->listFiles(); }
bool FileSystem::appendFile(const std::string& filename, const std::string& data) { return currentDir->appendFile(filename, data); }
bool FileSystem::resizeFile(const std::string& filename, int newSize) { return currentDir->resizeFile(filename, newSize); }
void FileSystem::infoFile(const std::string& filename) { currentDir->infoFile(filename); }
