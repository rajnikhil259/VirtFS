#ifndef FILESYSTEM_HPP
#define FILESYSTEM_HPP

#include "BlockManager.hpp"
#include "Directory.hpp"
#include <memory>
#include <string>

class FileSystem {
public:
    BlockManager* bm;
    std::unique_ptr<Directory> root;
    Directory* currentDir;

    FileSystem(BlockManager* blockManager);
    void load();
    void save();

    // Directory commands
    bool mkdir(const std::string& name);
    bool cd(const std::string& name);
    void ls();
    std::string pwd();
    bool removeDirectory(const std::string& name);
    bool chmodEntry(int mode, const std::string& name);
    bool checkMeta(bool repair);

    // File commands delegate to currentDir
    bool createFile(const std::string& filename, int size);
    bool deleteFile(const std::string& filename);
    bool writeFile(const std::string& filename, const std::string& content);
    std::string readFile(const std::string& filename);
    void listFiles();
    bool appendFile(const std::string& filename, const std::string& data);
    bool resizeFile(const std::string& filename, int newSize);
    void infoFile(const std::string& filename);
};

#endif
