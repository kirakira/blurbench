#include <iostream>
#include <sstream>
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>

#include "board.h"

using namespace std;

const char XBOARD_STRING[] = {"xboard\nprotover 2\n"};

void debug_output(const string &message)
{
    char time_string[256];
    time_t t = time(NULL);
    struct tm *tmp = localtime(&t);
    strftime(time_string, 256, "%a, %d %b %Y %T %z", tmp);
    cout << "[" << time_string << "] " << message << endl;
}

struct Game;

struct Engine
{
    int id;
    int read_fd, write_fd;
    int pid;
    Game *game;
};

struct Game
{
    bool first_engine_first;

    Engine *engines[2];
    Board board;
    int turn;
};

bool run(const char *engine, Engine *info)
{
    int piperead[2], pipewrite[2];
    if (pipe2(piperead, O_NONBLOCK) == -1)
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
        if (dup2(pipewrite[0], 0) == -1)
            return false;
        if (dup2(piperead[1], 1) == -1)
            return false;
        close(pipewrite[0]);
        close(piperead[1]);
        if (execl(engine, engine, NULL) == -1)
            return false;
    }

    close(piperead[1]);
    close(pipewrite[0]);

    info->read_fd = piperead[0];
    info->write_fd = pipewrite[1];
    info->pid = cpid;

    write(info->write_fd, XBOARD_STRING, sizeof(XBOARD_STRING));

    return true;
}

void handle_command(Engine *e, string command)
{
    debug_output(command);
}

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems)
{
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}


std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

bool handle_read(Engine *e)
{
    char c[10] = {0};
    string s;
    bool closed = false;
    while (true)
    {
        int ret = read(e->read_fd, c, 5);
        if (ret == O_NONBLOCK)
            break;
        if (ret == 0)
        {
            closed = true;
            break;
        }
        else if (ret == -1)
        {
            if (errno != EAGAIN)
                closed = true;
            break;
        }

        s += c;
        memset(c, 0, sizeof(c));
    }

    vector<string> commands = split(s, '\n');
    for (size_t i = 0; i < commands.size(); ++i)
        handle_command(e, commands[i]);

    if (closed)
        return true;

    return false;
}

bool io(vector<Engine *> engines, fd_set *fs_read)
{
    FD_ZERO(fs_read);
    int len = 0;
    for (size_t i = 0; i < engines.size(); ++i, ++len)
    {
        FD_SET(engines[i]->read_fd, fs_read);
        len = max(len, engines[i]->read_fd);
    }

    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    int ret = select(len + 1, fs_read, NULL, NULL, &timeout);

    if (ret == -1)
        return false;

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

    vector<Engine *> engines;
    vector<Game *> games;

    bool next_first_first = true;
    for (int i = 0; i < threads_count; ++i)
    {
        Game *game = new Game();
        game->first_engine_first = next_first_first;
        next_first_first = !next_first_first;
        game->turn = (game->first_engine_first ? 0 : 1);

        for (int j = 0; j < 2; ++j)
        {
            Engine *e = new Engine();
            game->engines[j] = e;
            e->id = j;
            e->game = game;
            if (!run(engine[j], e))
            {
                debug_output(string("Failed to run ") + engine[e->id]);
                exit(1);
            }
            engines.push_back(e);

        }

        games.push_back(game);
    }


    fd_set fs_read;
    while (true)
    {
        io(engines, &fs_read);
        for (size_t i = 0; i < engines.size(); ++i)
        {
            if (FD_ISSET(engines[i]->read_fd, &fs_read))
                handle_read(engines[i]);
        }
    }

    for (size_t i = 0; i < engines.size(); ++i)
        waitpid(engines[i]->pid, NULL, 0);

    for (size_t i = 0; i < games.size(); ++i)
    {
        delete games[i]->engines[0];
        delete games[i]->engines[1];
        delete games[i];
    }

    return 0;
}
