#include <iostream>

using namespace std;


int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        cerr << "Usage: ./blurbench engine1 engine2 threads" << endl;
    }
    return 0;
}
