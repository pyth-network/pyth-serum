// Set PC_HEAP_START before including oracle.h.
char heap_start[ 8192 ];
#define PC_HEAP_START ( heap_start )

#include <serum-pyth/serum-pyth.c> // NOLINT(bugprone-suspicious-include)
#include <criterion/criterion.h>

// Turn off to stop test suites on first failure.
#define SP_ASSERT_ALL 1

#if SP_ASSERT_ALL
#define sp_test_eq cr_expect_eq
#define sp_test_ne cr_expect_neq
#define sp_test_le cr_expect_leq
#define sp_test_lt cr_expect_lt
#define sp_test_ge cr_expect_geq
#define sp_test_gt cr_expect_gt
#else
#define sp_test_eq cr_assert_eq
#define sp_test_ne cr_assert_neq
#define sp_test_le cr_assert_leq
#define sp_test_lt cr_assert_lt
#define sp_test_ge cr_assert_geq
#define sp_test_gt cr_assert_gt
#endif

Test( serum_pyth, constants ) {

  sp_test_gt( SP_EXP_MAX, 0 );
  sp_test_lt( -SP_EXP_MAX, 0 );  // Assert signed.
  sp_test_lt( SP_EXP_MAX, SOL_ARRAY_SIZE( SP_POW10 ) );

  sp_test_gt( SP_SIZE_MAX, 0 );
  sp_test_lt( SP_SIZE_MAX, SP_SIZE_OVERFLOW );
  sp_test_gt( SP_SIZE_MAX, SP_POW10[ SP_EXP_MAX ] );

  sp_size_t pow10 = SP_POW10[ 0 ];
  sp_test_eq( pow10, 1 );
  for ( unsigned e = 1; e < SOL_ARRAY_SIZE( SP_POW10 ); ++e ) {
    sp_test_gt( pow10, 0 );
    sp_test_gt( SP_SIZE_MAX / 10, pow10 );
    pow10 *= 10;
    sp_test_eq( SP_POW10[ e ], pow10 );
  }

}

#define sp_test_div( numer, denom, expo, expected ) sp_test_eq( \
  sp_pow10_divide( numer, denom, expo ), \
  expected, \
  "sp_pow10_divide(%lu, %lu, %d) == %lu != %lu", \
  ( sp_size_t )( numer ), \
  ( sp_size_t )( denom ), \
  ( sp_exponent_t )( expo ), \
  sp_pow10_divide( numer, denom, expo ), \
  ( sp_size_t )( expected ) \
)

Test( serum_pyth, pow10_divide ) {

  sp_test_div( 0, 1, 0, 0 );
  sp_test_div( 0, 1, 1, 0 );
  sp_test_div( 0, 1, -1, 0 );

  sp_test_div( 1, 1, 0, 1 );
  sp_test_div( 1, 1, 1, 10 );
  sp_test_div( 1, 1, -1, 0 );

  sp_test_div( 0, 0, 0, SP_SIZE_OVERFLOW );
  sp_test_div( 0, 0, 1, SP_SIZE_OVERFLOW );
  sp_test_div( 1, 0, 0, SP_SIZE_OVERFLOW );
  sp_test_div( 1, 0, 1, SP_SIZE_OVERFLOW );

  sp_test_div( 1, 2, 0, 0 );
  sp_test_div( 1, 2, 1, 5 );
  sp_test_div( 10, 2, -1, 0 );
  sp_test_div( 100, 2, -1, 5 );

  sp_test_div( 2, 1, 0, 2 );
  sp_test_div( 2, 1, 1, 20 );
  sp_test_div( 20, 1, -1, 2 );
  sp_test_div( 200, 1, -1, 20 );

  sp_test_div( 5, 2, 0, 2 );
  sp_test_div( 5, 2, 1, 25 );
  sp_test_div( 50, 2, -1, 2 );
  sp_test_div( 500, 2, -1, 25 );

  for ( sp_exponent_t e = 0; e < SP_EXP_MAX; ++e ) {
    const sp_size_t pow10 = SP_POW10[ e ];
    sp_test_div( 1, 1, e, pow10 );
    sp_test_div( 1, 2, e, pow10 / 2 );
    sp_test_div( 3, 4, e, pow10 * 3 / 4 );
    sp_test_div( pow10, 1, -e, 1 );
    sp_test_div( pow10, 5, 1 - e, 2 );
    sp_test_div( pow10, 4, 2 - e, 25 );
    if ( e > 0 ) {
      sp_test_div( 3, pow10 / 5, e, 15 );
    }
  }

  for ( sp_size_t n = 0; n < 20; ++n ) {
    for ( sp_size_t d = 1; d < 20; ++d ) {
      sp_test_div( n, d, 0, n / d );
      sp_test_div( n, d, 1, n * 10 / d );
      sp_test_div( n * 10, d, -1, n / d );
      sp_test_div( n * 100, d, -1, n * 10 / d );
    }
  }
}

// Assert pyth-client allocations use test heap.
Test( serum_pyth, heap_start ) {

  sp_test_eq( PC_HEAP_START, heap_start );
  sp_test_ne( PC_HEAP_START, NULL );
  sp_test_ne( ( uint64_t ) PC_HEAP_START, HEAP_START_ADDRESS );

}

#define sp_test_conf( bid, ask, expected ) sp_test_eq( \
  sp_confidence( bid, ask ), \
  expected, \
  "sp_confidence(%lu, %lu) == %lu != %lu", \
  ( sp_size_t )( bid ), \
  ( sp_size_t )( ask ), \
  sp_confidence( bid, ask ), \
  ( sp_size_t )( expected ) \
)

Test( serum_pyth, confidence ) {

  // https://docs.pyth.network/publishers/confidence-interval-and-crypto-exchange-fees
  // bid=$50,000.000, ask=$50,000.010, fee=10bps -> CI=$50.005
  sp_test_conf( 50000000, 50000010, 50005 );
  sp_test_conf( 50000010, 50000000, 50005 );

}

// Old logic and input types for serum_to_pyth:
static inline uint64_t
sp_serum_to_pyth_old(
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

  // 1 [QuoteLot] = QuoteLotSize [QuoteNative]
  // 1 [BaseLot]  = BaseLotSize  [BaseNative]
  // 1 [QuoteNative] = 10^(PythExponent - QuoteNativeExponent) [Pyth]
  // 1 [BaseNative]  = 10^(PythExponent - BaseNativeExponent)  [Pyth]
  uint64_t pyth_scalar = SP_POW10[ pyth_exp ];
  uint64_t quote_to_pyth = quote_lotsize * SP_POW10[ pyth_exp - quote_exp ];
  uint64_t base_to_pyth = base_lotsize * SP_POW10[ pyth_exp - base_exp ];

  if ( quote_lotsize > SP_SIZE_MAX / pyth_scalar ) {
    // Old logic overflowed here.
    return SP_SIZE_OVERFLOW;
  }

  return quote_to_pyth * pyth_scalar / base_to_pyth;
}

#define sp_test_s2p( pe, qe, be, ql, bl, expected ) sp_test_eq( \
  sp_serum_to_pyth( pe, qe, be, ql, bl ), \
  expected, \
  "sp_serum_to_pyth(%d, %d, %d, %lu, %lu) == %lu != %lu", \
  ( sp_exponent_t )( pe ), \
  ( sp_exponent_t )( qe ), \
  ( sp_exponent_t )( be ), \
  ( sp_size_t )( ql ), \
  ( sp_size_t )( bl ), \
  sp_serum_to_pyth( pe, qe, be, ql, bl ), \
  ( sp_size_t )( expected ) \
)

#define sp_test_s2p_oflow( ... ) \
  sp_test_s2p( __VA_ARGS__, SP_SIZE_OVERFLOW )

Test( serum_pyth, serum_to_pyth ) {

  // pyth_exp, quote_exp, base_exp, quote_lotsize, base_lotsize, expected
  sp_test_s2p( 0, 0, 0, 1, 1, 1 );
  sp_test_s2p( 0, 0, 1, 1, 1, 10 );
  sp_test_s2p( 0, 1, 0, 1, 1, 0 );
  sp_test_s2p( 0, 1, 1, 1, 1, 1 );
  sp_test_s2p( 1, 0, 0, 1, 1, 10 );
  sp_test_s2p( 1, 0, 1, 1, 1, 100 );
  sp_test_s2p( 1, 1, 1, 1, 1, 10 );

  // MSOL/USDC: pyth_exp=8
  // USDC: base_exp=6
  // MSOL: quote_exp=9
  // let base_lotsize  = 2 * 10^2 [units 10^-6 USDC]
  // let quote_lotsize = 3 * 10^6 [units 10^-9 MSOL]
  // quote_to_pyth = quote_lotsize * 10^(pyth_exp - quote_exp) = 3 * 10^5
  // base_to_pyth = base_lotsize * 10^(pyth_exp - base_exp) = 2 * 10^4
  // serum_to_pyth = 10^(pyth_exp) * quote_to_pyth / base_to_pyth
  // serum_to_pyth = (3/2) * 10^(5 - 4 + 8) = 15 * 10^8
  sp_test_s2p( 8, 9, 6, 3000000, 200, 1500000000 );

  // base_lotsize=0 -> divide-by-zero -> SP_SIZE_OVERFLOW
  sp_test_s2p_oflow( 0, 0, 0, 1, 0 );
  sp_test_s2p_oflow( 0, 0, 1, 1, 0 );
  sp_test_s2p_oflow( 0, 1, 0, 1, 0 );
  sp_test_s2p_oflow( 0, 1, 1, 1, 0 );
  sp_test_s2p_oflow( 1, 0, 0, 1, 0 );
  sp_test_s2p_oflow( 1, 0, 1, 1, 0 );
  sp_test_s2p_oflow( 1, 1, 1, 1, 0 );

  // ( numer > SP_SIZE_MAX ) -> SP_SIZE_OVERFLOW
  sp_test_s2p_oflow( SP_EXP_MAX, 1, 1, SP_SIZE_MAX, 1 );
  sp_test_s2p_oflow( 0, 1, SP_EXP_MAX, SP_SIZE_MAX, 1 );

  // ( denom > SP_SIZE_MAX ) -> 0
  sp_test_s2p( 0, SP_EXP_MAX, 1, 1, SP_SIZE_MAX, 0 );

  // Internal state shouldn't overflow if expected output < SP_SIZE_MAX.
  sp_test_s2p( 2, SP_EXP_MAX, SP_EXP_MAX, 600, 20, 3000 );
  sp_test_s2p( SP_EXP_MAX, SP_EXP_MAX, 2, 600, 20, 3000 );

  // Compare to old serum_to_pyth logic.
  const uint8_t max_pyth_exp = ( uint8_t )( SP_EXP_MAX / 2 - 1 );
  const uint64_t max_lotsize = SP_POW10[ 3 ];
  for ( uint8_t pe = 0; pe <= max_pyth_exp; ++pe ) {
    for ( uint8_t qe = 0; qe <= pe; ++qe ) {
      for ( uint8_t be = 0; be <= pe; ++be ) {
        for ( uint64_t ql = 1; ql <= max_lotsize; ql *= 10 ) {
          for ( uint64_t bl = 1; bl <= max_lotsize; bl *= 10 ) {
            const uint64_t old = sp_serum_to_pyth_old( pe, qe, be, ql, bl );
            sp_test_s2p( pe, qe, be, ql, bl, old );
          }
        }
      }
    }
  }
}
