#pragma once

#include <criterion/criterion.h>

// Turn off to stop test suites on first failure.
#define SP_ASSERT_ALL 1

#if SP_ASSERT_ALL
#define sp_assert_eq cr_expect_eq
#define sp_assert_ne cr_expect_neq
#define sp_assert_le cr_expect_leq
#define sp_assert_lt cr_expect_lt
#define sp_assert_ge cr_expect_geq
#define sp_assert_gt cr_expect_gt
#else
#define sp_assert_eq cr_assert_eq
#define sp_assert_ne cr_assert_neq
#define sp_assert_le cr_assert_leq
#define sp_assert_lt cr_assert_lt
#define sp_assert_ge cr_assert_geq
#define sp_assert_gt cr_assert_gt
#endif

// Disallow implicit conversion to bool.
#define sp_assert( x, ... ) sp_assert_eq( x, true, __VA_ARGS__ )

#define sp_assert_op( op, fmt, type, actual, expected ) sp_assert( \
  ( actual ) op ( expected ), \
  #actual " (%" #fmt ") " #op " " #expected " (%" #fmt ")", \
  actual, \
  ( type )( expected ) \
)

#define sp_assert_ptr( actual, expected ) sp_assert_eq( \
  actual, \
  expected, \
  #actual " == " #expected " (%ld byte diff)", \
  ( char* )( actual ) - ( char* )( expected ) \
)

#define sp_assert_u64( a, e ) sp_assert_op( ==, lu, uint64_t, a, e )
#define sp_assert_u32( a, e ) sp_assert_op( ==, u,  uint32_t, a, e )
#define sp_assert_i32( a, e ) sp_assert_op( ==, d,  int32_t,  a, e )

#define sp_assert_u64_lt( a, e ) sp_assert_op( <, lu, uint64_t, a, e )
#define sp_assert_u64_gt( a, e ) sp_assert_op( >, lu, uint64_t, a, e )

#define sp_assert_i32_lt( a, e ) sp_assert_op( <, d, int32_t, a, e )
#define sp_assert_i32_gt( a, e ) sp_assert_op( >, d, int32_t, a, e )

#define sp_assert_size_eq sp_assert_u64
#define sp_assert_size_lt sp_assert_u64_lt
#define sp_assert_size_gt sp_assert_u64_gt

#define sp_assert_expo_lt sp_assert_i32_lt
#define sp_assert_expo_gt sp_assert_i32_gt
