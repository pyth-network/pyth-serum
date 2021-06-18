#include <pc/bincode.hpp>
#include <pc/log.hpp>
#include <pc/manager.hpp>

#include <csignal>
#include <iostream>

bool do_run = true;
void sig_handle( int )
{
  do_run = false;
}

class tx_wtr : public pc::net_wtr
{
public:
  void init( pc::bincode& tx ) {
    tx.attach( hd_->buf_ );
    tx.add( (uint16_t)PC_TPU_PROTO_ID );
    tx.add( (uint16_t)0 );
  }
  void commit( pc::bincode& tx ) {
    pc::tx_hdr *hdr = (pc::tx_hdr*)hd_->buf_;
    hd_->size_ = tx.size();
    hdr->size_ = tx.size();
  }
};

class serum_pyth : public pc::tx_request
{
public:
  void set_block_hash( pc::hash *bhash ) { bhash_ = bhash; }
  void set_publish( pc::key_pair *kp ) { pkey_ = kp; }
  void set_pubcache( pc::key_cache *kc ) { ckey_ = kc; }
  void set_program( pc::pub_key *pk ) { gkey_ = pk; }
  void set_serum_prog( pc::pub_key *pk ) { serum_prog_ = pk; }
  void set_serum_market( pc::pub_key *pk ) { serum_market_ = pk; }
  void set_serum_bids( pc::pub_key *pk ) { serum_bids_ = pk; }
  void set_serum_asks( pc::pub_key *pk ) { serum_asks_ = pk; }
  void set_spl_quote_mint( pc::pub_key *pk) { spl_quote_mint_ = pk; }
  void set_spl_base_mint( pc::pub_key *pk) { spl_base_mint_ = pk; }
  void build( pc::net_wtr& ) override;

private:
  pc::hash         *bhash_ = nullptr;
  pc::key_pair     *pkey_ = nullptr;
  pc::key_cache    *ckey_ = nullptr;
  pc::pub_key      *gkey_ = nullptr;
  pc::pub_key      *serum_prog_ = nullptr;
  pc::pub_key      *serum_market_ = nullptr;
  pc::pub_key      *serum_bids_ = nullptr;
  pc::pub_key      *serum_asks_ = nullptr;
  pc::pub_key      *spl_quote_mint_ = nullptr;
  pc::pub_key      *spl_base_mint_ = nullptr;
};

void serum_pyth::build( pc::net_wtr& wtr )
{
  // construct binary transaction and add header
  pc::bincode tx;
  ((tx_wtr&)wtr).init( tx );

  // signatures section
  tx.add_len<1>();      // one signature (publish)
  size_t pub_idx = tx.reserve_sign();

  // message header
  size_t tx_idx = tx.get_pos();
  tx.add( (uint8_t)1 ); // pub is only signing account
  tx.add( (uint8_t)0 ); // read-only signed accounts
  tx.add( (uint8_t)7 );

  // accounts
  tx.add_len<8>();
  tx.add( *pkey_ );
  tx.add( *serum_prog_ );
  tx.add( *serum_market_ );
  tx.add( *serum_bids_ );
  tx.add( *serum_asks_ );
  tx.add( *spl_quote_mint_ );
  tx.add( *spl_base_mint_ );
  tx.add( *gkey_ );

  // recent block hash
  tx.add( *bhash_ );    // recent block hash

  // instructions section
  tx.add_len<1>();      // one instruction
  tx.add( (uint8_t)7);  // program_id index
  tx.add_len<7>();
  tx.add( (uint8_t)0 );
  tx.add( (uint8_t)1 );
  tx.add( (uint8_t)2 );
  tx.add( (uint8_t)3 );
  tx.add( (uint8_t)4 );
  tx.add( (uint8_t)5 );
  tx.add( (uint8_t)6 );

  // instruction parameter section
  tx.add_len<0>();

  // all accounts need to sign transaction
  tx.sign( pub_idx, tx_idx, *ckey_ );
  ((tx_wtr&)wtr).commit( tx );
}

int main(int /*argc*/, char** /*argv*/)
{
  signal( SIGPIPE, SIG_IGN );
  signal( SIGINT, sig_handle );
  signal( SIGHUP, sig_handle );
  signal( SIGTERM, sig_handle );

  pc::log::set_level(PC_LOG_DBG_LVL);

  pc::manager mgr;
  mgr.set_rpc_host("api.mainnet-beta.solana.com");
  mgr.set_tx_host("localhost");
  mgr.set_dir("/home/mass/.pythd");
  mgr.set_do_capture(false);
  if (!mgr.init()) {
    std::cerr << "test_publish: " << mgr.get_err_msg() << std::endl;
    return 1;
  }

  pc::pub_key serumPID;
  serumPID.init_from_text(std::string("9xQeWvG816bUx9EPjHmaT23yvVM2ZWbrrpZb9PusVFin"));
  pc::pub_key market;
  market.init_from_text(std::string("C1EuT9VokAKLiW7i2ASnZUvxDoKuKkCpDDeNxAptuNe4"));
  pc::pub_key bids;
  bids.init_from_text(std::string("2e2bd5NtEGs6pb758QHUArNxt6X9TTC5abuE1Tao6fhS"));
  pc::pub_key asks;
  asks.init_from_text(std::string("F1tDtTDNzusig3kJwhKwGWspSu8z2nRwNXFWc6wJowjM"));
  pc::pub_key thisPID;
  thisPID.init_from_text(std::string("2PieNp8PgULJFeejR6EtQF4mnTjkFWycetwZhJht2qR3"));
  pc::pub_key quoteMint;
  quoteMint.init_from_text(std::string("Es9vMFrzaCERmJfrF4H2FYD4KCoNkY11McCe8BenwNYB"));
  pc::pub_key baseMint;
  baseMint.init_from_text(std::string("9n4nbM75f5Ui33ZbPYXn59EwSgE8CGsHtAeTH5YFeJ9E"));

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

      serum_pyth req[1];
      req->set_publish(mgr.get_publish_key_pair());
      req->set_pubcache(mgr.get_publish_key_cache());
      req->set_program(&thisPID);
      req->set_block_hash(mgr.get_recent_block_hash());
      req->set_serum_prog(&serumPID);
      req->set_serum_market(&market);
      req->set_serum_bids(&bids);
      req->set_serum_asks(&asks);
      req->set_spl_quote_mint(&quoteMint);
      req->set_spl_base_mint(&baseMint);
      mgr.submit(req);
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
