#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <solana_sdk.h>

#define SP_PACKED __attribute__(( __packed__ ))
#define SP_UNUSED __attribute__(( __unused__ ))

#define SP_LIKELY( cond ) __builtin_expect( cond, true )
#define SP_UNLIKELY( cond ) __builtin_expect( cond, false )

#define SP_ASSERT_SIZE( t, s ) \
  static_assert( sizeof( t ) == ( s ), "" )

// decltype( pc_price_t::expo_ )
// Wide and signed to catch overflow/underflow.
typedef int32_t sp_expo_t;

// decltype( serum_market::BaseLotSize ), etc.
typedef uint64_t sp_size_t;

static const sp_size_t SP_POW10[] = {
  1ul,
  10ul,
  100ul,
  1000ul,
  10000ul,
  100000ul,
  1000000ul,
  10000000ul,
  100000000ul,
  1000000000ul,
  10000000000ul,
  100000000000ul,
  1000000000000ul,
  10000000000000ul,
  100000000000000ul,
  1000000000000000ul,
  10000000000000000ul,
  100000000000000000ul,
  1000000000000000000ul,
  10000000000000000000ul,
};

static const sp_expo_t SP_EXP_MAX = SOL_ARRAY_SIZE( SP_POW10 ) - 1;
static const sp_size_t SP_SIZE_OVERFLOW = UINT64_MAX;
static const sp_size_t SP_SIZE_MAX = SP_SIZE_OVERFLOW - 1;

// Calculate 10^exp * numer / denom
static inline sp_size_t
sp_pow10_divide( sp_size_t numer, sp_size_t denom, sp_expo_t exp )
{
  if ( SP_LIKELY( exp >= 0 ) ) {
    while ( SP_UNLIKELY( exp > SP_EXP_MAX ) ) {
      if ( numer >= SP_SIZE_MAX / 10 ) {
        return SP_SIZE_OVERFLOW;
      }
      exp -= 1;
      numer *= 10;
    }
    const sp_size_t scale = SP_POW10[ exp ];
    if ( SP_UNLIKELY( numer >= SP_SIZE_MAX / scale ) ) {
      return SP_SIZE_OVERFLOW;
    }
    numer *= scale;
  }

  else {  // exp < 0
    while ( SP_UNLIKELY( -exp > SP_EXP_MAX ) ) {
      if ( denom >= SP_SIZE_MAX / 10 ) {
        return 0;
      }
      exp += 1;
      denom *= 10;
    }
    const sp_size_t scale = SP_POW10[ -exp ];
    if ( SP_UNLIKELY( denom >= SP_SIZE_MAX / scale ) ) {
      return 0;
    }
    denom *= scale;
  }

  return (
    SP_LIKELY( denom > 0 )
    ? ( numer / denom )
    : SP_SIZE_OVERFLOW
  );
}

// Return a multiplier for converting serum prices to pyth.
// Serum prices have units of QuoteLot/BaseLot.
//
static inline sp_size_t
sp_serum_to_pyth(
  const sp_expo_t pyth_exp,   // pc_price_t::expo_
  const sp_expo_t quote_exp,  // spl_mint::Decimals
  const sp_expo_t base_exp,   // spl_mint::Decimals
  const sp_size_t quote_lotsize,  // serum_market_t::BaseLotSize
  const sp_size_t base_lotsize )  // serum_market_t::QuoteLotSize
{
  // scale = 10^pyth_exp / ( 10^quote_exp / 10^base_exp )
  //       = 10( pyth_exp + base_exp - quote_exp )
  // return scale * quote_lotsize / base_lotsize
  const sp_expo_t scale_exp = pyth_exp + base_exp - quote_exp;
  return sp_pow10_divide( quote_lotsize, base_lotsize, scale_exp );
}

// Avoid overflowing with "( bid + ask ) / 2".
static inline sp_size_t sp_midpt( const sp_size_t bid, const sp_size_t ask )
{
  const sp_size_t sum_mod2 = ( bid % 2 ) + ( ask % 2 );
  return ( bid / 2 ) + ( ask / 2 ) + ( sum_mod2 / 2 );
}

// CI is half the bid-ask spread, adjusted for the best aggressive fee.
// https://docs.pyth.network/publishers/confidence-interval-and-crypto-exchange-fees
//
// spread = ask_adjusted - bid_adjusted
//        = ask * (1.0 + fee) - bid * (1.0 - fee)
//        = (ask - bid) + (ask + bid) * fee
//
static inline sp_size_t
sp_confidence( const sp_size_t bid, const sp_size_t ask )
{
  const sp_size_t fee_bps = 10ul;  // TODO Load from config or serum-dex.
  sp_size_t spread = SP_LIKELY( bid < ask ) ? ( ask - bid ) : ( bid - ask );
  spread += ( bid + ask ) * fee_bps / 10000ul;
  return spread / 2;
}

#ifdef __cplusplus
}
#endif
