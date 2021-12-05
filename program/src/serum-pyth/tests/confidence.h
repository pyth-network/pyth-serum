#pragma once

#include <serum-pyth/tests/assert.h>

static void sp_assert_conf(
  const sp_size_t bid,
  const sp_size_t ask,
  const sp_size_t expected
) {
  const sp_size_t actual = sp_confidence( bid, ask );
  sp_assert_eq(
    actual,
    expected,
    "sp_confidence(%lu, %lu) == %lu != %lu",
    bid,
    ask,
    actual,
    expected
  );
}

static void sp_test_confidence()
{
  // https://docs.pyth.network/publishers/confidence-interval-and-crypto-exchange-fees
  // bid=$50,000.000, ask=$50,000.010, fee=10bps -> CI=$50.005
  sp_assert_conf( 50000000, 50000010, 50005 );
  sp_assert_conf( 50000010, 50000000, 50005 );
}
