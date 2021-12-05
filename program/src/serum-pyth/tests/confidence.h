#pragma once

#include <serum-pyth/tests/assert.h>
#include <serum-pyth/tests/instruction.h>

static void sp_assert_conf(
  sp_test_input_t* const input,
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

  sp_pyth_instruction_t inst;
  sp_set_bid_ask( input, bid, ask );
  sp_assert_no_err( input, &inst );

  sp_assert_eq(
    inst.cmd.conf_,
    expected,
    "cmd.conf_(bid=%lu, ask=%lu) == %lu != %lu",
    bid,
    ask,
    inst.cmd.conf_,
    expected
  );
}

static void sp_test_confidence()
{
  sp_test_input_t input;
  sp_init_test_input( &input );

  // https://docs.pyth.network/publishers/confidence-interval-and-crypto-exchange-fees
  // bid=$50,000.000, ask=$50,000.010, fee=10bps -> CI=$50.005
  sp_assert_conf( &input, 50000000, 50000010, 50005 );
  sp_assert_conf( &input, 50000010, 50000000, 50005 );
}
