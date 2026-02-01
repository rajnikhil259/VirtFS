#ifndef DIRECTORY_HPP
#define DIRECTORY_HPP

#include "FileMeta.hpp"
#include "BlockManager.hpp"
#include <map>
#include <string>
#include <vector>
#include <memory>

class Directory {
public:
    std::string name;
    Directory* parent;
    std::map<std::string, FileMeta> files;
    std::vector<std::unique_ptr<Directory>> subdirs;
    BlockManager* bm;
    int permissions; // Unix-style permissions for the directory (0-7)

    Directory(const std::string& name_, Directory* parent_, BlockManager* blockManager);

    Directory* findSubdir(const std::string& name);
    bool addSubdir(const std::string& name);
    bool removeSubdir(const std::string& name); // remove only if empty
    bool removeDirectory(const std::string& name, BlockManager& bm); // recursive delete
    void listContents();
    bool chmodEntry(const std::string& name, int mode);

    // File operations (operate within this directory)
    bool createFile(const std::string& filename, int size);
    bool deleteFile(const std::string& filename);
    void listFiles();
    FileMeta getFile(const std::string& filename);  // Return by value to avoid dangling pointers
    bool hasFile(const std::string& filename);
    bool writeFile(const std::string& filename, const std::string& content);
    std::string readFile(const std::string& filename);
    void infoFile(const std::string& filename);
    bool appendFile(const std::string& filename, const std::string& data);
    bool resizeFile(const std::string& filename, int newSize);

    // Persistence helpers will call Serializer directly
    void saveDirectory();
    void loadDirectory();
};

#endif
