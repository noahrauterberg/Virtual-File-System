# Virtual File System

---

In this homework, I implemented a parts of a virtual file system based on modified inodes.

A new file system can be created by calling`./build/ha2 -c <name> <size_in_KiB>`
and an existing file system can be accessed by calling`./build/ha2 -l <name>`.

The file system supports the following commands where every path is absolute:

* `mkdir <path>` - creates a new directory
* `mkfile <path>` - creates a new file
* `list <path>` - lists all files and directories of a given path
* `writef <path> <text>` - write given text to file
* `readf <path>` - read a file
* `rm <path>` - remove a file or directory from the file system
* `export <virtual_path> <external_path>` - export a file from the virtual file system to the normal file system
* `import <external_path> <virtual_path>` - import a file from the normal file system into the virtual file system
* `dump` - save a virtual file system

---

All of these commands were implemented by me.