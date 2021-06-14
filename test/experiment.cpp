#include <string>
#include <cmath>
#include <iostream>

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <random>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <openssl/rand.h>

#include "bamboofilter/bamboofilter.hpp"

#include "common/random.h"
#include "common/timing.h"

using namespace std;

int main(int argc, char *argv[])
{
    size_t add_count = 65536;

    cout << "Prepare..." << endl;

    vector<string> to_add, to_lookup;
    GenerateRandom64(add_count, to_add, to_lookup);

    cout << "Begin test" << endl;

    BambooFilter<12> *l = new BambooFilter<12>(add_count);

    auto start_time = NowNanos();

    for (uint64_t added = 0; added < add_count; added++)
    {
        l->Insert(to_add[added].c_str());
    }

    cout << ((add_count * 1000.0) / static_cast<double>(NowNanos() - start_time)) << endl;

    start_time = NowNanos();
    for (uint64_t added = 0; added < add_count; added++)
    {
        if (!l->Lookup(to_add[added].c_str()))
        {
            throw logic_error("False Negative");
        }
    }
    cout << ((add_count * 1000.0) / static_cast<double>(NowNanos() - start_time)) << endl;

    return 0;
}