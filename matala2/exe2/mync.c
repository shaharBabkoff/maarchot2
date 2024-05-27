#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

void printErrorAndExit(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc != 3 || strcmp(argv[1], "-e") != 0) {
        fprintf(stderr, "Usage: %s -e <command>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *command = argv[2];
    char *args[4];
    args[0] = "sh";
    args[1] = "-c";
    args[2] = command;
    args[3] = NULL;

    pid_t pid = fork(); // יוצרים תהליך ילד
    if (pid == -1) {
        // קריאה ל-fork נכשלה
        printErrorAndExit("fork");
    } else if (pid == 0) {
        // זהו הקוד שמתבצע בתהליך הילד
        if (execvp(args[0], args) == -1) {
            printErrorAndExit("execvp");
        }
    } else {
        // זהו הקוד שמתבצע בתהליך האב
        if (wait(NULL) == -1) {
            printErrorAndExit("wait");
        }
    }

    return 0;
}
