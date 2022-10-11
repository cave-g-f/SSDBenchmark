// test.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <thread>
#include <vector>
#include <fstream>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 5)
    {
        std::cout << "Usage: test.exe inputLog output parttern threadNumber" << std::endl;
        return 0;
    }

    std::string inFile(argv[1]), outFile(argv[2]), p(argv[3]);
    std::uint64_t threads = atoi(argv[4]);

    std::ifstream f1(inFile);
    std::ofstream f2(outFile);


    std::string a;
    int cnt = 0;
    int threadCnt = 0;
    while(getline(f1, a))
    {
        bool flag = true;
        for (int i = 0; i < p.size(); i++)
        {
            if (a[i] != p[i])
            {
                flag = false;
                break;
            }
        }

        if (flag) cnt++;

        if (flag && (cnt % 3 == 1))
        {
            threadCnt++;
            if((threadCnt % threads == 0))
            f2 << a.substr(p.size() + 2, a.size() - (p.size() + 2)) << std::endl;
        }
    }


    return 0;
}
