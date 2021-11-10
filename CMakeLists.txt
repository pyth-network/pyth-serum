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
    target_compile_definitions( ${targ} PRIVATE __bpf__=1 )
    target_include_directories( ${targ} PRIVATE ${PC}/program/src )
    target_include_directories( ${targ} SYSTEM PRIVATE
      ${BPF}/c/inc
      ${BPF}/dependencies/criterion/include
    )
  endif()
endfunction()

add_bpf_lib( serum-pyth program/src/serum-pyth/serum-pyth.c )
add_bpf_lib( test-serum-pyth program/src/serum-pyth/test_serum-pyth.c )
