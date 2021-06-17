#include <solana_sdk.h>

typedef struct SerumAccountFlags
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
} SerumAccountFlags;
static_assert(sizeof(SerumAccountFlags) == 8, "incorrect size of SerumAccountFlags");

typedef struct MarketStateHeader
{
  SolPubkey OwnAddress;
  uint64_t VaultSignerNonce;
  SolPubkey BaseMint;
  SolPubkey QuoteMint;
  SolPubkey BaseVault;
  uint64_t BaseDepositsTotal;
  uint64_t BaseFeesAccrued;
  SolPubkey QuoteVault;
  uint64_t QuoteDepositsTotal;
  uint64_t QuoteFeesAccrued;
  uint64_t QuoteDustThreshold;
  SolPubkey RequestQueue;
  SolPubkey EventQueue;
  SolPubkey Bids;
  SolPubkey Asks;
  uint64_t BaseLotSize;
  uint64_t QuoteLotSize;
  uint64_t FeeRateBps;
  uint64_t ReferrerRebatesAccrued;
} MarketStateHeader;
static_assert(sizeof(MarketStateHeader) == 368, "incorrect size of MarketStateHeader");

typedef struct BookHeader
{
  uint64_t BumpIndex;
  uint64_t FreeListLen;
  uint32_t FreeListHead;
  uint32_t Root;
  uint64_t LeafCount;
} BookHeader;
static_assert(sizeof(BookHeader) == 32, "incorrect size of BookHeader");

enum NodeTag
{
  eUninitialized = 0,
  eInnerNode = 1,
  eLeafNode = 2,
  eFreeNode = 3,
  eLastFreeNode = 4
};

typedef struct InnerNode
{
  uint32_t Tag;
  uint32_t PrefixLen;
  uint64_t Key0;
  uint64_t Key1;
  uint32_t ChildA;
  uint32_t ChildB;
} InnerNode;
static_assert(sizeof(InnerNode) == 32, "incorrect size of InnerNode");

typedef struct LeafNode
{
  uint32_t Tag;
  uint8_t OwnerSlot;
  uint8_t FeeTier;
  uint8_t Padding[2];
  uint64_t Key0;
  uint64_t Key1;
  SolPubkey Owner;
  uint64_t Quantity;
  uint64_t ClientId;
} LeafNode;
static_assert(sizeof(LeafNode) == 72, "incorrect size of LeafNode");

typedef struct FreeNode
{
  uint32_t Tag;
  uint32_t Next;
} FreeNode;
static_assert(sizeof(FreeNode) == 8, "incorrect size of FreeNode");

typedef struct AnyNode
{
  uint32_t Tag;
  uint32_t Data[17];
} AnyNode;
static_assert(sizeof(AnyNode) == 72, "incorrect size of AnyNode");

extern uint64_t entrypoint(const uint8_t* input)
{
  sol_log("serum-pyth entrypoint");

  // 0) Payer               [signer]
  // 1) Serum ProgramID     []
  // 2) Serum Market        []
  // 3) Serum Bids          []
  // 4) Serum Asks          []

  SolAccountInfo inputAccounts[5];
  SolParameters inputParams;

  inputParams.ka = inputAccounts;

  if (!sol_deserialize(input, &inputParams, SOL_ARRAY_SIZE(inputAccounts))) {
    sol_log("ERR1");
    return ERROR_INVALID_ARGUMENT;
  }

  if (inputParams.ka_num != SOL_ARRAY_SIZE(inputAccounts)) {
    sol_log("ERR2");
    return ERROR_NOT_ENOUGH_ACCOUNT_KEYS;
  }

  const SolAccountInfo* accPayer       = inputAccounts + 0;
  const SolAccountInfo* accSerumPID    = inputAccounts + 1;
  const SolAccountInfo* accSerumMarket = inputAccounts + 2;
  const SolAccountInfo* accSerumBids   = inputAccounts + 3;
  const SolAccountInfo* accSerumAsks   = inputAccounts + 4;

  const MarketStateHeader* market = NULL;
  uint64_t bestBid = 0;
  uint64_t bestAsk = 0;

  // Verify stuff about Payer
  {
    //TODO
    sol_log("payer passed validation");
  }

  // Verify stuff about SerumPID
  {
    if (!accSerumPID->executable) {
      sol_log("ERR3");
      return ERROR_INVALID_ARGUMENT;
    }
    sol_log("serumPID passed validation");
  }

  // Verify stuff about SerumMarket
  {
    if (!SolPubkey_same(accSerumMarket->owner, accSerumPID->key)) {
      sol_log("ERR4");
      return ERROR_INVALID_ARGUMENT;
    }

    uint8_t* iter = accSerumMarket->data;
    uint64_t left = accSerumMarket->data_len;

    if (left < 5 || sol_memcmp(iter, "serum", 5) != 0) {
      sol_log("ERR7");
      return ERROR_INVALID_ARGUMENT;
    }
    iter += 5;
    left -= 5;

    if (left < 7 || sol_memcmp(iter + left - 7, "padding", 7) != 0) {
      sol_log("ERR8");
      return ERROR_INVALID_ARGUMENT;
    }
    left -= 7;

    if (left < sizeof(SerumAccountFlags)) {
      sol_log("ERR9");
      return ERROR_INVALID_ARGUMENT;
    }
    const SerumAccountFlags* flags = (const SerumAccountFlags*) iter;
    iter += sizeof(SerumAccountFlags);
    left -= sizeof(SerumAccountFlags);

    if (!flags->Initialized || !flags->Market || flags->Disabled || flags->Reserved) {
      sol_log("ERR10");
      return ERROR_INVALID_ARGUMENT;
    }

    if (left < sizeof(MarketStateHeader)) {
      sol_log("ERR11");
      return ERROR_INVALID_ARGUMENT;
    }
    market = (const MarketStateHeader*) iter;
    iter += sizeof(MarketStateHeader);
    left -= sizeof(MarketStateHeader);

    if (!SolPubkey_same(&market->OwnAddress, accSerumMarket->key)) {
      sol_log("ERR12");
      return ERROR_INVALID_ARGUMENT;
    }
    if (!SolPubkey_same(&market->Bids, accSerumBids->key)) {
      sol_log("ERR17");
      return ERROR_INVALID_ARGUMENT;
    }
    if (!SolPubkey_same(&market->Asks, accSerumAsks->key)) {
      sol_log("ERR18");
      return ERROR_INVALID_ARGUMENT;
    }

    sol_log("serumMarket passed validation");
  }

  // Verify stuff about SerumBids
  {
    if (!SolPubkey_same(accSerumBids->owner, accSerumPID->key)) {
      sol_log("ERR5");
      return ERROR_INVALID_ARGUMENT;
    }

    uint8_t* iter = accSerumBids->data;
    uint64_t left = accSerumBids->data_len;

    if (left < 5 || sol_memcmp(iter, "serum", 5) != 0) {
      sol_log("ERR13");
      return ERROR_INVALID_ARGUMENT;
    }
    iter += 5;
    left -= 5;

    if (left < 7 || sol_memcmp(iter + left - 7, "padding", 7) != 0) {
      sol_log("ERR14");
      return ERROR_INVALID_ARGUMENT;
    }
    left -= 7;

    if (left < sizeof(SerumAccountFlags)) {
      sol_log("ERR15");
      return ERROR_INVALID_ARGUMENT;
    }
    const SerumAccountFlags* flags = (const SerumAccountFlags*) iter;
    iter += sizeof(SerumAccountFlags);
    left -= sizeof(SerumAccountFlags);

    if (!flags->Initialized || !flags->Bids || flags->Disabled || flags->Reserved) {
      sol_log("ERR16");
      return ERROR_INVALID_ARGUMENT;
    }

    if (left < sizeof(BookHeader)) {
      sol_log("ERR23");
      return ERROR_INVALID_ARGUMENT;
    }
    const BookHeader* hdr = (const BookHeader*) iter;
    iter += sizeof(BookHeader);
    left -= sizeof(BookHeader);

    //TODO: Verify size of nodes and whatnot

    // Find maximum (best) bid
    const AnyNode* nodes = (const AnyNode*) iter;
    if (hdr->LeafCount > 0) {
      const AnyNode* node = &nodes[hdr->Root];
      while (true) {
        if (node->Tag == eLeafNode) {
          const LeafNode* leaf = (const LeafNode*) node;
          bestBid = leaf->Key1;
          break;
        }
        else if (node->Tag == eInnerNode) {
          const InnerNode* inner = (const InnerNode*) node;
          node = &nodes[inner->ChildB];
        }
        else {
          sol_log("ERR24");
          return ERROR_INVALID_ARGUMENT;
        }
      }
    }

    sol_log("serumBids passed validation");
  }

  // Verify stuff about SerumAsks
  {
    if (!SolPubkey_same(accSerumAsks->owner, accSerumPID->key)) {
      sol_log("ERR6");
      return ERROR_INVALID_ARGUMENT;
    }

    uint8_t* iter = accSerumAsks->data;
    uint64_t left = accSerumAsks->data_len;

    if (left < 5 || sol_memcmp(iter, "serum", 5) != 0) {
      sol_log("ERR19");
      return ERROR_INVALID_ARGUMENT;
    }
    iter += 5;
    left -= 5;

    if (left < 7 || sol_memcmp(iter + left - 7, "padding", 7) != 0) {
      sol_log("ERR20");
      return ERROR_INVALID_ARGUMENT;
    }
    left -= 7;

    if (left < sizeof(SerumAccountFlags)) {
      sol_log("ERR21");
      return ERROR_INVALID_ARGUMENT;
    }
    const SerumAccountFlags* flags = (const SerumAccountFlags*) iter;
    iter += sizeof(SerumAccountFlags);
    left -= sizeof(SerumAccountFlags);

    if (!flags->Initialized || !flags->Asks || flags->Disabled || flags->Reserved) {
      sol_log("ERR22");
      return ERROR_INVALID_ARGUMENT;
    }

    if (left < sizeof(BookHeader)) {
      sol_log("ERR26");
      return ERROR_INVALID_ARGUMENT;
    }
    const BookHeader* hdr = (const BookHeader*) iter;
    iter += sizeof(BookHeader);
    left -= sizeof(BookHeader);

    //TODO: Verify size of nodes and whatnot

    // Find minimum (best) ask
    const AnyNode* nodes = (const AnyNode*) iter;
    if (hdr->LeafCount > 0) {
      const AnyNode* node = &nodes[hdr->Root];
      while (true) {
        if (node->Tag == eLeafNode) {
          const LeafNode* leaf = (const LeafNode*) node;
          bestAsk = leaf->Key1;
          break;
        }
        else if (node->Tag == eInnerNode) {
          const InnerNode* inner = (const InnerNode*) node;
          node = &nodes[inner->ChildA];
        }
        else {
          sol_log("ERR25");
          return ERROR_INVALID_ARGUMENT;
        }
      }
    }

    sol_log("serumAsks passed validation");
  }

  sol_log("**** BEST BID / BEST ASK ****");
  sol_log_64(bestBid, bestAsk, 0, 0, 0);

  /*
  sol_log("**** this programId:");
  sol_log_pubkey(inputParams.program_id);
  sol_log("\n");

  sol_log("**** number of accounts, data len");
  sol_log_64(inputParams.ka_num, inputParams.data_len, 0, 0, 0);
  sol_log("\n");

  for (uint64_t i = 0; i < inputParams.ka_num; ++i) {
    const SolAccountInfo* acc = &(inputAccounts[i]);
    sol_log("**** account index, data_len, is_sign, is_write, executable");
    sol_log_64(i, acc->data_len, acc->is_signer, acc->is_writable, acc->executable);
    sol_log("**** account pubkey");
    sol_log_pubkey(acc->key);
    sol_log("**** account program owner");
    sol_log_pubkey(acc->owner);
    sol_log("\n\n");
  }
  */

  return SUCCESS;
}
