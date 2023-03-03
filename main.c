#include "strack.h"

int main(int argc, char **args) {
    char buf[100];
    int i;
    init();
    if(argc <= 1) {
        for(;;) {
            inputBuf(buf, 100);
            runLine(buf);
            printf("\n");
        }
    } else {
        for(i = 1; i < argc; i++) runFile(args[i]);
    }
    return 0;
}
