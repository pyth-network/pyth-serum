#include <solana_sdk.h>

#include "oracle/oracle.h"
#include "serum-pyth.h"

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
extern uint64_t entrypoint(const uint8_t* input)
{
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

  sysvar_clock_t* clock; //TODO: Use direct syscall when it's supported

  uint8_t pyth_exponent = 0;
  uint8_t quote_exponent = 0;
  uint8_t base_exponent = 0;

  uint64_t base_lot_size = 0;
  uint64_t quote_lot_size = 0;

  bool trading = true;
  uint64_t serum_bid = 0;
  uint64_t serum_ask = 0;

  uint64_t pyth_bid = 0;
  uint64_t pyth_ask = 0;

  // Verify constraints on payer
  if (!account_payer->is_signer || !account_payer->is_writable)
    return ERROR_MISSING_REQUIRED_SIGNATURES;

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
  if (!account_pyth_prog->executable)
    return ERROR_INVALID_ARGUMENT;

  // Verify constraints on Serum progam ID
  if (!account_serum_prog->executable)
    return ERROR_INVALID_ARGUMENT;

  // Verify constraints on Pyth price account
  {
    if (!SolPubkey_same(account_pyth_price->owner, account_pyth_prog->key))
      return ERROR_INCORRECT_PROGRAM_ID;
    if (!account_pyth_price->is_writable)
      return ERROR_INVALID_ARGUMENT;
    if (account_pyth_price->data_len != sizeof(pc_price_t))
      return ERROR_ACCOUNT_DATA_TOO_SMALL;
    pc_price_t* price = (pc_price_t*) account_pyth_price->data;
    if (price->magic_ != PC_MAGIC ||
        price->ver_ != PC_VERSION ||
        price->type_ != PC_ACCTYPE_PRICE ||
        price->ptype_ != PC_PTYPE_PRICE)
      return ERROR_INVALID_ACCOUNT_DATA;
    pyth_exponent = -1 * price->expo_;
    if (pyth_exponent >= SOL_ARRAY_SIZE(TEN_TO_THE))
      return ERROR_INVALID_ACCOUNT_DATA;
  }

  // Verify constraints on SPL quote mint
  {
    if (!SolPubkey_same(account_spl_quote_mint->owner, &SPL_TOKEN_PROGRAM))
      return ERROR_INCORRECT_PROGRAM_ID;
    if (account_spl_quote_mint->data_len != sizeof(spl_mint_t))
      return ERROR_ACCOUNT_DATA_TOO_SMALL;
    quote_exponent = ((spl_mint_t*) account_spl_quote_mint->data)->Decimals;
    if (quote_exponent > pyth_exponent)
      return ERROR_INVALID_ACCOUNT_DATA; //TODO: Support this later
  }

  // Verify constraints on SPL base mint
  {
    if (!SolPubkey_same(account_spl_base_mint->owner, &SPL_TOKEN_PROGRAM))
      return ERROR_INCORRECT_PROGRAM_ID;
    if (account_spl_base_mint->data_len != sizeof(spl_mint_t))
      return ERROR_ACCOUNT_DATA_TOO_SMALL;
    base_exponent = ((spl_mint_t*) account_spl_base_mint->data)->Decimals;
    if (base_exponent > pyth_exponent)
      return ERROR_INVALID_ACCOUNT_DATA; //TODO: Support this later
  }

  // Verify constraints on Serum market
  {
    if (!SolPubkey_same(account_serum_market->owner, account_serum_prog->key))
      return ERROR_INCORRECT_PROGRAM_ID;

    uint8_t* iter = account_serum_market->data;
    uint64_t left = account_serum_market->data_len;
    if (!trim_serum_padding(&iter, &left))
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
  }

  // Verify constraints on Serum bids
  {
    if (!SolPubkey_same(account_serum_bids->owner, account_serum_prog->key))
      return ERROR_INCORRECT_PROGRAM_ID;

    uint8_t* iter = account_serum_bids->data;
    uint64_t left = account_serum_bids->data_len;
    if (!trim_serum_padding(&iter, &left))
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
        if (idx >= max_nodes)
          return ERROR_INVALID_ACCOUNT_DATA;
        serum_node_any_t* node = &nodes[idx];
        if (node->Tag == SERUM_NODE_TYPE_LEAF) {
          serum_bid = ((serum_node_leaf_t*) node)->Key1;
          break;
        } else if (node->Tag == SERUM_NODE_TYPE_INNER) {
          idx = ((serum_node_inner_t*) node)->ChildB; // Larger prices to the right
        } else {
          return ERROR_INVALID_ACCOUNT_DATA;
        }
      }
    }
  }

  // Verify constraints on Serum asks
  {
    if (!SolPubkey_same(account_serum_asks->owner, account_serum_prog->key))
      return ERROR_INCORRECT_PROGRAM_ID;

    uint8_t* iter = account_serum_asks->data;
    uint64_t left = account_serum_asks->data_len;
    if (!trim_serum_padding(&iter, &left))
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
        if (idx >= max_nodes)
          return ERROR_INVALID_ACCOUNT_DATA;
        serum_node_any_t* node = &nodes[idx];
        if (node->Tag == SERUM_NODE_TYPE_LEAF) {
          serum_ask = ((serum_node_leaf_t*) node)->Key1;
          break;
        } else if (node->Tag == SERUM_NODE_TYPE_INNER) {
          idx = ((serum_node_inner_t*) node)->ChildA; // Smaller prices to the left
        } else {
          return ERROR_INVALID_ACCOUNT_DATA;
        }
      }
    }
  }

  // Convert Serum prices into Pyth formatted prices
  if (trading)
  {
    // Serum prices have units of [QuoteLot/BaseLot]. We need to convert to the
    // units that Pyth expects for this price account (based on the expo_ field).
    //
    // 1 [QuoteLot] = QuoteLotSize [QuoteNative]
    // 1 [BaseLot]  = BaseLotSize  [BaseNative]
    //
    // 1 [QuoteNative] = 10^(PythExponent - QuoteNativeExponent) [Pyth]
    // 1 [BaseNative]  = 10^(PythExponent - BaseNativeExponent)  [Pyth]

    //TODO: Do checked math here. 128-bit for overflow?
    uint64_t pyth_scalar = TEN_TO_THE[pyth_exponent];
    uint64_t quote_lots_to_pyth = quote_lot_size * TEN_TO_THE[pyth_exponent - quote_exponent];
    uint64_t base_lots_to_pyth = base_lot_size * TEN_TO_THE[pyth_exponent - base_exponent];
    uint64_t serum_to_pyth =
      ((uint64_t) quote_lots_to_pyth) *
      ((uint64_t) pyth_scalar) /
      ((uint64_t) base_lots_to_pyth);

    pyth_bid = serum_bid * serum_to_pyth;
    pyth_ask = serum_ask * serum_to_pyth;
  }

  // Publish update to Pyth via cross-program invokation

  // key[0] funding account       [signer writable]
  // key[1] price account         [writable]
  // key[2] param account         [readable]
  // key[3] sysvar_clock account  [readable]
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
  cmd.conf_ = (pyth_bid < pyth_ask) ? ((pyth_ask - pyth_bid) / 2) : ((pyth_bid - pyth_ask) / 2);
  cmd.pub_slot_ = clock->slot_;

  SolInstruction inst;
  inst.program_id = account_pyth_prog->key;
  inst.accounts = meta;
  inst.account_len = SOL_ARRAY_SIZE(meta);
  inst.data = (uint8_t*) &cmd;
  inst.data_len = sizeof(cmd);

  return sol_invoke(&inst, input_accounts, SOL_ARRAY_SIZE(input_accounts));
}
