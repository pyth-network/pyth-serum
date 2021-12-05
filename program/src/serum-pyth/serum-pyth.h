#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <serum-pyth/sp-util.h>

// --- SPL Token Program -------------------------------------------------------

// TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA
static const SolPubkey SPL_TOKEN_PROGRAM = { .x = {
  0x06, 0xDD, 0xF6, 0xE1, 0xD7, 0x65, 0xA1, 0x93,
  0xD9, 0xCB, 0xE1, 0x46, 0xCE, 0xEB, 0x79, 0xAC,
  0x1C, 0xB4, 0x85, 0xED, 0x5F, 0x5B, 0x37, 0x91,
  0x3A, 0x8C, 0xF5, 0x85, 0x7E, 0xFF, 0x00, 0xA9,
}};

typedef struct SP_PACKED spl_mint
{
  SP_UNUSED
  uint32_t  MintAuthorityOp;
  SP_UNUSED
  SolPubkey MintAuthority;
  SP_UNUSED
  uint64_t  TotalSupply;
  uint8_t   Decimals;
  SP_UNUSED
  uint8_t   Init;
  SP_UNUSED
  uint32_t  FreezeAuthorityOp;
  SP_UNUSED
  SolPubkey FreezeAuthority;
} spl_mint_t;

SP_ASSERT_SIZE( spl_mint_t, 82 );

// --- Serum Program -----------------------------------------------------------

typedef struct SP_PACKED serum_flags
{
  uint64_t Initialized  : 1;
  uint64_t Market       : 1;
  SP_UNUSED
  uint64_t OpenOrders   : 1;
  SP_UNUSED
  uint64_t RequestQueue : 1;
  SP_UNUSED
  uint64_t EventQueue   : 1;
  uint64_t Bids         : 1;
  uint64_t Asks         : 1;
  uint64_t Disabled     : 1;
  uint64_t Reserved     : 56;
} serum_flags_t;

SP_ASSERT_SIZE( serum_flags_t, 8 );

typedef struct SP_PACKED serum_market
{
  SolPubkey OwnAddress;
  SP_UNUSED
  uint64_t  VaultSignerNonce;
  SolPubkey BaseMint;
  SolPubkey QuoteMint;
  SP_UNUSED
  SolPubkey BaseVault;
  SP_UNUSED
  uint64_t  BaseDepositsTotal;
  SP_UNUSED
  uint64_t  BaseFeesAccrued;
  SP_UNUSED
  SolPubkey QuoteVault;
  SP_UNUSED
  uint64_t  QuoteDepositsTotal;
  SP_UNUSED
  uint64_t  QuoteFeesAccrued;
  SP_UNUSED
  uint64_t  QuoteDustThreshold;
  SP_UNUSED
  SolPubkey RequestQueue;
  SP_UNUSED
  SolPubkey EventQueue;
  SolPubkey Bids;
  SolPubkey Asks;
  uint64_t  BaseLotSize;
  uint64_t  QuoteLotSize;
  SP_UNUSED
  uint64_t  FeeRateBps;
  SP_UNUSED
  uint64_t  ReferrerRebatesAccrued;
} serum_market_t;

SP_ASSERT_SIZE( serum_market_t, 368 );

typedef struct SP_PACKED serum_book
{
  SP_UNUSED
  uint64_t BumpIndex;
  SP_UNUSED
  uint64_t FreeListLen;
  SP_UNUSED
  uint32_t FreeListHead;
  uint32_t Root;
  uint64_t LeafCount;
} serum_book_t;

SP_ASSERT_SIZE( serum_book_t, 32 );

#define SERUM_NODE_TYPE_INNER 1
#define SERUM_NODE_TYPE_LEAF  2

typedef struct SP_PACKED serum_node_any
{
  uint32_t Tag;
  SP_UNUSED
  uint32_t Data[17];
} serum_node_any_t;

SP_ASSERT_SIZE( serum_node_any_t, 72 );

typedef struct SP_PACKED serum_node_inner
{
  SP_UNUSED
  uint32_t Tag;
  SP_UNUSED
  uint32_t PrefixLen;
  SP_UNUSED
  uint64_t Key0;
  SP_UNUSED
  uint64_t Key1;
  uint32_t ChildA;
  uint32_t ChildB;
} serum_node_inner_t;

SP_ASSERT_SIZE( serum_node_inner_t, 32 );

typedef struct SP_PACKED serum_node_leaf
{
  SP_UNUSED
  uint32_t  Tag;
  SP_UNUSED
  uint8_t   OwnerSlot;
  SP_UNUSED
  uint8_t   FeeTier;
  SP_UNUSED
  uint8_t   Padding[2];
  SP_UNUSED
  uint64_t  Key0;
  uint64_t  Key1;
  SP_UNUSED
  SolPubkey Owner;
  SP_UNUSED
  uint64_t  Quantity;
  SP_UNUSED
  uint64_t  ClientId;
} serum_node_leaf_t;

SP_ASSERT_SIZE( serum_node_leaf_t, 72 );

static const char SERUM_HEADER[] = "serum";
static const char SERUM_FOOTER[] = "padding";

// No trailing '\0':
#define SERUM_HEADER_LEN ( sizeof( SERUM_HEADER ) - 1 )
#define SERUM_FOOTER_LEN ( sizeof( SERUM_FOOTER ) - 1 )

static_assert( SERUM_HEADER_LEN == 5, "" );
static_assert( SERUM_FOOTER_LEN == 7, "" );

static inline bool sp_has_serum_header(
  const uint8_t* const buf,
  const uint64_t len
) {
  return SP_LIKELY( len >= SERUM_HEADER_LEN ) && SP_LIKELY( ! sol_memcmp(
    buf,
    SERUM_HEADER,
    SERUM_HEADER_LEN
  ) );
}

static inline bool sp_has_serum_footer(
  const uint8_t* const buf,
  const uint64_t len
) {
  return SP_LIKELY( len >= SERUM_FOOTER_LEN ) && SP_LIKELY( ! sol_memcmp(
    buf + len - SERUM_FOOTER_LEN,
    SERUM_FOOTER,
    SERUM_FOOTER_LEN
  ) );
}

static inline bool trim_serum_padding(
  uint8_t** const iter,
  uint64_t* const left
) {
  if ( ! sp_has_serum_header( *iter, *left ) ) {
    return false;
  }

  *iter += SERUM_HEADER_LEN;
  *left -= SERUM_HEADER_LEN;

  if ( ! sp_has_serum_footer( *iter, *left ) ) {
    return false;
  }

  *left -= SERUM_FOOTER_LEN;

  return true;
}

static inline bool sp_flags_valid(
  const serum_flags_t* const flags,
  const uint64_t field
) {
  return (
    SP_LIKELY( !! field )
    && SP_LIKELY( !! flags->Initialized )
    && SP_LIKELY( ! flags->Disabled )
    && SP_LIKELY( ! flags->Reserved )
  );
}

#ifdef __cplusplus
}
#endif
