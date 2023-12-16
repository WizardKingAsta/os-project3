# RU File System using FUSE Report

### Authors
- Trevor Dovan - td441
- Manaav Choudhary - mc2432

## Implementation Details

## Benchmark Results

## Additional Notes
- Compiled and tested on ilab3.cs.rutgers.edu and kill.cs.rutgers.edu
- Unfortunately we were unable to fully implement everything due to poor timing, and the scale of this project.

## Difficulties & Issues
- **Memory Management**: We faced challenges in efficiently managing memory, especially while handling larger files and directories.
- Every function required many user-space allocations with malloc, as well as keeping the memory of the DISKFILE up to date and properly managed.
- **Concurrency**: Ensuring thread safety and handling concurrent file operations was a complex aspect of the project.  
- **Debugging**: Debugging was particularly challenging due to the complexity of file system operations and interactions between various components.
- Understanding how the blocks and reading and writing interacted with the DISKFILe was conceptually complex and also added a layer of difficulty to debugging.
- Having to mount and unmount in different situations as well as debug and run was difficult as it felt like there were many moving pieces that had to be considered for every test.
- It was also a foreign idea to mount and debug in one terminal and use another to cd to the file system and use commands.

## Collaboration and References
This project was a collaborative effort among group members Maanav Choudhary and Trevor Dovan.

- Consulted various online forums and documentation for FUSE:
- https://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial/
- https://www.cs.hmc.edu/~geoff/classes/hmc.cs135.201109/homework/fuse/fuse_doc.html
- https://libfuse.github.io/doxygen/index.html
- Utilized Stack Overflow for troubleshooting specific implementation issues.


