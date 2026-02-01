# 🗄️ Filesystem Simulator (C++)

An educational **block-based filesystem emulator** implemented in **C++ (C++11)**.  
This project simulates the internal working of a filesystem by managing files, directories, and metadata on a virtual disk using binary files.

The simulator provides an interactive command-line interface (CLI) to perform common filesystem operations.

---

## 📂 Project Structure

- **filesystem/**
  - filesystem.cpp
  - filesystem.hpp
  - blockmanager.cpp
  - blockmanager.hpp
  - directory.cpp
  - directory.hpp
  - serializer.cpp
  - serializer.hpp
  - filemeta.hpp

- **disc/** *(created at runtime)*
  - virtualdisc.bin
  - meta.bin

- main.cpp  
- .gitignore  
- README.md  

> **Note:**  
> The `disc/` directory and `.bin` files are generated automatically at runtime and are **not committed** to the repository.

---

## ⚙️ Features

### 1. File Management
- Create, read, write, append, resize, and delete files
- View file metadata such as size, permissions, and allocated blocks

### 2. Directory Management
- Create and remove directories
- Navigate directory hierarchy (`cd`, `pwd`)
- List directory contents (`ls`)

### 3. Metadata & Storage
- Block-based virtual disk simulation
- Bitmap-based block allocation
- Binary serialization of filesystem metadata
- Persistent filesystem state across runs

### 4. Debug & Maintenance
- View raw metadata (`diskview`)
- Filesystem consistency check (`fsck`)
- Optional repair mode for inconsistencies

---

## 🖥️ CLI Commands

Type commands at the `fs>` prompt.

### File Commands
- `create <filename> <size>`
- `write <filename> "content"`
- `read <filename>`
- `append <filename> "data"`
- `resize <filename> <newsize>`
- `delete <filename>`
- `info <filename>`

### Directory Commands
- `mkdir <name>`
- `rmdir <name>`
- `cd <name>` / `cd ..`
- `ls`
- `pwd`

### Permissions
- `chmod <mode> <name>` *(mode range: 0–7)*

### Debug / Maintenance
- `diskview`
- `fsck [repair]`
- `exit`

---

## 🚀 Build & Run

### Compile
Requires `g++` with C++11 support.

```bash
g++ -std=c++11 -O2 main.cpp filesystem/*.cpp -I. -o fs_emulator

## 🛠️ Tech Stack
- Programming Language: C++ (C++11)
- Core Concepts: Filesystem Design, Block Allocation, Metadata Management
- Storage: Binary file-based virtual disk
- Interface: Command-Line Interface (CLI)
- Compiler / Tools: GCC (g++)

## 👨‍💻 Author
- Developed by [NIKHIL RAJ] 
- 🎯 IIIT Manipur | B.Tech CSE