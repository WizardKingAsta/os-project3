#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "./rufs.h"

#define DISKFILE_PATH "os-project4/project4-release/DISKFILE"

#define TESTDIR "/tmp/mc2432/mountdir"

int main() {
    char buffer[256];  // Assuming a reasonable maximum path length
    if (getcwd(buffer, sizeof(buffer)) != NULL)
    {
        printf("Current working directory: %s\n", buffer);
    }
    else
    {
        perror("getcwd");
    }

    // Remove existing diskfile to simulate first-time setup
    unlink(DISKFILE_PATH);

    // Attempt to mount the filesystem
    //int mount_result = system("./rufs -s " TESTDIR);

    // Prepare arguments for run_rufs
    char *args[] = {"rufs", "-s", TESTDIR, NULL};
    int arg_count = sizeof(args) / sizeof(args[0]) - 1; // Subtract 1 for NULL

    // Call run_rufs with the arguments
    int mount_result = run_rufs(arg_count, args);

    if (mount_result != 0) {
        printf("Failed to mount filesystem.\n");
        return 1;
    }

    // Check if the diskfile is created
    if (access(DISKFILE_PATH, F_OK) != -1) {
        printf("Diskfile created successfully.\n");
    } else {
        printf("Diskfile not found.\n");
        return 1;
    }

    // Optional: Perform additional basic file operations here

    // Unmount the filesystem
    system("fusermount -u " TESTDIR);

    printf("Test completed.\n");
    return 0;
}
