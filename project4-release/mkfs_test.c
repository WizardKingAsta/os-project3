#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#define DISKFILE_PATH "os-project4/project4-release/DISKFILE"

#define TESTDIR "/tmp/mc2432/mountdir"

int main() {
    // Remove existing diskfile to simulate first-time setup
    unlink(DISKFILE_PATH);

    // Attempt to mount the filesystem
    int mount_result = system("./rufs -s " TESTDIR);

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
