// Set PC_HEAP_START before including oracle.h.
char heap_start[ 8192 ];
#define PC_HEAP_START ( heap_start )

#include <criterion/criterion.h>

#include <serum-pyth/serum-pyth.c> // NOLINT(bugprone-suspicious-include)
#include <serum-pyth/tests/assert.h>
#include <serum-pyth/tests/confidence.h>
#include <serum-pyth/tests/math.h>
#include <serum-pyth/tests/serum_to_pyth.h>

// Assert pyth-client allocations use test heap.
Test( serum_pyth, heap_start )
{
  sp_assert_eq( PC_HEAP_START, heap_start );
  sp_assert_ne( PC_HEAP_START, NULL );
  sp_assert_ne( ( uint64_t ) PC_HEAP_START, HEAP_START_ADDRESS );
}

Test( serum_pyth, confidence ) { sp_test_confidence(); }
Test( serum_pyth, constants ) { sp_test_constants(); }
Test( serum_pyth, pow10_divide ) { sp_test_pow10div(); }
Test( serum_pyth, serum_to_pyth ) { sp_test_serum_to_pyth(); }
