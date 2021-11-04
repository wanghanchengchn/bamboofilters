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
#include "bamboofilter/bitsutil.h"

#include "common/random.h"
#include "common/timing.h"

#define loop(x, a, b) for (uint64_t x = a; x < b; ++x)

using namespace std;

int main(int argc, char *argv[])
{
    size_t add_count = 200000 * 7;

    cout << "Prepare..." << endl;

    vector<string> to_add, to_lookup;
    GenerateRandom64(add_count, to_add, to_lookup);

    cout << "Begin test" << endl;

    for (auto exp_idx = 1; exp_idx <= 7; exp_idx++)
    {
        auto add_count = exp_idx * 200000;

        BambooFilter *bbf = new BambooFilter(upperpower2(200000), 2);
        for (uint64_t added = 0; added < add_count; added++)
        {
            bbf->Insert(to_add[added].c_str());
        }

        auto start_time = NowNanos();
        for (uint64_t added = 0; added < add_count; added++)
        {
            bbf->Lookup(to_add[added].c_str());
        }
        cout << ((add_count * 1000.0) / static_cast<double>(NowNanos() - start_time)) << endl;
    }

    return 0;
}