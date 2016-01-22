#ifndef RANDOMNESS_H__
#define RANDOMNESS_H__

#include <random>

// Mersenne twister engine is used for random numbers here because they should
// create the same numbers on different systems, and because they are fast. Be
// sure to pass the engine by reference or pointer because there is mutable
// state. The _64 stands for 64 bits of randomness. This is necessary because
// with only 32 bits we might start repeating operations.
typedef std::mt19937_64 RNG;

#endif // RANDOMNESS_H__
