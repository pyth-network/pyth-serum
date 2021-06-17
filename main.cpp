#include <pc/log.hpp>
#include <pc/manager.hpp>
#include <pc/bincode.hpp>

#include <csignal>
#include <iostream>

bool do_run = true;

pc::manager* man = nullptr;

void sig_handle( int )
{
  do_run = false;
}

int main(int /*argc*/, char** /*argv*/)
{
  signal( SIGPIPE, SIG_IGN );
  signal( SIGINT, sig_handle );
  signal( SIGHUP, sig_handle );
  signal( SIGTERM, sig_handle );

  pc::log::set_level(PC_LOG_DBG_LVL);

  pc::manager mgr;
  man = &mgr;
  mgr.set_rpc_host("api.mainnet-beta.solana.com");
  mgr.set_tx_host("localhost");
  mgr.set_dir("/home/mass/.pythd");
  mgr.set_do_capture(false);
  if (!mgr.init()) {
    std::cerr << "test_publish: " << mgr.get_err_msg() << std::endl;
    return 1;
  }

  int64_t last = pc::get_now();

  // run event loop and wait for product updates, price updates
  // and requests to submit price
  while( do_run && !mgr.get_is_err() ) {
    int64_t now = pc::get_now();

    mgr.poll(false);

    if (now - last > int64_t(3e9)) {
      if (mgr.get_recent_block_hash() == nullptr)
        continue;

      last = now;

      const auto* snd = mgr.get_publish_key_pair();
      pc::signature sig;

      pc::pub_key serumPID;
      serumPID.init_from_text(std::string("9xQeWvG816bUx9EPjHmaT23yvVM2ZWbrrpZb9PusVFin"));
      pc::pub_key market;
      market.init_from_text(std::string("C1EuT9VokAKLiW7i2ASnZUvxDoKuKkCpDDeNxAptuNe4"));
      pc::pub_key bids;
      bids.init_from_text(std::string("2e2bd5NtEGs6pb758QHUArNxt6X9TTC5abuE1Tao6fhS"));
      pc::pub_key asks;
      asks.init_from_text(std::string("F1tDtTDNzusig3kJwhKwGWspSu8z2nRwNXFWc6wJowjM"));
      //pc::pub_key pythPID;
      //pythPID.init_from_text(std::string("3UKMDZkEFaCKFd4TrGVf1Hr2QfQUqAEeZYM5wo2uX7q4")); //TODO
      pc::pub_key thisPID;
      thisPID.init_from_text(std::string("2PieNp8PgULJFeejR6EtQF4mnTjkFWycetwZhJht2qR3"));

      // construct binary transaction
      pc::net_buf *bptr = pc::net_buf::alloc();
      pc::bincode tx( bptr->buf_ );

      // signatures section
      tx.add_len<1>();      // one signature
      size_t sign_idx = tx.reserve_sign();

      // message header
      size_t tx_idx = tx.get_pos();
      tx.add( (uint8_t)1 ); // signing accounts
      tx.add( (uint8_t)0 ); // read-only signed accounts
      tx.add( (uint8_t)5 ); // read-only unsigned accounts

      // accounts
      tx.add_len<6>();      // TODO
      tx.add( *snd );      // sender account
      tx.add( serumPID );
      tx.add( market );      // market
      tx.add( bids );      // bid
      tx.add( asks );      // ask
      tx.add( thisPID );     // programid

      // recent block hash
      tx.add( *mgr.get_recent_block_hash() );    // recent block hash

      // instructions section
      tx.add_len<1>();
      tx.add( (uint8_t)5);  // program_id index
      tx.add_len<5>();
      tx.add( (uint8_t)0 );
      tx.add( (uint8_t)1 );
      tx.add( (uint8_t)2 );
      tx.add( (uint8_t)3 );
      tx.add( (uint8_t)4 );

      // instruction parameter section
      tx.add_len<0>();     // size of data array

      // sign message
      tx.sign( sign_idx, tx_idx, *snd );
      sig.init_from_buf( (const uint8_t*)(tx.get_buf() + sign_idx) );

      uint8_t res[4096];
      int len = pc::enc_base58((uint8_t*)tx.get_buf(), tx.size(), res, sizeof(res));

      std::cout << std::endl << std::endl;
      std::cout << "curl http://api.mainnet-beta.solana.com -X POST -H \"Content-Type: application/json\""
                << " -d \'{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"sendTransaction\",\"params\":[\"";
      std::cout.write((char*) res, len);
      std::cout << "\"]}\'";
      std::cout << std::endl << std::endl;
      std::cout << std::endl << std::endl;

      bptr->dealloc();
    }
  }

  // report any errors on exit
  // please note that manager exits in error if error submitting price
  int retcode = 0;
  if ( mgr.get_is_err() ) {
    std::cerr << "test_publish: " << mgr.get_err_msg() << std::endl;
    retcode = 1;
  }

  return retcode;
}
