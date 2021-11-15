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

typedef uint8_t sp_exponent_t;
typedef uint64_t sp_size_t;

static const sp_size_t TEN_TO_THE[] = {
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
  sp_size_t spread = ( bid < ask ) ? ( ask - bid ) : ( bid - ask );
  spread += ( bid + ask ) * fee_bps / 10000ul;
  return spread / 2;
}

#ifdef __cplusplus
}
#endif
