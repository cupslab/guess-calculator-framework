// big_count.h - use native math until we can't anymore
// only handle integer multiplication and addition

#ifndef BIG_COUNT_H__
#include <gmp.h>
#include <cstdint>

class BigCount {
  public:
    static void mul(BigCount& dest, const BigCount& op1, const uint64_t op2);
    static void add(BigCount& dest, const BigCount& op1, const uint64_t op2);
    static int cmp(const BigCount& op1, const BigCount& op2);
    static void get(mpz_t dest, const BigCount& src);
    BigCount();
    BigCount(const uint64_t init);
    BigCount(const mpz_t init);
    ~BigCount();

  private:
    mpz_t mpval;
    uint64_t nativeval;
    bool usemp;

    void initmp();
};
#define BIG_COUNT_H__
#endif
