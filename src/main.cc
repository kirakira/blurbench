#include <iostream>
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

using namespace std;

void debug_output(const string &message)
{
    char time_string[256];
    time_t t = time(NULL);
    struct tm *tmp = localtime(&t);
    strftime(time_string, 256, "%a, %d %b %Y %T %z", tmp);
    cout << "[" << time_string << "] " << message << endl;
}

typedef struct sEngine
{
    int id;
    int read_fd, write_fd;
    int pid;
} Engine;

bool run(const char *engine, Engine *info)
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
        if (execl(engine, engine, NULL) == -1)
            return false;
    }

    close(piperead[1]);
    close(pipewrite[0]);

    info->read_fd = piperead[0];
    info->write_fd = pipewrite[1];
    info->pid = cpid;

    return true;
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        cerr << "Usage: ./blurbench engine1 engine2 threads" << endl;
        return 1;
    }

    char *engine[2] = {argv[1], argv[1]};

    int threads_count = atoi(argv[3]);

    cout << "===============================" << endl;
    debug_output(string(engine[0]) + " vs " + engine[1] + " (#T=" + argv[3] + ")");

    Engine *info = new Engine[threads_count * 2];
    for (int i = 0; i < 2 * threads_count; ++i)
    {
        info->id = (i % 2);
        if (!run(engine[info->id], &info[i]))
        {
            debug_output(string("Failed to run ") + engine[info->id]);
            exit(1);
        }
    }

    for (int i = 0; i < threads_count; ++i)
        waitpid(info->pid, NULL, 0);

    delete []info;

    return 0;
}
