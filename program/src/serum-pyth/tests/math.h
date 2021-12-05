#pragma once

#include <serum-pyth/sp-util.h>
#include <serum-pyth/tests/assert.h>

static void sp_assert_midpt(
  const sp_size_t bid,
  const sp_size_t ask,
  const sp_size_t expected
) {
    sp_assert_eq(
      sp_midpt( bid, ask ),
      expected,
      "sp_midpt(%lu, %lu) == %lu != %lu",
      bid,
      ask,
      sp_midpt( bid, ask ),
      expected
    );
}

static void sp_assert_pow10div(
  const sp_size_t numer,
  const sp_size_t denom,
  const sp_expo_t expo,
  const sp_size_t expected
) {
  sp_assert_eq(
    sp_pow10_divide( numer, denom, expo ),
    expected,
    "sp_pow10_divide(%lu, %lu, %d) == %lu != %lu",
    numer,
    denom,
    expo,
    sp_pow10_divide( numer, denom, expo ),
    expected
  );
}

static void sp_test_constants()
{
  sp_assert_expo_gt( SP_EXP_MAX, 0 );
  sp_assert_expo_lt( -SP_EXP_MAX, 0 );  // Assert signed.
  sp_assert_expo_lt( SP_EXP_MAX, SOL_ARRAY_SIZE( SP_POW10 ) );

  sp_assert_size_gt( SP_SIZE_MAX, 0 );
  sp_assert_size_lt( SP_SIZE_MAX, SP_SIZE_OVERFLOW );
  sp_assert_size_gt( SP_SIZE_MAX, SP_POW10[ SP_EXP_MAX ] );

  sp_size_t pow10 = SP_POW10[ 0 ];
  sp_assert_size_eq( pow10, 1 );
  for ( unsigned e = 1; e < SOL_ARRAY_SIZE( SP_POW10 ); ++e ) {
    sp_assert_size_gt( pow10, 0 );
    sp_assert_size_gt( SP_SIZE_MAX / 10, pow10 );
    pow10 *= 10;
    sp_assert_size_eq( SP_POW10[ e ], pow10 );
  }

}

static void sp_test_midpt()
{
  for ( unsigned i = 0; i <= 5; ++i ) {
    for ( unsigned j = 0; j <= 5; ++j ) {
      unsigned const mid = ( i + j ) / 2;
      unsigned const mod = ( i + j ) % 2;
      sp_assert_midpt( i, j, mid );
      sp_assert_midpt(
        SP_SIZE_MAX - i,
        SP_SIZE_MAX - j,
        SP_SIZE_MAX - mid - mod
      );
    }
  }
}

static void sp_test_pow10div()
{
  sp_assert_pow10div( 0, 1, 0, 0 );
  sp_assert_pow10div( 0, 1, 1, 0 );
  sp_assert_pow10div( 0, 1, -1, 0 );

  sp_assert_pow10div( 1, 1, 0, 1 );
  sp_assert_pow10div( 1, 1, 1, 10 );
  sp_assert_pow10div( 1, 1, -1, 0 );

  sp_assert_pow10div( 0, 0, 0, SP_SIZE_OVERFLOW );
  sp_assert_pow10div( 0, 0, 1, SP_SIZE_OVERFLOW );
  sp_assert_pow10div( 1, 0, 0, SP_SIZE_OVERFLOW );
  sp_assert_pow10div( 1, 0, 1, SP_SIZE_OVERFLOW );

  sp_assert_pow10div( 1, 2, 0, 0 );
  sp_assert_pow10div( 1, 2, 1, 5 );
  sp_assert_pow10div( 10, 2, -1, 0 );
  sp_assert_pow10div( 100, 2, -1, 5 );

  sp_assert_pow10div( 2, 1, 0, 2 );
  sp_assert_pow10div( 2, 1, 1, 20 );
  sp_assert_pow10div( 20, 1, -1, 2 );
  sp_assert_pow10div( 200, 1, -1, 20 );

  sp_assert_pow10div( 5, 2, 0, 2 );
  sp_assert_pow10div( 5, 2, 1, 25 );
  sp_assert_pow10div( 50, 2, -1, 2 );
  sp_assert_pow10div( 500, 2, -1, 25 );

  for ( sp_expo_t e = 0; e < SP_EXP_MAX; ++e ) {
    const sp_size_t pow10 = SP_POW10[ e ];
    sp_assert_pow10div( 1, 1, e, pow10 );
    sp_assert_pow10div( 1, 2, e, pow10 / 2 );
    sp_assert_pow10div( 3, 4, e, pow10 * 3 / 4 );
    sp_assert_pow10div( pow10, 1, -e, 1 );
    sp_assert_pow10div( pow10, 5, 1 - e, 2 );
    sp_assert_pow10div( pow10, 4, 2 - e, 25 );
    if ( e > 0 ) {
      sp_assert_pow10div( 3, pow10 / 5, e, 15 );
    }
  }

  for ( sp_size_t n = 0; n < 20; ++n ) {
    for ( sp_size_t d = 1; d < 20; ++d ) {
      sp_assert_pow10div( n, d, 0, n / d );
      sp_assert_pow10div( n, d, 1, n * 10 / d );
      sp_assert_pow10div( n * 10, d, -1, n / d );
      sp_assert_pow10div( n * 100, d, -1, n * 10 / d );
    }
  }
}
