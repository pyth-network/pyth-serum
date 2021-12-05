cmake_minimum_required( VERSION 3.13 )

project( serum-pyth )
add_subdirectory( test-crank )

function( add_bpf_lib targ )
  if( SOLANA AND NOT BPF )
    set( BPF ${SOLANA}/sdk/bpf )
  endif()
  if( BPF )
    if( NOT PC )
      set( PC ../pyth-client )
    endif()
    add_library( ${targ} STATIC ${ARGN} )
    if( ${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.21" )
      # bpf.mk uses --std=c17
      set_property( TARGET ${targ} PROPERTY C_STANDARD 17 )
    else()
      set_property( TARGET ${targ} PROPERTY C_STANDARD 11 )
    endif()
    set_property( TARGET ${targ} PROPERTY C_STANDARD_REQUIRED ON )
    set_property( TARGET ${targ} PROPERTY C_EXTENSIONS OFF )
    target_compile_definitions( ${targ} PRIVATE __bpf__=1 )
    target_include_directories( ${targ} PRIVATE
      program/src
      ${PC}/program/src
    )
    target_include_directories( ${targ} SYSTEM PRIVATE
      ${BPF}/c/inc
      ${BPF}/dependencies/criterion/include
    )
  endif()
endfunction()

add_bpf_lib( serum-pyth program/src/serum-pyth/serum-pyth.c )
add_bpf_lib( test-serum-pyth program/src/serum-pyth/test_serum-pyth.c )
