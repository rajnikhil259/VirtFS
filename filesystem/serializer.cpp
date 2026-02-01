#include "serializer.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <ctime>
#include <cstdio>
#include <iomanip>

using namespace std;

// Helper function to format timestamp to human-readable string
static string formatTimestampToString(long timestamp) {
    if (timestamp == 0) return "Not-set";
    time_t t = (time_t)timestamp;
    struct tm* timeinfo = localtime(&t);
    char buffer[30];
    strftime(buffer, sizeof(buffer), "%d-%m-%Y_%H:%M:%S", timeinfo);
    return string(buffer);
}

// Helper function to parse human-readable timestamp back to long
static long parseTimestampFromString(const string& timestampStr) {
    if (timestampStr == "Not-set") return 0;
    
    // Parse format: "dd-mm-yyyy_HH:MM:SS"
    int day, month, year, hour, minute, second;
    int parsed = sscanf(timestampStr.c_str(), "%d-%d-%d_%d:%d:%d", 
                        &day, &month, &year, &hour, &minute, &second);
    
    if (parsed != 6) return time(nullptr);
    
    struct tm timeinfo = {};
    timeinfo.tm_mday = day;
    timeinfo.tm_mon = month - 1;  // tm_mon is 0-11
    timeinfo.tm_year = year - 1900;  // tm_year is years since 1900
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = second;
    timeinfo.tm_isdst = -1;  // Let mktime determine DST
    
    return (long)mktime(&timeinfo);
}

// Write index block for a file
void Serializer::writeIndexBlock(BlockManager& bm, FileMeta& fm) {
    vector<char> buffer(bm.getBlockSize(), 0);
    int count = (int)fm.blocks.size();
    memcpy(buffer.data(), &count, sizeof(int));
    memcpy(buffer.data() + sizeof(int), fm.blocks.data(), count * sizeof(int));
    bm.writeBlock(fm.indexBlock, buffer);
}

// Save directory to meta.bin (simple serialization)
void Serializer::saveDirectory(BlockManager& bm, map<string, FileMeta>& files) {
    // Maintain backwards compatibility: flatten map format
    vector<char> buffer(bm.getBlockSize(), 0);
    stringstream ss;

    for (auto& pair : files) {
        FileMeta& fm = pair.second;
            ss << fm.filename << " " << fm.fileSize << " " << fm.indexBlock << " ";
            for (int blk : fm.blocks) ss << blk << " ";
            ss << "| " << formatTimestampToString(fm.createdAt) << " " 
               << formatTimestampToString(fm.modifiedAt);
            // append optional permission token for backward compatibility
            ss << " perm " << fm.permissions << "\n";
    }

    string data = ss.str();
    for (size_t i = 0; i < data.size() && i < buffer.size(); i++)
        buffer[i] = data[i];

    bm.writeBlock(0, buffer); // store directory at block 0
}

// Save a Directory tree recursively
void Serializer::saveDirectory(BlockManager& bm, Directory* dir) {
    stringstream ss;

    // Recursive lambda
    function<void(Directory*, int)> writeDir = [&](Directory* d, int indent) {
            ss << string(indent, ' ') << "DIR " << d->name << " perm " << d->permissions << "\n";
        for (auto& sd : d->subdirs) writeDir(sd.get(), indent + 2);
        for (auto& p : d->files) {
            FileMeta& fm = p.second;
            ss << string(indent + 2, ' ') << "FILE " << fm.filename << " " << fm.fileSize << " " << fm.indexBlock << " ";
            for (int blk : fm.blocks) ss << blk << " ";
                ss << "| " << formatTimestampToString(fm.createdAt) << " " << formatTimestampToString(fm.modifiedAt);
                ss << " perm " << fm.permissions << "\n";
            // ensure index block on disk matches fm.blocks
            writeIndexBlock(bm, fm);
        }
        ss << string(indent, ' ') << "END_DIR\n";
    };

    writeDir(dir, 0);

    string data = ss.str();
    vector<char> buffer(bm.getBlockSize(), 0);
    for (size_t i = 0; i < data.size() && i < buffer.size(); i++) buffer[i] = data[i];
    bm.writeBlock(0, buffer);
}

// Load directory from meta.bin
Directory* Serializer::loadDirectory(BlockManager& bm) {
    vector<char> buffer;
    if (!bm.readBlock(0, buffer)) return nullptr;

    // Check if block 0 is all zeros (uninitialized disk)
    bool allZeros = true;
    for (char c : buffer) {
        if (c != 0) {
            allZeros = false;
            break;
        }
    }
    if (allZeros) return nullptr;

    // Find the end of actual data (before padding nulls)
    size_t dataEnd = 0;
    for (size_t i = 0; i < buffer.size(); i++) {
        if (buffer[i] != 0) dataEnd = i + 1;
    }

    string data(buffer.begin(), buffer.begin() + dataEnd);
    stringstream ss(data);

    // Parser stack
    string line;
    Directory* root = nullptr;
    vector<Directory*> stack;

    while (getline(ss, line)) {
        if (line.empty()) continue;
        // trim leading spaces
        size_t pos = line.find_first_not_of(' ');
        string trimmed = (pos==string::npos) ? string() : line.substr(pos);
        stringstream ls(trimmed);
        string token;
        ls >> token;
        if (token == "DIR") {
            string name; ls >> name;
            int perm = 7;
            string maybe;
            if (ls >> maybe) {
                if (maybe == "perm") {
                    ls >> perm;
                }
                // otherwise ignore unknown token
            }
            Directory* parent = stack.empty() ? nullptr : stack.back();
            Directory* dir = new Directory(name, parent, &bm);
            dir->permissions = perm;
            if (parent) parent->subdirs.emplace_back(dir);
            else root = dir;
            stack.push_back(dir);
        } else if (token == "END_DIR") {
            if (!stack.empty()) stack.pop_back();
        } else if (token == "FILE") {
            FileMeta fm;
            ls >> fm.filename >> fm.fileSize >> fm.indexBlock;
            string tk;
            while (ls >> tk) {
                if (tk == "|") break;
                fm.blocks.push_back(stoi(tk));
            }
            string createdStr, modifiedStr;
            if (ls >> createdStr >> modifiedStr) {
                fm.createdAt = parseTimestampFromString(createdStr);
                fm.modifiedAt = parseTimestampFromString(modifiedStr);
            } else {
                fm.createdAt = time(nullptr);
                fm.modifiedAt = fm.createdAt;
            }
                // try to parse optional permission token
                string extra;
                if (ls >> extra) {
                    if (extra == "perm") {
                        int p; ls >> p; fm.permissions = p & 7;
                    } else {
                        // unknown token, ignore
                    }
                } else {
                    fm.permissions = 6; // default file perm if not present
                }
            if (!stack.empty()) {
                Directory* cur = stack.back();
                cur->files[fm.filename] = fm;
                // ensure index block contents are on disk (write index block
                // based on fm.blocks)
                writeIndexBlock(bm, cur->files[fm.filename]);
            }
        }
    }

    return root;
}
