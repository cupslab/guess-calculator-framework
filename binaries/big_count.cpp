// big_count.cpp - use native math until we can't anymore
// we only deal with positive increases, and optimize comparison
// based on forcing initial values to native if possible

#include "big_count.h"
#include <limits>
#include <assert.h>

bool mul_overflow(const uint64_t op1, const uint64_t op2, uint64_t* dest) {
  // gcc5 has __builtin_mul_overflow
  if (op2 == 0) {
    *dest = 0;
    return false;
  }

  if (op1 > std::numeric_limits<uint64_t>::max() / op2) {
    return true;
  } else {
    *dest = op1 * op2;
    return false;
  }
}

bool add_overflow(const uint64_t op1, const uint64_t op2, uint64_t *dest) {
  // gcc5 has __builtin_add_overflow
  if (op1 > std::numeric_limits<uint64_t>::max() - op2) {
    return true;
  } else {
    *dest = op1 + op2;
    return false;
  }
}

void BigCount::mul(BigCount& dest, const BigCount& op1, const uint64_t op2) {
  if (op1.usemp) {
    dest.initmp();
    mpz_mul_ui(dest.mpval, op1.mpval, op2);
    return;
  } else {
    uint64_t result;
    bool overflow = mul_overflow(op1.nativeval, op2, &result);
// print
    if (overflow) {
      BigCount temp(op1);
      temp.initmp();
      dest.initmp();
      mpz_set_ui(temp.mpval, temp.nativeval);
      mpz_mul_ui(dest.mpval, temp.mpval, op2);
      return;
    }
    dest.nativeval = result;
    return;
  }
}

void BigCount::add(BigCount& dest, const BigCount& op1, const uint64_t op2) {
  if (op1.usemp) {
    dest.initmp();
    mpz_add_ui(dest.mpval, op1.mpval, op2);
    return;
  } else {
    uint64_t result;
    bool overflow = add_overflow(op1.nativeval, op2, &result);
// print
    if (overflow) {
      BigCount temp(op1);
      temp.initmp();
      dest.initmp();
      mpz_set_ui(temp.mpval, temp.nativeval);
      mpz_add_ui(dest.mpval, temp.mpval, op2);
      return;
    }
    dest.nativeval = result;
    return;
  }
}

int BigCount::cmp(const BigCount& op1, const BigCount& op2) {
  if (op1.usemp == false && op2.usemp == false) {
    if (op1.nativeval > op2.nativeval) {
      return 1;
    } else if (op1.nativeval == op2.nativeval) {
      return 0;
    } else if (op1.nativeval < op2.nativeval) {
      return -1;
    }
  } else if (op1.usemp == true && op2.usemp == false) {
    return 1;
  } else if (op1.usemp == false && op2.usemp == true) {
    return -1;
  } else if (op1.usemp == true && op2.usemp == true) {
    return mpz_cmp(op1.mpval, op2.mpval);
  }
  assert(0); // can't happen
  return 0;
}

void BigCount::get(mpz_t dest, const BigCount& src) {
  if (src.usemp) {
    mpz_set(dest, src.mpval);
  } else {
    mpz_set_ui(dest, src.nativeval);
  }
  return;
}

void BigCount::initmp() {
  if (usemp) {
    return;
  } else {
    mpz_init(mpval);
    usemp = true;
    return;
  }
}

BigCount::~BigCount() {
  if (usemp) {
    usemp = false;
    mpz_clear(mpval);
  }
  return;
}

BigCount::BigCount() {
  nativeval = 0;
  usemp = false;
  return;
}

BigCount::BigCount(const uint64_t init) {
  nativeval = init;
  usemp = false;
  return;
}

BigCount::BigCount(const mpz_t init) {
  usemp = false;
  uint64_t check = mpz_get_ui(init);
  if (mpz_cmp_ui(init, check) > 0) {
    initmp();
    mpz_set(mpval, init);
    return;
  } else {
    nativeval = check;
  }
}

