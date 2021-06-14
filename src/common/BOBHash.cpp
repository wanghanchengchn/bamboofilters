#include "BOBHash.h"

#define mix(a, b, c)    \
    {                   \
        a -= b;         \
        a -= c;         \
        a ^= (c >> 13); \
        b -= c;         \
        b -= a;         \
        b ^= (a << 8);  \
        c -= a;         \
        c -= b;         \
        c ^= (b >> 13); \
        a -= b;         \
        a -= c;         \
        a ^= (c >> 12); \
        b -= c;         \
        b -= a;         \
        b ^= (a << 16); \
        c -= a;         \
        c -= b;         \
        c ^= (b >> 5);  \
        a -= b;         \
        a -= c;         \
        a ^= (c >> 3);  \
        b -= c;         \
        b -= a;         \
        b ^= (a << 10); \
        c -= a;         \
        c -= b;         \
        c ^= (b >> 15); \
    }

BOBHash::BOBHash() {
    this->primeNum = 0;
}

BOBHash::BOBHash(uint32_t primeNum) {
    this->primeNum = primeNum;
}

void BOBHash::initialize(uint32_t primeNum) {
    this->primeNum = primeNum;
}

uint32_t BOBHash::run(const void* buf, uint32_t len) {
    return BOBHash::run(buf, len, this->primeNum);
}

uint32_t BOBHash::run(const void* buf, uint32_t len, uint32_t primeNum) {
    const char* str = (const char*)buf;
    //register ub4 a,b,c,len;
    uint32_t a, b, c;
    /* Set up the internal state */
    //len = length;
    a = b = 0x9e3779b9; /* the golden ratio; an arbitrary value */

    c = primeNum; /* the previous hash value */

    /*---------------------------------------- handle most of the key */
    while (len >= 12) {
        a += (str[0] + ((uint32_t)str[1] << 8) + ((uint32_t)str[2] << 16) +
              ((uint32_t)str[3] << 24));
        b += (str[4] + ((uint32_t)str[5] << 8) + ((uint32_t)str[6] << 16) +
              ((uint32_t)str[7] << 24));
        c += (str[8] + ((uint32_t)str[9] << 8) + ((uint32_t)str[10] << 16) +
              ((uint32_t)str[11] << 24));
        mix(a, b, c);
        str += 12;
        len -= 12;
    }

    /*------------------------------------- handle the last 11 bytes */
    c += len;
    switch (len) /* all the case statements fall through */
    {
    case 11:
        c += ((uint32_t)str[10] << 24);
    case 10:
        c += ((uint32_t)str[9] << 16);
    case 9:
        c += ((uint32_t)str[8] << 8);
        /* the first byte of c is reserved for the length */
    case 8:
        b += ((uint32_t)str[7] << 24);
    case 7:
        b += ((uint32_t)str[6] << 16);
    case 6:
        b += ((uint32_t)str[5] << 8);
    case 5:
        b += str[4];
    case 4:
        a += ((uint32_t)str[3] << 24);
    case 3:
        a += ((uint32_t)str[2] << 16);
    case 2:
        a += ((uint32_t)str[1] << 8);
    case 1:
        a += str[0];
        /* case 0: nothing left to add */
    }
    mix(a, b, c);
    /*-------------------------------------------- report the result */
    return c;
}

BOBHash::~BOBHash() {}
