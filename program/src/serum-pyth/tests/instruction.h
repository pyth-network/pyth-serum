#pragma once

#include <serum-pyth/serum-pyth.h>
#include <serum-pyth/tests/assert.h>

// Aligned with errors in solana_sdk.h
static const char* SP_SOL_ERR_MSGS[] = {
  "SUCCESS",
  "CUSTOM_ZERO",
  "INVALID_ARGUMENT",
  "INVALID_INSTRUCTION_DATA",
  "INVALID_ACCOUNT_DATA",
  "ACCOUNT_DATA_TOO_SMALL",
  "INSUFFICIENT_FUNDS",
  "INCORRECT_PROGRAM_ID",
  "MISSING_REQUIRED_SIGNATURES",
  "ACCOUNT_ALREADY_INITIALIZED",
  "UNINITIALIZED_ACCOUNT",
  "NOT_ENOUGH_ACCOUNT_KEYS",
  "ACCOUNT_BORROW_FAILED",
  "MAX_SEED_LENGTH_EXCEEDED",
  "INVALID_SEEDS",
};

// Inverse of TO_BUILTIN(), for reading error codes in assertions.
// sp_error_idx( ERROR_INVALID_ARGUMENT ) == 2 -> "INVALID_ARGUMENT"
static inline uint64_t sp_error_idx( const sp_errcode_t err )
{
  return ( err >> 32 ) & ( ( 1ul << 32 ) - 1 );
}

static inline const char* sp_error_msg( const sp_errcode_t err )
{
  const uint64_t idx = sp_error_idx( err );
  return (
    idx < SOL_ARRAY_SIZE( SP_SOL_ERR_MSGS )
    ? SP_SOL_ERR_MSGS[ idx ]
    : "UNKNOWN_ERROR"
  );
}

typedef uint8_t sp_market_buf_t[
  SERUM_HEADER_LEN
  + sizeof( serum_flags_t )
  + sizeof( serum_market_t )
  + SERUM_FOOTER_LEN
];

typedef uint8_t sp_book_buf_t[
  SERUM_HEADER_LEN
  + sizeof( serum_flags_t )
  + sizeof( serum_book_t )
  + sizeof( serum_node_any_t ) * 2  // inner + leaf
  + SERUM_FOOTER_LEN
];

typedef struct
{
  sp_program_input_t prog_input;
  SolPubkey keys[ SP_NUM_ACCOUNTS ];

  SolPubkey token_prog;
  sysvar_clock_t sys_clock;

  pc_price_t pyth_price;
  spl_mint_t quote_mint;
  spl_mint_t base_mint;

  sp_market_buf_t mkt_buf;
  sp_book_buf_t bid_buf;
  sp_book_buf_t ask_buf;

  serum_flags_t* mkt_flags;
  serum_flags_t* bid_flags;
  serum_flags_t* ask_flags;

  serum_market_t* market;
  serum_book_t* bid_book;
  serum_book_t* ask_book;

  serum_node_inner_t* bid_inner;
  serum_node_inner_t* ask_inner;

  serum_node_leaf_t* bid_leaf;
  serum_node_leaf_t* ask_leaf;

} sp_test_input_t;

#define SP_MEMCPY_SIZEOF( dst, src ) sol_memcpy( dst, src, sizeof( *( dst ) ) )
#define SP_MEMSET_SIZEOF( dst, val ) sol_memset( dst, val, sizeof( *( dst ) ) )

static serum_flags_t*
sp_init_serum_buf( uint8_t* const buf, const uint64_t len )
{
  sol_memcpy( buf, SERUM_HEADER, SERUM_HEADER_LEN );
  sp_assert( sp_has_serum_header( buf, len ) );

  sol_memcpy( buf + len - SERUM_FOOTER_LEN, SERUM_FOOTER, SERUM_FOOTER_LEN );
  sp_assert( sp_has_serum_footer( buf, len ) );

  uint8_t* iter = buf;
  uint64_t left = len;
  sp_assert( trim_serum_padding( &iter, &left ) );
  sp_assert_ptr( iter, buf + SERUM_HEADER_LEN );
  sp_assert_u64( left, len - SERUM_HEADER_LEN - SERUM_FOOTER_LEN );

  serum_flags_t* const flags = ( serum_flags_t* )( iter );
  SP_MEMSET_SIZEOF( flags, 0 );
  flags->Initialized = 1;
  sp_assert( sp_flags_valid( flags, flags->Initialized ) );

  return flags;
}

static void sp_set_bid_ask(
  sp_test_input_t* const input,
  const sp_size_t bid,
  const sp_size_t ask
) {
  input->bid_leaf->Key1 = bid;
  input->ask_leaf->Key1 = ask;
}

static void sp_set_pyth_expo( sp_test_input_t* const input, const sp_expo_t e )
{
  input->pyth_price.expo_ = -e;
}

static void sp_set_quote_expo( sp_test_input_t* const input, const sp_expo_t e )
{
  input->quote_mint.Decimals = ( uint8_t )( e );
}

static void sp_set_base_expo( sp_test_input_t* const input, const sp_expo_t e )
{
  input->base_mint.Decimals = ( uint8_t )( e );
}

static void sp_set_quote_lot( sp_test_input_t* const input, const sp_size_t l )
{
  input->market->QuoteLotSize = l;
}

static void sp_set_base_lot( sp_test_input_t* const input, const sp_size_t l )
{
  input->market->BaseLotSize = l;
}

static void sp_init_test_input( sp_test_input_t* const input )
{
  SP_MEMSET_SIZEOF( input, 1234 );

  SolAccountInfo* const accounts = input->prog_input.accounts;
  SolPubkey* const keys = input->keys;

  for ( int i = 0; i < SP_NUM_ACCOUNTS; ++i ) {
    SP_MEMSET_SIZEOF( &keys[ i ], i );
    accounts[ i ].key = &keys[ i ];
    accounts[ i ].is_signer = false;
    accounts[ i ].is_writable = false;
    accounts[ i ].lamports = NULL; // unused
    accounts[ i ].rent_epoch = 0;  // unused
  }

  accounts[ SP_ACC_SYSVAR_CLOCK ].data = ( uint8_t* )( &input->sys_clock );
  accounts[ SP_ACC_SYSVAR_CLOCK ].data_len = sizeof( input->sys_clock );
  sol_memcpy(
    keys[ SP_ACC_SYSVAR_CLOCK ].x,
    sysvar_clock,
    sizeof( keys[ SP_ACC_SYSVAR_CLOCK ].x )
  );

  accounts[ SP_ACC_PAYER ].is_signer = true;
  accounts[ SP_ACC_PAYER ].is_writable = true;

  accounts[ SP_ACC_PYTH_PROG ].executable = true;
  accounts[ SP_ACC_SERUM_PROG ].executable = true;

  accounts[ SP_ACC_PYTH_PRICE ].is_writable = true;
  accounts[ SP_ACC_PYTH_PRICE ].owner = &keys[ SP_ACC_PYTH_PROG ];
  accounts[ SP_ACC_PYTH_PRICE ].data = ( uint8_t* )( &input->pyth_price );
  accounts[ SP_ACC_PYTH_PRICE ].data_len = sizeof( input->pyth_price );

  accounts[ SP_ACC_SERUM_MARKET ].owner = &keys[ SP_ACC_SERUM_PROG ];
  accounts[ SP_ACC_SERUM_MARKET ].data = input->mkt_buf;
  accounts[ SP_ACC_SERUM_MARKET ].data_len = sizeof( input->mkt_buf );

  accounts[ SP_ACC_SERUM_BIDS ].owner = &keys[ SP_ACC_SERUM_PROG ];
  accounts[ SP_ACC_SERUM_BIDS ].data = input->bid_buf;
  accounts[ SP_ACC_SERUM_BIDS ].data_len = sizeof( input->bid_buf );

  accounts[ SP_ACC_SERUM_ASKS ].owner = &keys[ SP_ACC_SERUM_PROG ];
  accounts[ SP_ACC_SERUM_ASKS ].data = input->ask_buf;
  accounts[ SP_ACC_SERUM_ASKS ].data_len = sizeof( input->ask_buf );

  input->token_prog = SPL_TOKEN_PROGRAM;
  accounts[ SP_ACC_QUOTE_MINT ].owner = &input->token_prog;
  accounts[ SP_ACC_QUOTE_MINT ].data = ( uint8_t* )( &input->quote_mint );
  accounts[ SP_ACC_QUOTE_MINT ].data_len = sizeof( input->quote_mint );

  accounts[ SP_ACC_BASE_MINT ].owner = &input->token_prog;
  accounts[ SP_ACC_BASE_MINT ].data = ( uint8_t* )( &input->base_mint );
  accounts[ SP_ACC_BASE_MINT ].data_len = sizeof( input->base_mint );

  input->mkt_flags = sp_init_serum_buf( input->mkt_buf, sizeof( input->mkt_buf ) );
  input->bid_flags = sp_init_serum_buf( input->bid_buf, sizeof( input->bid_buf ) );
  input->ask_flags = sp_init_serum_buf( input->ask_buf, sizeof( input->ask_buf ) );

  input->market = ( serum_market_t* )( input->mkt_flags + 1 );
  SP_MEMCPY_SIZEOF( &input->market->OwnAddress, &keys[ SP_ACC_SERUM_MARKET ] );
  SP_MEMCPY_SIZEOF( &input->market->QuoteMint, &keys[ SP_ACC_QUOTE_MINT ] );
  SP_MEMCPY_SIZEOF( &input->market->BaseMint, &keys[ SP_ACC_BASE_MINT ] );
  SP_MEMCPY_SIZEOF( &input->market->Bids, &keys[ SP_ACC_SERUM_BIDS ] );
  SP_MEMCPY_SIZEOF( &input->market->Asks, &keys[ SP_ACC_SERUM_ASKS ] );

  input->bid_book = ( serum_book_t * )( input->bid_flags + 1 );
  input->ask_book = ( serum_book_t * )( input->ask_flags + 1 );

  input->bid_inner = ( serum_node_inner_t* )( input->bid_book + 1 );
  input->ask_inner = ( serum_node_inner_t* )( input->ask_book + 1 );

  input->bid_leaf = ( ( serum_node_leaf_t* ) input->bid_inner ) + 1;
  input->ask_leaf = ( ( serum_node_leaf_t* ) input->ask_inner ) + 1;

  const uint8_t* const bid_footer = ( uint8_t* )( input->bid_leaf + 1 );
  const uint8_t* const ask_footer = ( uint8_t* )( input->ask_leaf + 1 );
  const uint8_t* const bid_end = input->bid_buf + sizeof( input->bid_buf );
  const uint8_t* const ask_end = input->ask_buf + sizeof( input->ask_buf );
  sp_assert_ptr( bid_footer + SERUM_FOOTER_LEN, bid_end );
  sp_assert_ptr( ask_footer + SERUM_FOOTER_LEN, ask_end );

  input->mkt_flags->Market = 1;
  input->bid_flags->Bids = 1;
  input->ask_flags->Asks = 1;

  input->bid_book->Root = 0;
  input->ask_book->Root = 0;

  input->bid_book->LeafCount = 1;
  input->ask_book->LeafCount = 1;

  input->bid_inner->Tag = SERUM_NODE_TYPE_INNER;
  input->ask_inner->Tag = SERUM_NODE_TYPE_INNER;

  input->bid_leaf->Tag = SERUM_NODE_TYPE_LEAF;
  input->ask_leaf->Tag = SERUM_NODE_TYPE_LEAF;

  input->bid_inner->ChildA = UINT32_MAX;
  input->bid_inner->ChildB = 1;
  input->ask_inner->ChildA = 1;
  input->ask_inner->ChildB = UINT32_MAX;

  input->pyth_price.magic_ = PC_MAGIC;
  input->pyth_price.ver_ = PC_VERSION;
  input->pyth_price.type_ = PC_ACCTYPE_PRICE;
  input->pyth_price.ptype_ = PC_PTYPE_PRICE;

  sp_set_bid_ask( input, 1, 1 );
  sp_set_pyth_expo( input, 0 );
  sp_set_quote_expo( input, 0 );
  sp_set_base_expo( input, 0 );
  sp_set_quote_lot( input, 1 );
  sp_set_base_lot( input, 1 );
}

static sp_errcode_t sp_get_test_instruction(
  const sp_test_input_t* const input,
  sp_pyth_instruction_t* const inst
) {
  SP_MEMSET_SIZEOF( inst, 3456 );
  inst->cmd.price_ = 3456;
  inst->cmd.conf_ = 3456;
  return sp_get_pyth_instruction( &( input )->prog_input, inst );
}

#define sp_assert_err( input, inst, err ) sp_assert_eq( \
  sp_get_test_instruction( input, inst ), \
  err, \
  "%s == %s", \
  sp_error_msg( sp_get_test_instruction( input, inst ) ), \
  sp_error_msg( err ) \
)

#define sp_assert_any_err( input, inst ) sp_assert_ne( \
  sp_get_test_instruction( input, inst ), \
  SP_NO_ERROR, \
  "%s != %s", \
  sp_error_msg( sp_get_test_instruction( input, inst ) ), \
  sp_error_msg( SP_NO_ERROR ) \
)

#define sp_assert_no_err( input, inst ) \
  sp_assert_err( input, inst, SP_NO_ERROR )

static void sp_test_pyth_instruction()
{
  sp_test_input_t input;
  sp_init_test_input( &input );

  sp_pyth_instruction_t inst;
  sp_assert_no_err( &input, &inst );

  sp_assert_ptr( inst.inst.accounts, inst.meta );
  sp_assert_ptr( inst.inst.account_len, SP_NUM_META );

  SolAccountMeta* const meta_payer = &inst.meta[ SP_META_PAYER ];
  SolAccountMeta* const meta_price = &inst.meta[ SP_META_PYTH_PRICE ];
  SolAccountMeta* const meta_clock = &inst.meta[ SP_META_SYSVAR_CLOCK ];

  sp_assert( meta_payer->is_writable );
  sp_assert( meta_payer->is_signer );
  sp_assert( meta_price->is_writable );

  sp_assert( ! meta_price->is_signer );
  sp_assert( ! meta_clock->is_writable );
  sp_assert( ! meta_clock->is_signer );

  sp_assert_ptr( meta_payer->pubkey, &input.keys[ SP_ACC_PAYER ] );
  sp_assert_ptr( meta_price->pubkey, &input.keys[ SP_ACC_PYTH_PRICE ] );
  sp_assert_ptr( meta_clock->pubkey, &input.keys[ SP_ACC_SYSVAR_CLOCK ] );

  sp_assert_u32( inst.cmd.ver_, PC_VERSION );
  sp_assert_i32( inst.cmd.cmd_, e_cmd_upd_price );
  sp_assert_u32( inst.cmd.status_, PC_STATUS_TRADING );
  sp_assert_u32( inst.cmd.unused_, 0 );
  sp_assert_u64( inst.cmd.pub_slot_, input.sys_clock.slot_ );

  SolPubkey bad_key;
  SP_MEMSET_SIZEOF( &bad_key, 5678 );

  for ( unsigned i = 0; i < SP_NUM_ACCOUNTS; ++i ) {
    SolAccountInfo* const acc = &input.prog_input.accounts[ i ];

    if ( i == SP_ACC_PAYER ) {
      acc->is_signer = false;
      sp_assert_err( &input, &inst, ERROR_MISSING_REQUIRED_SIGNATURES );
      acc->is_signer = true;
      sp_assert_no_err( &input, &inst );
    }

    else if ( i == SP_ACC_PYTH_PROG || i == SP_ACC_SERUM_PROG ) {
      acc->executable = false;
      sp_assert_err( &input, &inst, ERROR_INVALID_ARGUMENT );
      acc->executable = true;
      sp_assert_no_err( &input, &inst );
    }

    else {
      acc->data_len -= 1;
      sp_assert_any_err( &input, &inst );
      acc->data_len += 1;
      sp_assert_no_err( &input, &inst );

      if ( i == SP_ACC_SYSVAR_CLOCK ) {
        acc->key = &bad_key;
        sp_assert_err( &input, &inst, ERROR_INVALID_ARGUMENT );
        acc->key = &input.keys[ i ];
        sp_assert_no_err( &input, &inst );
      }

      else {
        SolPubkey* const owner = acc->owner;
        acc->owner = &bad_key;
        sp_assert_any_err( &input, &inst );
        acc->owner = owner;
        sp_assert_no_err( &input, &inst );
      }
    }
  }
}
