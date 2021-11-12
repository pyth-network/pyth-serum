#pragma once

// --- Utils -------------------------------------------------------------------

#define PACKED __attribute__((__packed__))

#define BUF_CAST(n, t, i, l) \
  if (l < sizeof(t)) { return ERROR_ACCOUNT_DATA_TOO_SMALL; } \
  const t* n = (const t*) i; \
  i += sizeof(t); \
  l -= sizeof(t);

static const uint64_t TEN_TO_THE[20] = {
  1UL,
  10UL,
  100UL,
  1000UL,
  10000UL,
  100000UL,
  1000000UL,
  10000000UL,
  100000000UL,
  1000000000UL,
  10000000000UL,
  100000000000UL,
  1000000000000UL,
  10000000000000UL,
  100000000000000UL,
  1000000000000000UL,
  10000000000000000UL,
  100000000000000000UL,
  1000000000000000000UL,
  10000000000000000000UL
};

extern uint64_t sol_get_clock_sysvar(sysvar_clock_t* clock);

// --- SPL Token Program -------------------------------------------------------

// TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA
static const SolPubkey SPL_TOKEN_PROGRAM = { .x = {
  0x06, 0xDD, 0xF6, 0xE1, 0xD7, 0x65, 0xA1, 0x93,
  0xD9, 0xCB, 0xE1, 0x46, 0xCE, 0xEB, 0x79, 0xAC,
  0x1C, 0xB4, 0x85, 0xED, 0x5F, 0x5B, 0x37, 0x91,
  0x3A, 0x8C, 0xF5, 0x85, 0x7E, 0xFF, 0x00, 0xA9,
}};

typedef struct PACKED spl_mint
{
  uint32_t  MintAuthorityOp;
  SolPubkey MintAuthority;
  uint64_t  TotalSupply;
  uint8_t   Decimals;
  uint8_t   Init;
  uint32_t  FreezeAuthorityOp;
  SolPubkey FreezeAuthority;
} spl_mint_t;
static_assert(sizeof(spl_mint_t) == 82, "incorrect size of spl_mint_t");

// --- Serum Program -----------------------------------------------------------

typedef struct PACKED serum_flags
{
  uint64_t Initialized  : 1;
  uint64_t Market       : 1;
  uint64_t OpenOrders   : 1;
  uint64_t RequestQueue : 1;
  uint64_t EventQueue   : 1;
  uint64_t Bids         : 1;
  uint64_t Asks         : 1;
  uint64_t Disabled     : 1;
  uint64_t Reserved     : 56;
} serum_flags_t;
static_assert(sizeof(serum_flags_t) == 8, "incorrect size of serum_flags_t");

typedef struct PACKED serum_market
{
  SolPubkey OwnAddress;
  uint64_t  VaultSignerNonce;
  SolPubkey BaseMint;
  SolPubkey QuoteMint;
  SolPubkey BaseVault;
  uint64_t  BaseDepositsTotal;
  uint64_t  BaseFeesAccrued;
  SolPubkey QuoteVault;
  uint64_t  QuoteDepositsTotal;
  uint64_t  QuoteFeesAccrued;
  uint64_t  QuoteDustThreshold;
  SolPubkey RequestQueue;
  SolPubkey EventQueue;
  SolPubkey Bids;
  SolPubkey Asks;
  uint64_t  BaseLotSize;
  uint64_t  QuoteLotSize;
  uint64_t  FeeRateBps;
  uint64_t  ReferrerRebatesAccrued;
} serum_market_t;
static_assert(sizeof(serum_market_t) == 368, "incorrect size of serum_market_t");

typedef struct PACKED serum_book
{
  uint64_t BumpIndex;
  uint64_t FreeListLen;
  uint32_t FreeListHead;
  uint32_t Root;
  uint64_t LeafCount;
} serum_book_t;
static_assert(sizeof(serum_book_t) == 32, "incorrect size of serum_book_t");

#define SERUM_NODE_TYPE_INNER 1
#define SERUM_NODE_TYPE_LEAF  2

typedef struct PACKED serum_node_any
{
  uint32_t Tag;
  uint32_t Data[17];
} serum_node_any_t;
static_assert(sizeof(serum_node_any_t) == 72, "incorrect size of serum_node_any_t");

typedef struct PACKED serum_node_inner
{
  uint32_t Tag;
  uint32_t PrefixLen;
  uint64_t Key0;
  uint64_t Key1;
  uint32_t ChildA;
  uint32_t ChildB;
} serum_node_inner_t;
static_assert(sizeof(serum_node_inner_t) == 32, "incorrect size of serum_node_inner_t");

typedef struct PACKED serum_node_leaf
{
  uint32_t  Tag;
  uint8_t   OwnerSlot;
  uint8_t   FeeTier;
  uint8_t   Padding[2];
  uint64_t  Key0;
  uint64_t  Key1;
  SolPubkey Owner;
  uint64_t  Quantity;
  uint64_t  ClientId;
} serum_node_leaf_t;
static_assert(sizeof(serum_node_leaf_t) == 72, "incorrect size of serum_node_leaf_t");

static bool trim_serum_padding(uint8_t** iter, uint64_t* left)
{
  if (*left < 5 || sol_memcmp(*iter, "serum", 5) != 0)
    return false;
  *iter = *iter + 5;
  *left = *left - 5;

  if (*left < 7 || sol_memcmp(*iter + *left - 7, "padding", 7) != 0)
    return false;
  *left = *left - 7;

  return true;
}

// --- serum-pyth -------------------------------------------------------------

// CI is half the bid-ask spread, adjusted for the best aggressive fee.
// https://docs.pyth.network/publishers/confidence-interval-and-crypto-exchange-fees
// spread = ask_adjusted - bid_adjusted
//   = ask * (1.0 + fee) - bid * (1.0 - fee)
//   = (ask - bid) + (ask + bid) * fee
static inline uint64_t get_confidence(const uint64_t bid, const uint64_t ask)
{
  const uint64_t fee_bps = 10UL;  // TODO Read from config or serum API
  uint64_t spread = (bid < ask) ? (ask - bid) : (bid - ask);
  spread += (bid + ask) * fee_bps / 10000UL;
  return spread / 2;
}
