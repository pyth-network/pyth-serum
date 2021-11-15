// Set PC_HEAP_START before including oracle.h.
char heap_start[ 8192 ];
#define PC_HEAP_START ( heap_start )

#include "serum-pyth.c" // NOLINT(bugprone-suspicious-include)
#include <criterion/criterion.h>

// Turn off to stop test suites on first failure.
#define SP_ASSERT_ALL 1

#if SP_ASSERT_ALL
#define sp_test_eq cr_expect_eq
#define sp_test_ne cr_expect_neq
#else
#define sp_test_eq cr_assert_eq
#define sp_test_ne cr_assert_neq
#endif

// Assert pyth-client allocations use test heap.
Test( serum_pyth, heap_start ) {

  sp_test_eq( PC_HEAP_START, heap_start );
  sp_test_ne( PC_HEAP_START, NULL );
  sp_test_ne( ( uint64_t ) PC_HEAP_START, HEAP_START_ADDRESS );

}

#define sp_test_conf( bid, ask, expected ) sp_test_eq( \
  sp_confidence( bid, ask ), \
  expected, \
  "sp_confidence(%lu, %lu) == %lu != %lu", \
  ( sp_size_t )( bid ), \
  ( sp_size_t )( ask ), \
  sp_confidence( bid, ask ), \
  ( sp_size_t )( expected )\
)

Test( serum_pyth, confidence ) {

  // https://docs.pyth.network/publishers/confidence-interval-and-crypto-exchange-fees
  // bid=$50,000.000, ask=$50,000.010, fee=10bps -> CI=$50.005
  sp_test_conf( 50000000, 50000010, 50005 );
  sp_test_conf( 50000010, 50000000, 50005 );

}
