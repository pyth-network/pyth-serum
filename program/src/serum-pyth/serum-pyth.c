#include <solana_sdk.h>

#define SERUM_NODE_TYPE_INNER 1
#define SERUM_NODE_TYPE_LEAF  2

typedef struct serum_flags
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

typedef struct serum_market
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

typedef struct serum_book
{
  uint64_t BumpIndex;
  uint64_t FreeListLen;
  uint32_t FreeListHead;
  uint32_t Root;
  uint64_t LeafCount;
} serum_book_t;
static_assert(sizeof(serum_book_t) == 32, "incorrect size of serum_book_t");

typedef struct serum_node_inner
{
  uint32_t Tag;
  uint32_t PrefixLen;
  uint64_t Key0;
  uint64_t Key1;
  uint32_t ChildA;
  uint32_t ChildB;
} serum_node_inner_t;
static_assert(sizeof(serum_node_inner_t) == 32, "incorrect size of serum_node_inner_t");

typedef struct serum_node_leaf
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

typedef struct serum_node_any
{
  uint32_t Tag;
  uint32_t Data[17];
} serum_node_any_t;
static_assert(sizeof(serum_node_any_t) == 72, "incorrect size of serum_node_any_t");

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

#define BUF_CAST(n, t, i, l) \
  if (l < sizeof(t)) { return ERROR_ACCOUNT_DATA_TOO_SMALL; } \
  const t* n = (const t*) i; \
  i += sizeof(t); \
  l -= sizeof(t);

extern uint64_t entrypoint(const uint8_t* input)
{
  // This program takes a single instruction, which has no binary data, and
  // expects the following accounts as parameters:
  // 0) Payer               [signer,writeable]
  // 1) Serum ProgramID     []
  // 2) Serum Market        []
  // 3) Serum Bids          []
  // 4) Serum Asks          []

  SolAccountInfo input_accounts[5];
  SolParameters input_params;
  input_params.ka = input_accounts;
  if (!sol_deserialize(input, &input_params, SOL_ARRAY_SIZE(input_accounts)))
    return ERROR_INVALID_ARGUMENT;
  if (input_params.ka_num != SOL_ARRAY_SIZE(input_accounts))
    return ERROR_NOT_ENOUGH_ACCOUNT_KEYS;

  /*
  sol_log("**** this programId:");
  sol_log_pubkey(input_params.program_id);
  sol_log("\n");

  sol_log("**** number of accounts, data len");
  sol_log_64(input_params.ka_num, input_params.data_len, 0, 0, 0);
  sol_log("\n");

  for (uint64_t i = 0; i < input_params.ka_num; ++i) {
    const SolAccountInfo* acc = &(input_accounts[i]);
    sol_log("**** account index, data_len, is_sign, is_write, executable");
    sol_log_64(i, acc->data_len, acc->is_signer, acc->is_writable, acc->executable);
    sol_log("**** account pubkey");
    sol_log_pubkey(acc->key);
    sol_log("**** account program owner");
    sol_log_pubkey(acc->owner);
    sol_log("\n\n");
  }
  */

  const SolAccountInfo* account_payer        = input_accounts + 0;
  const SolAccountInfo* account_serum_prog   = input_accounts + 1;
  const SolAccountInfo* account_serum_market = input_accounts + 2;
  const SolAccountInfo* account_serum_bids   = input_accounts + 3;
  const SolAccountInfo* account_serum_asks   = input_accounts + 4;

  bool trading = true;
  uint64_t best_bid_lots = 0;
  uint64_t best_ask_lots = 0;

  // Verify constraints on payer
  {
    //TODO
    sol_log("payer passed validation");
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
        !SolPubkey_same(&market->Bids, account_serum_bids->key) ||
        !SolPubkey_same(&market->Asks, account_serum_asks->key))
      return ERROR_INVALID_ACCOUNT_DATA;

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

  sol_log("**** BEST BID / BEST ASK / TRADING ****");
  sol_log_64(best_bid_lots, best_ask_lots, trading, 0, 0);

  return SUCCESS;
}
