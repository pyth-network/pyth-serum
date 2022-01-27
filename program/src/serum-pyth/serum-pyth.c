#include <serum-pyth/serum-pyth.h>
#include <oracle/oracle.h>

// This program takes a single instruction with no binary data
// and the following accounts as parameters:
enum
{
  SP_ACC_PAYER,         // [signer,writeable]
  SP_ACC_PYTH_PRICE,    // [writeable]
  SP_ACC_SERUM_PROG,    // []
  SP_ACC_SERUM_MARKET,  // []
  SP_ACC_SERUM_BIDS,    // []
  SP_ACC_SERUM_ASKS,    // []
  SP_ACC_QUOTE_MINT,    // []
  SP_ACC_BASE_MINT,     // []
  SP_ACC_SYSVAR_CLOCK,  // []
  SP_ACC_PYTH_PROG,     // []

  SP_NUM_ACCOUNTS
};
static_assert( SP_NUM_ACCOUNTS == 10, "" );

// Account metadata for invoking pyth-client:
enum
{
  SP_META_PAYER,         // [signer writable]
  SP_META_PYTH_PRICE,    // [writeable]
  SP_META_SYSVAR_CLOCK,  // []

  SP_NUM_META
};
static_assert( SP_NUM_META == 3, "" );

typedef struct
{
  SolAccountInfo accounts[ SP_NUM_ACCOUNTS ];
} sp_program_input_t;

typedef struct
{
  SolInstruction inst;
  SolAccountMeta meta[ SP_NUM_META ];
  cmd_upd_price_t cmd;
} sp_pyth_instruction_t;

#define BUF_CAST( name, type, buf_ptr, buf_size ) \
  if ( SP_UNLIKELY( ( buf_size ) < sizeof( type ) ) ) { \
    return ERROR_ACCOUNT_DATA_TOO_SMALL; \
  } \
  const type* const ( name ) = ( const type* ) ( buf_ptr ); \
  ( buf_ptr ) += sizeof( type ); \
  ( buf_size ) -= sizeof( type )

static inline sp_errcode_t sp_get_pyth_instruction(
  const sp_program_input_t* const input,
  sp_pyth_instruction_t* const output
) {

  const SolAccountInfo
    *const account_payer          = &input->accounts[ SP_ACC_PAYER ],
    *const account_pyth_price     = &input->accounts[ SP_ACC_PYTH_PRICE ],
    *const account_serum_prog     = &input->accounts[ SP_ACC_SERUM_PROG ],
    *const account_serum_market   = &input->accounts[ SP_ACC_SERUM_MARKET ],
    *const account_serum_bids     = &input->accounts[ SP_ACC_SERUM_BIDS ],
    *const account_serum_asks     = &input->accounts[ SP_ACC_SERUM_ASKS ],
    *const account_spl_quote_mint = &input->accounts[ SP_ACC_QUOTE_MINT ],
    *const account_spl_base_mint  = &input->accounts[ SP_ACC_BASE_MINT ],
    *const account_sysvar_clock   = &input->accounts[ SP_ACC_SYSVAR_CLOCK ],
    *const account_pyth_prog      = &input->accounts[ SP_ACC_PYTH_PROG ];

  bool trading = true;

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
  }

  // Verify constraints on Pyth program ID
  if (!account_pyth_prog->executable)
    return ERROR_INVALID_ARGUMENT;

  // Verify constraints on Serum progam ID
  if (!account_serum_prog->executable)
    return ERROR_INVALID_ARGUMENT;

  // Verify constraints on Pyth price account
  sp_expo_t pyth_exponent;
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
  }

  // Verify constraints on SPL quote mint
  sp_expo_t quote_exponent;
  {
    if (!SolPubkey_same(account_spl_quote_mint->owner, &SPL_TOKEN_PROGRAM))
      return ERROR_INCORRECT_PROGRAM_ID;
    if (account_spl_quote_mint->data_len != sizeof(spl_mint_t))
      return ERROR_ACCOUNT_DATA_TOO_SMALL;
    quote_exponent = ((spl_mint_t*) account_spl_quote_mint->data)->Decimals;
  }

  // Verify constraints on SPL base mint
  sp_expo_t base_exponent;
  {
    if (!SolPubkey_same(account_spl_base_mint->owner, &SPL_TOKEN_PROGRAM))
      return ERROR_INCORRECT_PROGRAM_ID;
    if (account_spl_base_mint->data_len != sizeof(spl_mint_t))
      return ERROR_ACCOUNT_DATA_TOO_SMALL;
    base_exponent = ((spl_mint_t*) account_spl_base_mint->data)->Decimals;
  }

  // Verify constraints on Serum market
  sp_size_t base_lot_size;
  sp_size_t quote_lot_size;
  {
    if (!SolPubkey_same(account_serum_market->owner, account_serum_prog->key))
      return ERROR_INCORRECT_PROGRAM_ID;

    uint8_t* iter = account_serum_market->data;
    uint64_t left = account_serum_market->data_len;
    if (!trim_serum_padding(&iter, &left))
      return ERROR_INVALID_ACCOUNT_DATA;

    BUF_CAST(flags, serum_flags_t, iter, left);
    if (!sp_flags_valid(flags, flags->Market))
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
  sp_size_t serum_bid = 0;
  {
    if (!SolPubkey_same(account_serum_bids->owner, account_serum_prog->key))
      return ERROR_INCORRECT_PROGRAM_ID;

    uint8_t* iter = account_serum_bids->data;
    uint64_t left = account_serum_bids->data_len;
    if (!trim_serum_padding(&iter, &left))
      return ERROR_INVALID_ACCOUNT_DATA;

    BUF_CAST(flags, serum_flags_t, iter, left);
    if (!sp_flags_valid(flags, flags->Bids))
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
  sp_size_t serum_ask = 0;
  {
    if (!SolPubkey_same(account_serum_asks->owner, account_serum_prog->key))
      return ERROR_INCORRECT_PROGRAM_ID;

    uint8_t* iter = account_serum_asks->data;
    uint64_t left = account_serum_asks->data_len;
    if (!trim_serum_padding(&iter, &left))
      return ERROR_INVALID_ACCOUNT_DATA;

    BUF_CAST(flags, serum_flags_t, iter, left);
    if (!sp_flags_valid(flags, flags->Asks))
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
  int64_t pyth_price = 0;
  uint64_t pyth_conf = 0;
  if ( SP_LIKELY( trading ) ) {
    const sp_size_t serum_to_pyth = sp_serum_to_pyth(
      pyth_exponent,
      quote_exponent,
      base_exponent,
      quote_lot_size,
      base_lot_size
    );

    if ( SP_UNLIKELY( serum_to_pyth == SP_SIZE_OVERFLOW ) ) {
      return ERROR_INVALID_ACCOUNT_DATA;
    }

    sp_size_t pyth_bid = serum_bid * serum_to_pyth;
    sp_size_t pyth_ask = serum_ask * serum_to_pyth;
    pyth_price = ( int64_t ) sp_midpt( pyth_bid, pyth_ask );
    pyth_conf = sp_confidence( pyth_bid, pyth_ask );

    // status will be unknown unless the spread is sufficiently tight.
    int64_t threshold_conf = (pyth_price / PRICE_CONF_THRESHOLD);
    if (threshold_conf < 0) {
      // Safe as long as threshold_conf isn't the min int64, which it isn't as long as PRICE_CONF_THRESHOLD > 1.
      threshold_conf = -threshold_conf;
    }
    if ( pyth_conf > threshold_conf ) {
      trading = false;
    }
  }

  // Prepare pyth-client instruction for cross-program invocation.
  cmd_upd_price_t* const cmd = &output->cmd;
  cmd->ver_ = PC_VERSION;
  cmd->cmd_ = e_cmd_upd_price;
  cmd->status_ = ( trading ? PC_STATUS_TRADING : PC_STATUS_UNKNOWN );
  cmd->unused_ = 0;
  cmd->price_ = pyth_price;
  cmd->conf_ = pyth_conf;
  cmd->pub_slot_ = (  // TODO: Use direct syscall.
    ( const sysvar_clock_t* ) account_sysvar_clock->data
  )->slot_;

  {
    SolAccountMeta *const payer_meta = &output->meta[ SP_META_PAYER ];
    payer_meta->pubkey = account_payer->key;
    payer_meta->is_writable = true;
    payer_meta->is_signer = true;
  }
  {
    SolAccountMeta *const price_meta = &output->meta[ SP_META_PYTH_PRICE ];
    price_meta->pubkey = account_pyth_price->key;
    price_meta->is_writable = true;
    price_meta->is_signer = false;
  }
  {
    SolAccountMeta *const clock_meta = &output->meta[ SP_META_SYSVAR_CLOCK ];
    clock_meta->pubkey = account_sysvar_clock->key;
    clock_meta->is_writable = false;
    clock_meta->is_signer = false;
  }
  {
    SolInstruction* const inst = &output->inst;
    inst->program_id = account_pyth_prog->key;
    inst->accounts = output->meta;
    inst->account_len = SP_NUM_META;
    inst->data = ( uint8_t* ) cmd;
    inst->data_len = sizeof( *cmd );
  }

  return SP_NO_ERROR;
}

SP_UNUSED
extern sp_errcode_t entrypoint( const uint8_t* const buf )
{
  sp_program_input_t input;
  SolParameters params;
  params.ka = input.accounts;

  const bool valid = sol_deserialize( buf, &params, SP_NUM_ACCOUNTS );
  if ( SP_UNLIKELY( ! valid ) ) {
    return ERROR_INVALID_ARGUMENT;
  }
  if ( SP_UNLIKELY( params.ka_num != SP_NUM_ACCOUNTS ) ) {
    return ERROR_NOT_ENOUGH_ACCOUNT_KEYS;
  }

  sp_pyth_instruction_t inst;
  const sp_errcode_t err = sp_get_pyth_instruction( &input, &inst );
  if ( SP_UNLIKELY( err != SP_NO_ERROR ) ) {
    return err;
  }

  return sol_invoke(
    &inst.inst,
    input.accounts,
    SP_NUM_ACCOUNTS
  );
}
