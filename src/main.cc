#include <iostream>
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#include <unistd.h>

using namespace std;

void debug_output(const string &message)
{
    char time_string[256];
    time_t t = time(NULL);
    struct tm *tmp = localtime(&t);
    strftime(time_string, 256, "%a, %d %b %Y %T %z", tmp);
    cout << "[" << time_string << "] " << message << endl;
}

bool run(const char *engine, int *read_fd, int *write_fd, int *pid)
{
    int piperead[2], pipewrite[2];
    if (pipe(piperead) == -1)
    {
        debug_output("Create pipe failed 1");
        return false;
    }
    if (pipe(pipewrite) == -1)
    {
        close(piperead[0]);
        close(piperead[1]);
        debug_output("Create pipe failed 2");
        return false;
    }

    int cpid = fork();
    if (cpid == -1)
    {
        close(piperead[0]);
        close(piperead[1]);
        close(pipewrite[0]);
        close(pipewrite[1]);
        debug_output(string("Fork ") + engine + " failed");
        return false;
    }

    if (cpid == 0)
    {
        close(piperead[0]);
        close(pipewrite[1]);
        if (dup2(0, pipewrite[0]) == -1)
            return false;
        if (dup2(1, piperead[1]) == -1)
            return false;
        if (execl(engine, NULL, NULL) == -1)
            return false;
    }

    close(piperead[1]);
    close(pipewrite[0]);
    *pid = cpid;
    *read_fd = piperead[0];
    *write_fd = pipewrite[1];

    return true;
}

void match(const char *engine1, const char *engine2, int first)
{
    int fd1[2], fd2[2], pid1, pid2;
    if (!run(engine1, fd1, fd1 + 1, &pid1))
    {
        debug_output(string("Failed to run ") + engine1);
        return;
    }
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        cerr << "Usage: ./blurbench engine1 engine2 threads" << endl;
        return 1;
    }

    char *engine1 = argv[1], *engine2 = argv[1];

    int threads_count = atoi(argv[3]);

    cout << "===============================" << endl;
    debug_output(string(engine1) + " vs " + engine2 + " (#T=" + argv[3] + ")");

    match(engine1, engine2, 0);

    return 0;
}
