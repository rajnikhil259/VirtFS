#include "filesystem/BlockManager.hpp"
#include "filesystem/FileSystem.hpp"
#include <iostream>
#include <sstream>
using namespace std;

int main() {
    BlockManager bm("disc/virtualdisc.bin", "disc/meta.bin", 512, 100);
    bm.init();

    // FileSystem manages the directory tree and current working directory
    FileSystem fs(&bm);
    fs.load();

    cout << "=== File System Emulator CLI ===\n";
    cout << "Commands: create, write, read, delete, list, info, append, resize, mkdir, cd, pwd, ls, chmod, diskview, fsck, rmdir, exit\n";

    string line;
    while (true) {
        cout << "fs> ";
        getline(cin, line);
        stringstream ss(line);
        string cmd;
        ss >> cmd;

        if (cmd == "exit") break;

        else if (cmd == "create") {
            string filename;
            int size;
            ss >> filename >> size;
            if (filename.empty() || size <= 0) {
                cout << "[ERROR] Usage: create filename size\n";
                continue;
            }
            fs.createFile(filename, size);
        }

        else if (cmd == "write") {
            string filename;
            ss >> filename;
            string content;
            getline(ss, content); // rest of line
            if (filename.empty() || content.empty()) {
                cout << "[ERROR] Usage: write filename \"content\"\n";
                continue;
            }
            // remove leading space from content
            if (content[0] == ' ') content = content.substr(1);
            fs.writeFile(filename, content);
        }

        else if (cmd == "read") {
            string filename;
            ss >> filename;
            if (filename.empty()) {
                cout << "[ERROR] Usage: read filename\n";
                continue;
            }
            string content = fs.readFile(filename);
            cout << content << endl;
        }

        else if (cmd == "delete") {
            string filename;
            ss >> filename;
            if (filename.empty()) {
                cout << "[ERROR] Usage: delete filename\n";
                continue;
            }
            fs.deleteFile(filename);
        }

        else if (cmd == "list") {
            fs.listFiles();
        }

        else if (cmd == "info") {
            string filename;
            ss >> filename;
            if (filename.empty()) {
                cout << "[ERROR] Usage: info filename\n";
                continue;
            }
            fs.infoFile(filename);
        }

        else if (cmd == "append") {
            string filename;
            ss >> filename;
            string data;
            getline(ss, data); 
            if (filename.empty() || data.empty()) {
                cout << "[ERROR] Usage: append filename \"data\"\n";
                continue;
            }
            // remove leading space from data
            if (data[0] == ' ') data = data.substr(1);
            fs.appendFile(filename, data);
        }

        else if (cmd == "resize") {
            string filename;
            int newSize;
            ss >> filename >> newSize;
            if (filename.empty() || newSize < 0) {
                cout << "[ERROR] Usage: resize filename newsize\n";
                continue;
            }
            fs.resizeFile(filename, newSize);
        }

        else if (cmd == "mkdir") {
            string name; ss >> name;
            if (name.empty()) { cout << "[ERROR] Usage: mkdir name\n"; continue; }
            fs.mkdir(name);
        }

        else if (cmd == "cd") {
            string name; ss >> name;
            if (name.empty()) { cout << "[ERROR] Usage: cd name\n"; continue; }
            if (!fs.cd(name)) cout << "[ERROR] Directory not found or cannot move up\n";
            else {
                fs.ls();
            }
        }

        else if (cmd == "pwd") {
            cout << fs.pwd() << "\n";
        }

        else if (cmd == "ls") {
            fs.ls();
        }

        else if (cmd == "diskview") {
            // Print the metadata block (block 0) which stores directory tree
            std::vector<char> buf;
            if (!bm.readBlock(0, buf)) { cout << "[ERROR] Failed to read disk block 0\n"; continue; }
            // Find last non-zero char to avoid printing trailing garbage
            int last = (int)buf.size() - 1;
            while (last >= 0 && buf[last] == 0) --last;
            if (last < 0) { cout << "[diskview] (empty)\n"; continue; }
            string s(buf.begin(), buf.begin() + last + 1);
            cout << "[diskview]\n" << s << "\n";
        }

        else if (cmd == "rmdir") {
            string name; ss >> name;
            if (name.empty()) { cout << "[ERROR] Usage: rmdir name\n"; continue; }
            fs.removeDirectory(name);
        }

        else if (cmd == "fsck") {
            string arg; ss >> arg;
            bool repair = false;
            if (arg == "repair") repair = true;
            fs.checkMeta(repair);
        }

        // (restoremeta removed)

        else if (cmd == "chmod") {
            int mode; string name; ss >> mode >> name;
            if (name.empty()) { cout << "[ERROR] Usage: chmod <mode> <name>\n"; continue; }
            if (mode < 0 || mode > 7) { cout << "[ERROR] Mode must be in 0-7\n"; continue; }
            if (!fs.chmodEntry(mode, name)) {
                cout << "[ERROR] chmod failed\n";
            }
        }

        else {
            cout << "[ERROR] Unknown command\n";
        }
    }

    fs.save();
    bm.saveMeta();
    cout << "Exiting File System Emulator.\n";
    return 0;
}
