char heap_start[8192];
#define PC_HEAP_START (heap_start)
#include "serum-pyth.c"
#include <criterion/criterion.h>

#define sp_expect_eq(act, exp, fmt) \
  cr_expect_eq(act, exp, "expected " fmt ", got " fmt, exp, act)

#define sp_expect_uint(act, exp) \
  sp_expect_eq(act, exp, "%lu")

Test(serum_pyth, confidence) {

  // https://docs.pyth.network/publishers/confidence-interval-and-crypto-exchange-fees
  // bid=$50,000.000, ask=$50,000.010, fee=10bps -> CI=$50.005
  sp_expect_uint(get_confidence(50000000, 50000010), 50005ul);
  sp_expect_uint(get_confidence(50000010, 50000000), 50005ul);

}
