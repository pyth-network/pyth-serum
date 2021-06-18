#include <solana_sdk.h>

#include "../../../../pyth-client/program/src/oracle/oracle.h" //TODO

#define PACKED __attribute__((__packed__))

//extern uint64_t sol_get_clock_sysvar(sysvar_clock_t* clock);

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

#define BUF_CAST(n, t, i, l) \
  if (l < sizeof(t)) { return ERROR_ACCOUNT_DATA_TOO_SMALL; } \
  const t* n = (const t*) i; \
  i += sizeof(t); \
  l -= sizeof(t);

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

static bool verify_serum_padding(uint8_t** iter, uint64_t* left)
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

extern uint64_t entrypoint(const uint8_t* input)
{
  // This program takes a single instruction, which has no binary data, and
  // expects the following accounts as parameters:
  // 0) Payer               [signer,writeable]
  // 1) Pyth Price          [writeable]
  // 2) Serum ProgramID     []
  // 3) Serum Market        []
  // 4) Serum Bids          []
  // 5) Serum Asks          []
  // 6) SPL Quote Mint      []
  // 7) SPL Base Mint       []
  // 8) Sysvar Clock        []
  // 9) Pyth ProgramID      []
  // 10) Pyth Param         []

  SolAccountInfo input_accounts[11];
  SolParameters input_params;
  input_params.ka = input_accounts;
  if (!sol_deserialize(input, &input_params, SOL_ARRAY_SIZE(input_accounts)))
    return ERROR_INVALID_ARGUMENT;
  if (input_params.ka_num != SOL_ARRAY_SIZE(input_accounts))
    return ERROR_NOT_ENOUGH_ACCOUNT_KEYS;

  const SolAccountInfo* account_payer          = input_accounts + 0;
  const SolAccountInfo* account_pyth_price     = input_accounts + 1;
  const SolAccountInfo* account_serum_prog     = input_accounts + 2;
  const SolAccountInfo* account_serum_market   = input_accounts + 3;
  const SolAccountInfo* account_serum_bids     = input_accounts + 4;
  const SolAccountInfo* account_serum_asks     = input_accounts + 5;
  const SolAccountInfo* account_spl_quote_mint = input_accounts + 6;
  const SolAccountInfo* account_spl_base_mint  = input_accounts + 7;
  const SolAccountInfo* account_sysvar_clock   = input_accounts + 8;
  const SolAccountInfo* account_pyth_prog      = input_accounts + 9;
  const SolAccountInfo* account_pyth_param     = input_accounts + 10;

  bool trading = true;
  uint64_t base_lot_size = 0;
  uint64_t quote_lot_size = 0;
  uint64_t best_bid_lots = 0;
  uint64_t best_ask_lots = 0;
  uint8_t quote_exponent = 0;
  uint8_t base_exponent = 0;
  uint64_t pyth_bid = 0;
  uint64_t pyth_ask = 0;
  sysvar_clock_t* clock; //sol_get_clock_sysvar(&clock);

  // Verify constraints on payer
  {
    //TODO
    sol_log("payer passed validation");
  }

  // Verify constraints on Clock sysvar
  {
    SolPubkey pk;
    sol_memcpy(pk.x, sysvar_clock, sizeof(pk.x));
    if (!SolPubkey_same(account_sysvar_clock->key, &pk))
      return ERROR_INVALID_ARGUMENT;
    if (account_sysvar_clock->data_len != sizeof(sysvar_clock_t))
      return ERROR_ACCOUNT_DATA_TOO_SMALL;
    clock = (sysvar_clock_t*) account_sysvar_clock->data;
  }

  // Verify constraints on Pyth program ID
  {
    //TODO
  }

  // Verify constraints on Pyth price account
  {
    //TODO
  }

  // Verify constraints on SPL quote mint
  {
    if (account_spl_quote_mint->data_len != sizeof(spl_mint_t))
      return ERROR_ACCOUNT_DATA_TOO_SMALL;
    quote_exponent = ((spl_mint_t*) account_spl_quote_mint->data)->Decimals;
  }

  // Verify constraints on SPL base mint
  {
    if (account_spl_base_mint->data_len != sizeof(spl_mint_t))
      return ERROR_ACCOUNT_DATA_TOO_SMALL;
    base_exponent = ((spl_mint_t*) account_spl_base_mint->data)->Decimals;
  }

  // Verify constraints on Serum progam ID
  {
    if (!account_serum_prog->executable)
      return ERROR_INVALID_ARGUMENT;
    sol_log("serum program ID passed validation");
  }

  // Verify constraints on Serum market
  {
    if (!SolPubkey_same(account_serum_market->owner, account_serum_prog->key))
      return ERROR_INCORRECT_PROGRAM_ID;

    uint8_t* iter = account_serum_market->data;
    uint64_t left = account_serum_market->data_len;
    if (!verify_serum_padding(&iter, &left))
      return ERROR_INVALID_ACCOUNT_DATA;

    BUF_CAST(flags, serum_flags_t, iter, left);
    if (!flags->Initialized || !flags->Market || flags->Disabled || flags->Reserved)
      return ERROR_INVALID_ACCOUNT_DATA;

    BUF_CAST(market, serum_market_t, iter, left);
    if (!SolPubkey_same(&market->OwnAddress, account_serum_market->key) ||
        !SolPubkey_same(&market->QuoteMint, account_spl_quote_mint->key) ||
        !SolPubkey_same(&market->BaseMint, account_spl_base_mint->key) ||
        !SolPubkey_same(&market->Bids, account_serum_bids->key) ||
        !SolPubkey_same(&market->Asks, account_serum_asks->key))
      return ERROR_INVALID_ACCOUNT_DATA;

    base_lot_size = market->BaseLotSize;
    quote_lot_size = market->QuoteLotSize;

    sol_log("serum market passed validation");
  }

  // Verify constraints on Serum bids
  {
    if (!SolPubkey_same(account_serum_bids->owner, account_serum_prog->key))
      return ERROR_INCORRECT_PROGRAM_ID;

    uint8_t* iter = account_serum_bids->data;
    uint64_t left = account_serum_bids->data_len;
    if (!verify_serum_padding(&iter, &left))
      return ERROR_INVALID_ACCOUNT_DATA;

    BUF_CAST(flags, serum_flags_t, iter, left);
    if (!flags->Initialized || !flags->Bids || flags->Disabled || flags->Reserved)
      return ERROR_INVALID_ACCOUNT_DATA;

    BUF_CAST(book, serum_book_t, iter, left);
    serum_node_any_t* nodes = (serum_node_any_t*) iter;

    uint64_t max_nodes = left / sizeof(serum_node_any_t);
    if (book->LeafCount > max_nodes)
      return ERROR_INVALID_ACCOUNT_DATA;

    if (book->LeafCount == 0) {
      trading = false;
    } else {
      uint32_t idx = book->Root;
      while (true) {
        if (idx > max_nodes)
          return ERROR_INVALID_ACCOUNT_DATA;
        serum_node_any_t* node = &nodes[idx];
        if (node->Tag == SERUM_NODE_TYPE_LEAF) {
          best_bid_lots = ((serum_node_leaf_t*) node)->Key1;
          break;
        } else if (node->Tag == SERUM_NODE_TYPE_INNER) {
          idx = ((serum_node_inner_t*) node)->ChildB; // Larger prices to the right
        } else {
          return ERROR_INVALID_ACCOUNT_DATA;
        }
      }
    }

    sol_log("serum bids passed validation");
  }

  // Verify constraints on Serum asks
  {
    if (!SolPubkey_same(account_serum_asks->owner, account_serum_prog->key))
      return ERROR_INCORRECT_PROGRAM_ID;

    uint8_t* iter = account_serum_asks->data;
    uint64_t left = account_serum_asks->data_len;
    if (!verify_serum_padding(&iter, &left))
      return ERROR_INVALID_ACCOUNT_DATA;

    BUF_CAST(flags, serum_flags_t, iter, left);
    if (!flags->Initialized || !flags->Asks || flags->Disabled || flags->Reserved)
      return ERROR_INVALID_ACCOUNT_DATA;

    BUF_CAST(book, serum_book_t, iter, left);
    serum_node_any_t* nodes = (serum_node_any_t*) iter;

    uint64_t max_nodes = left / sizeof(serum_node_any_t);
    if (book->LeafCount > max_nodes)
      return ERROR_INVALID_ACCOUNT_DATA;

    if (book->LeafCount == 0) {
      trading = false;
    } else {
      uint32_t idx = book->Root;
      while (true) {
        if (idx > max_nodes)
          return ERROR_INVALID_ACCOUNT_DATA;
        serum_node_any_t* node = &nodes[idx];
        if (node->Tag == SERUM_NODE_TYPE_LEAF) {
          best_ask_lots = ((serum_node_leaf_t*) node)->Key1;
          break;
        } else if (node->Tag == SERUM_NODE_TYPE_INNER) {
          idx = ((serum_node_inner_t*) node)->ChildA; // Smaller prices to the left
        } else {
          return ERROR_INVALID_ACCOUNT_DATA;
        }
      }
    }

    sol_log("serum asks passed validation");
  }

  // Convert serum prices into pyth-format prices
  if (trading)
  {
    // Price has units of [QuoteLot/BaseLot]. We need to convert this to the units
    // that Pyth expects for this price account (based on the price exponent).
    //
    // 1 [QuoteLot] = QuoteLotSize [QuoteNative]
    // 1 [BaseLot]  = BaseLotSize  [BaseNative]
    //
    // 1 [QuoteNative] = 10^(PythExponent - QuoteNativeExponent) [Pyth]
    // 1 [BaseNative]  = 10^(PythExponent - BaseNativeExponent)  [Pyth]

    sol_log("**** BEST BID / BEST ASK / TRADING / BASE LOT SIZE / QUOTE LOT SIZE****");
    sol_log_64(best_bid_lots, best_ask_lots, trading, base_lot_size, quote_lot_size);

    //TODO: Make all these numbers come from real places
    uint8_t pyth_exponent = 9;

    if (quote_exponent > pyth_exponent || base_exponent > pyth_exponent)
      return ERROR_INVALID_ARGUMENT; //TODO: Support this

    if (pyth_exponent >= SOL_ARRAY_SIZE(TEN_TO_THE) ||
        /*abs*/(pyth_exponent - quote_exponent) >= SOL_ARRAY_SIZE(TEN_TO_THE) ||
        /*abs*/(pyth_exponent - base_exponent) >= SOL_ARRAY_SIZE(TEN_TO_THE))
      return ERROR_INVALID_ACCOUNT_DATA;

    //TODO: Do checked math here
    uint64_t pyth_scalar = TEN_TO_THE[pyth_exponent];
    uint64_t quote_lots_to_pyth = quote_lot_size * TEN_TO_THE[pyth_exponent - quote_exponent];
    uint64_t base_lots_to_pyth = base_lot_size * TEN_TO_THE[pyth_exponent - base_exponent];
    //uint64_t price_to_pyth =
    //  ((__uint128_t) quote_lots_to_pyth) *
    //  ((__uint128_t) pyth_scalar) /
    //  ((__uint128_t) base_lots_to_pyth);
    uint64_t price_to_pyth =
      ((uint64_t) quote_lots_to_pyth) *
      ((uint64_t) pyth_scalar) /
      ((uint64_t) base_lots_to_pyth);

    pyth_bid = best_bid_lots * price_to_pyth;
    pyth_ask = best_ask_lots * price_to_pyth;

    sol_log("**** BEST BID PYTH / BEST ASK PYTH****");
    sol_log_64(pyth_bid, pyth_ask, price_to_pyth, quote_lots_to_pyth, base_lots_to_pyth);
  }

  // Publish update to Pyth via cross-program invokation
  {
    // key[0] funding account       [signer writable]
    // key[1] price account         [writable]
    // key[2] param account         [readable]
    // key[3] sysvar_clock account  [readable]

    SolAccountInfo pyth_accounts[5] = {
      *account_payer,
      *account_pyth_price,
      *account_pyth_param,
      *account_sysvar_clock,
      *account_pyth_prog,
    };

    SolAccountMeta meta[4] = {
      {account_payer->key, true, true},
      {account_pyth_price->key, true, false},
      {account_pyth_param->key, false, false},
      {account_sysvar_clock->key, false, false},
    };

    cmd_upd_price_t cmd;
    cmd.ver_ = PC_VERSION;
    cmd.cmd_ = e_cmd_upd_price;
    cmd.status_ = (trading ? PC_STATUS_TRADING : PC_STATUS_UNKNOWN);
    cmd.unused_ = 0;
    cmd.price_ = (pyth_bid + pyth_ask) / 2;
    cmd.conf_ = (pyth_bid < pyth_ask) ? (pyth_ask - pyth_bid) : 0;
    cmd.pub_slot_ = clock->slot_;

    SolInstruction inst;
    inst.program_id = pyth_accounts[4].key;
    inst.accounts = meta;
    inst.account_len = SOL_ARRAY_SIZE(meta);
    inst.data = (uint8_t*) &cmd;
    inst.data_len = sizeof(cmd);

    return sol_invoke(&inst, pyth_accounts, SOL_ARRAY_SIZE(pyth_accounts));
  }

  return ERROR_INVALID_ARGUMENT; // Should not reach here
}
