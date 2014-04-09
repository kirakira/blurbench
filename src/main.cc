#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <map>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>

#include "board.h"

using namespace std;

const char XBOARD_STRING[] = {"xboard\nprotover 2\n"},
      NEW_GO[] = {"new\nwhite\ngo\n"},
      NEW_FORCE[] = {"new\nwhite\nforce\n"},
      FORCE[] = {"force\n"},
      GO[] = {"go\n"},
      QUIT[] = {"quit\n"},
      MAIN_LOG[] = {"main.log"};

enum Result {
    ENGINE0_FIRST_WIN = 0,
    ENGINE0_SECOND_WIN,
    ENGINE0_FIRST_DRAW,
    ENGINE0_SECOND_DRAW,
    ENGINE0_FIRST_LOSE,
    ENGINE0_SECOND_LOSE
};
const int WIN = 0, DRAW = 1, LOSE = 2;

void debug_output(const string &file, const string &message, bool print = true, bool timestamp = true)
{
    ofstream fout;
    fout.open(file.c_str(), fstream::out | fstream::app);
    char time_string[256];
    time_t t = time(NULL);
    struct tm *tmp = localtime(&t);
    strftime(time_string, 256, "%a, %d %b %Y %T %z", tmp);

    if (!timestamp)
        fout << message << endl;
    else
        fout << "[" << time_string << "] " << message << endl;
    fout.close();

    if (print)
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
    string log_file, history_file;
    map<uint64_t, int> repetition;

    Engine *engines[2];
    Board board;
    int turn;
};

int results[6] = {0};

vector<Engine *> engines;
vector<Game *> games;
bool next_first_first = true;
char *engine_path[2];

bool run(const char *engine, Engine *info)
{
    int piperead[2], pipewrite[2];
    if (pipe2(piperead, O_NONBLOCK) == -1)
    {
        debug_output(MAIN_LOG, "Create pipe failed 1");
        return false;
    }

    if (pipe(pipewrite) == -1)
    {
        close(piperead[0]);
        close(piperead[1]);
        debug_output(MAIN_LOG, "Create pipe failed 2");
        return false;
    }

    int cpid = fork();
    if (cpid == -1)
    {
        close(piperead[0]);
        close(piperead[1]);
        close(pipewrite[0]);
        close(pipewrite[1]);
        debug_output(MAIN_LOG, string("Fork ") + engine + " failed");
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

    write(info->write_fd, XBOARD_STRING, strlen(XBOARD_STRING));

    return true;
}

vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems)
{
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim))
        elems.push_back(item);
    return elems;
}

std::vector<std::string> split(const std::string &s, char delim)
{
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

void output_stats()
{
    ostringstream oss;
    int tot = 0;
    for (int i = 0; i < 6; ++i)
    {
        oss << results[i] << " ";
        tot += results[i];
    }

    debug_output(MAIN_LOG, "Overall result: " + oss.str());
    ostringstream oss2;

    double wr0 = (double) (results[ENGINE0_FIRST_WIN] + results[ENGINE0_SECOND_WIN]) / (double) tot;
    double wr1 = (double) (results[ENGINE0_FIRST_LOSE] + results[ENGINE0_SECOND_LOSE]) / (double) tot;
    oss2 << "Engine0 win rate: " << wr0 * 100 << "%, engine1 win rate: " << wr1 * 100 << "%";
    debug_output(MAIN_LOG, oss2.str());
}

void end_game(Game *game, int id, int r)
{
    Result result;
    if (id != 0)
        r = 2 - r;
    if (game->first_engine_first)
    {
        if (r == WIN)
            result = ENGINE0_FIRST_WIN;
        else if (r == DRAW)
            result = ENGINE0_FIRST_DRAW;
        else
            result = ENGINE0_FIRST_LOSE;
    }
    else
    {
        if (r == WIN)
            result = ENGINE0_SECOND_WIN;
        else if (r == DRAW)
            result = ENGINE0_SECOND_DRAW;
        else
            result = ENGINE0_SECOND_LOSE;
    }
    ++results[(int) result];
    debug_output(MAIN_LOG, string("Game ended with result ") + (char) ('0' + result));
    output_stats();

    write(game->engines[0]->write_fd, QUIT, strlen(QUIT));
    write(game->engines[1]->write_fd, QUIT, strlen(QUIT));
    waitpid(game->engines[0]->pid, NULL, 0);
    waitpid(game->engines[1]->pid, NULL, 0);
    engines.erase(find(engines.begin(), engines.end(), game->engines[0]));
    engines.erase(find(engines.begin(), engines.end(), game->engines[1]));
    games.erase(find(games.begin(), games.end(), game));
    delete game->engines[0];
    delete game->engines[1];
    delete game;
}

string generate_random(int len)
{
    string s;
    for (int i = 0; i < len; ++i)
        s += ('a' + rand() % 26);
    return s + ".log";
}

void start_new_game()
{
    Game *game = new Game();
    game->first_engine_first = next_first_first;
    next_first_first = !next_first_first;
    game->turn = (game->first_engine_first ? 0 : 1);
    game->log_file = generate_random(10);
    game->history_file = generate_random(10);

    for (int j = 0; j < 2; ++j)
    {
        Engine *e = new Engine();
        game->engines[j] = e;
        e->id = j;
        e->game = game;
        if (!run(engine_path[j], e))
        {
            debug_output(MAIN_LOG, string("Failed to run ") + engine_path[e->id]);
            exit(1);
        }
        engines.push_back(e);
    }

    games.push_back(game);

    debug_output(MAIN_LOG, "New game started, log file: " + game->log_file + ", history file: " + game->history_file);

    ostringstream oss;
    oss << "First engine: " << engine_path[0] << ", second engine: " << engine_path[1] << ", first_engine_first: " << game->first_engine_first;
    debug_output(game->log_file, oss.str(), false);
    debug_output(game->log_file, "History file: " + game->history_file, false);
}

void handle_command(Engine *e, string command)
{
    ostringstream oss;
    oss << "[" << e->id << "] ";
    debug_output(e->game->log_file, oss.str() + command, false);

    vector<string> tokens = split(command, ' ');
    if (tokens.size() > 0 && tokens[0] == "feature")
    {
        if ((e->id == 0 && e->game->first_engine_first)
                || (e->id == 1 && !e->game->first_engine_first))
            write(e->write_fd, NEW_GO, strlen(NEW_GO));
        else
            write(e->write_fd, NEW_FORCE, strlen(NEW_FORCE));
    }
    else if (tokens.size() >= 2 && tokens[0] == "move")
    {
        debug_output(e->game->history_file, tokens[1], false, false);

        if (!e->game->board.checked_move(e->game->first_engine_first ? 1 - e->game->turn : e->game->turn, make_move(tokens[1]), NULL))
        {
            debug_output(e->game->log_file, "Illegal move made");
            end_game(e->game, e->id, LOSE);
            start_new_game();
        }
        else
        {
            e->game->turn = 1 - e->game->turn;
            uint64_t hash = e->game->board.hash_code(e->game->turn);
            int rep = 0;
            if (e->game->repetition.find(hash) != e->game->repetition.end())
                rep = e->game->repetition[hash];
            e->game->repetition[hash] = rep + 1;

            if (rep >= 3)
            {
                end_game(e->game, e->id, DRAW);
                start_new_game();
            }
            else
            {
                Engine *other = e->game->engines[1 - e->id];
                write(other->write_fd, FORCE, strlen(FORCE));

                string move = tokens[1] + "\n";
                write(other->write_fd, move.c_str(), move.length());

                write(other->write_fd, GO, strlen(GO));
            }
        }
    }
    else if (tokens.size() > 0 && (tokens[0] == "1-0" || tokens[0] == "0-1") && command.find("resigns") != string::npos)
    {
        end_game(e->game, e->id, LOSE);
        start_new_game();
    }
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

    srand(time(0));

    engine_path[0] = argv[1];
    engine_path[1] = argv[2];

    int threads_count = atoi(argv[3]);

    cout << "===============================" << endl;
    debug_output(MAIN_LOG, "============================");
    debug_output(MAIN_LOG, string(engine_path[0]) + " vs " + engine_path[1] + " (#T=" + argv[3] + ")");

    for (int i = 0; i < threads_count; ++i)
        start_new_game();


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
