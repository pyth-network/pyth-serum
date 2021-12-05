#pragma once

#include <serum-pyth/sp-util.h>
#include <serum-pyth/tests/assert.h>
#include <serum-pyth/tests/instruction.h>

static void sp_assert_s2p(
  sp_test_input_t* const input,
  const sp_expo_t pyth_exp,
  const sp_expo_t quote_exp,
  const sp_expo_t base_exp,
  const sp_size_t quote_lotsize,
  const sp_size_t base_lotsize,
  const sp_size_t expected_s2p
) {
  const sp_size_t actual_s2p = sp_serum_to_pyth(
    pyth_exp,
    quote_exp,
    base_exp,
    quote_lotsize,
    base_lotsize
  );

  sp_assert_eq(
    actual_s2p,
    expected_s2p,
    "sp_serum_to_pyth(%d, %d, %d, %lu, %lu) == %lu != %lu",
    pyth_exp,
    quote_exp,
    base_exp,
    quote_lotsize,
    base_lotsize,
    actual_s2p,
    expected_s2p
  );

  sp_set_bid_ask( input, 1, 1 );
  sp_set_pyth_expo( input, pyth_exp );
  sp_set_quote_expo( input, quote_exp );
  sp_set_base_expo( input, base_exp );
  sp_set_quote_lot( input, quote_lotsize );
  sp_set_base_lot( input, base_lotsize );

  sp_pyth_instruction_t inst;
  if ( expected_s2p == SP_SIZE_OVERFLOW ) {
    sp_assert_err( input, &inst, ERROR_INVALID_ACCOUNT_DATA );
  }
  else {
    sp_assert_no_err( input, &inst );
    sp_assert_eq(
      inst.cmd.price_,
      ( int64_t )( expected_s2p ),
      "s2p(%d, %d, %d, %lu, %lu) -> cmd.price_ == %lu%s != %lu",
      pyth_exp,
      quote_exp,
      base_exp,
      quote_lotsize,
      base_lotsize,
      inst.cmd.price_,
      inst.cmd.price_ == ( int64_t )SP_SIZE_OVERFLOW ? " (overflow)" : "",
      expected_s2p
    );
  }
}

#define sp_assert_s2p_oflow( ... ) \
  sp_assert_s2p( __VA_ARGS__, SP_SIZE_OVERFLOW )

// Original logic and input types for serum_to_pyth:
static uint64_t sp_old_s2p(
  const uint8_t pyth_exp,
  const uint8_t quote_exp,
  const uint8_t base_exp,
  const uint64_t quote_lotsize,
  const uint64_t base_lotsize )
{
  // Old impl required pyth_exp >= max( quote_exp, base_exp ).
  if ( pyth_exp < quote_exp ) {
    return 0;
  }
  if ( pyth_exp < base_exp ) {
    return SP_SIZE_OVERFLOW;
  }

  uint64_t pyth_scalar = SP_POW10[ pyth_exp ];
  uint64_t quote_to_pyth = quote_lotsize * SP_POW10[ pyth_exp - quote_exp ];
  uint64_t base_to_pyth = base_lotsize * SP_POW10[ pyth_exp - base_exp ];

  if ( quote_lotsize >= SP_SIZE_MAX / pyth_scalar ) {
    return SP_SIZE_OVERFLOW;
  }

  return quote_to_pyth * pyth_scalar / base_to_pyth;
}

static void sp_test_serum_to_pyth()
{
  sp_test_input_t inp;
  sp_init_test_input( &inp );

  // pyth_exp, quote_exp, base_exp, quote_lotsize, base_lotsize, expected
  sp_assert_s2p( &inp, 0, 0, 0, 1, 1, 1 );
  sp_assert_s2p( &inp, 0, 0, 1, 1, 1, 10 );
  sp_assert_s2p( &inp, 0, 1, 0, 1, 1, 0 );
  sp_assert_s2p( &inp, 0, 1, 1, 1, 1, 1 );
  sp_assert_s2p( &inp, 1, 0, 0, 1, 1, 10 );
  sp_assert_s2p( &inp, 1, 0, 1, 1, 1, 100 );
  sp_assert_s2p( &inp, 1, 1, 1, 1, 1, 10 );

  // MSOL/USDC: pyth_exp=8
  // USDC: base_exp=6
  // MSOL: quote_exp=9
  // let base_lotsize  = 2 * 10^2 [units 10^-6 USDC]
  // let quote_lotsize = 3 * 10^6 [units 10^-9 MSOL]
  // quote_to_pyth = quote_lotsize * 10^(pyth_exp - quote_exp) = 3 * 10^5
  // base_to_pyth = base_lotsize * 10^(pyth_exp - base_exp) = 2 * 10^4
  // serum_to_pyth = 10^(pyth_exp) * quote_to_pyth / base_to_pyth
  // serum_to_pyth = (3/2) * 10^(5 - 4 + 8) = 15 * 10^8
  sp_assert_s2p( &inp, 8, 9, 6, 3000000, 200, 1500000000 );

  // base_lotsize=0 -> divide-by-zero -> SP_SIZE_OVERFLOW
  sp_assert_s2p_oflow( &inp, 0, 0, 0, 1, 0 );
  sp_assert_s2p_oflow( &inp, 0, 0, 1, 1, 0 );
  sp_assert_s2p_oflow( &inp, 0, 1, 0, 1, 0 );
  sp_assert_s2p_oflow( &inp, 0, 1, 1, 1, 0 );
  sp_assert_s2p_oflow( &inp, 1, 0, 0, 1, 0 );
  sp_assert_s2p_oflow( &inp, 1, 0, 1, 1, 0 );
  sp_assert_s2p_oflow( &inp, 1, 1, 1, 1, 0 );

  // ( numer > SP_SIZE_MAX ) -> SP_SIZE_OVERFLOW
  sp_assert_s2p_oflow( &inp, SP_EXP_MAX, 1, 1, SP_SIZE_MAX, 1 );
  sp_assert_s2p_oflow( &inp, 0, 1, SP_EXP_MAX, SP_SIZE_MAX, 1 );

  // ( denom > SP_SIZE_MAX ) -> 0
  sp_assert_s2p( &inp, 0, SP_EXP_MAX, 1, 1, SP_SIZE_MAX, 0 );

  // Internal state shouldn't overflow if expected output < SP_SIZE_MAX.
  sp_assert_s2p( &inp, 2, SP_EXP_MAX, SP_EXP_MAX, 600, 20, 3000 );
  sp_assert_s2p( &inp, SP_EXP_MAX, SP_EXP_MAX, 2, 600, 20, 3000 );

  // Compare to old serum_to_pyth logic.
  const uint8_t max_pyth_exp = ( uint8_t )( SP_EXP_MAX / 2 - 1 );
  const uint64_t max_lotsize = SP_POW10[ 3 ];
  for ( uint8_t pe = 0; pe <= max_pyth_exp; ++pe ) {
    for ( uint8_t qe = 0; qe <= pe; ++qe ) {
      for ( uint8_t be = 0; be <= pe; ++be ) {
        for ( uint64_t ql = 1; ql <= max_lotsize; ql *= 10 ) {
          for ( uint64_t bl = 1; bl <= max_lotsize; bl *= 10 ) {
            const uint64_t old = sp_old_s2p( pe, qe, be, ql, bl );
            sp_assert_s2p( &inp, pe, qe, be, ql, bl, old );
          }
        }
      }
    }
  }
}
