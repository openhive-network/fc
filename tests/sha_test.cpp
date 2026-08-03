
#include <fc/crypto/restartable_sha256.hpp>
#include <fc/crypto/sha256.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

// Reference digest computed with the standard (OpenSSL-backed) fc::sha256.
std::string reference_hex( const std::vector<uint8_t>& data )
{
   return fc::sha256::hash( (const char*) data.data(), (uint32_t) data.size() ).str();
}

// Hash `data` with restartable_sha256, feeding it in fixed-size pieces.
// chunk == 0 means a single update() call with the whole buffer.
fc::restartable_sha256 run( const std::vector<uint8_t>& data, size_t chunk )
{
   fc::restartable_sha256 s;
   if( chunk == 0 )
   {
      s.update( data.data(), data.size() );
   }
   else
   {
      size_t off = 0;
      while( off < data.size() )
      {
         size_t n = std::min( chunk, data.size() - off );
         s.update( data.data() + off, n );
         off += n;
      }
   }
   s.finish();
   return s;
}

// Verify restartable_sha256 against the reference for `data` fed in `chunk`-sized
// pieces. Also verifies the to_sha256() == hexdigest() invariant. Returns true on
// success; on failure prints details (at most `report` of them) and returns false.
bool check( const std::vector<uint8_t>& data, size_t chunk, const char* label, int& reported )
{
   fc::restartable_sha256 s = run( data, chunk );
   std::string got = s.hexdigest();
   std::string ref = reference_hex( data );

   bool ok = ( got == ref );
   if( !ok && reported < 20 )
   {
      std::cerr << "MISMATCH [" << label << "] len=" << data.size()
                << " (len&63=" << ( data.size() & 63 ) << ")\n"
                << "  got " << got << "\n  ref " << ref << std::endl;
      ++reported;
   }

   // to_sha256() must yield the exact same string form as hexdigest()
   if( s.to_sha256().str() != got )
   {
      if( reported < 20 )
      {
         std::cerr << "to_sha256() mismatch: " << s.to_sha256().str()
                   << " != " << got << std::endl;
         ++reported;
      }
      ok = false;
   }

   return ok;
}

} // namespace

int main( int argc, char** argv, char** envp )
{
   // Deterministic pseudo-random input bytes.
   std::vector<uint8_t> buf( 512 );
   uint32_t st = 0x12345678;
   for( auto& b : buf )
   {
      st = st * 1103515245u + 12345u;
      b = uint8_t( st >> 16 );
   }

   int failures = 0;
   int reported = 0;

   auto slice = [&]( size_t len ) { return std::vector<uint8_t>( buf.begin(), buf.begin() + len ); };

   // Test 1: single update() call, every length 0..300.
   // The current implementation corrupts the buffer whenever the final fill
   // (length & 0x3F) lands in [57,63], so those lengths yield a wrong digest.
   for( size_t len = 0; len <= 300; ++len )
      if( !check( slice( len ), 0, "single-update", reported ) )
         ++failures;

   // Test 2: consensus pattern - repeated fixed 8-byte updates.
   // rc_stats_object::add_stats() feeds sizeof(int64_t)==8 bytes per transaction.
   for( size_t len = 0; len <= 304; len += 8 )
      if( !check( slice( len ), 8, "8-byte-chunks", reported ) )
         ++failures;

   // Test 3: many different chunk sizes crossing block boundaries, lengths 0..300.
   for( size_t chunk = 1; chunk <= 20; ++chunk )
      for( size_t len = 0; len <= 300; ++len )
         if( !check( slice( len ), chunk, "split-update", reported ) )
            ++failures;

   if( failures != 0 )
   {
      std::cerr << "\nrestartable_sha256 does NOT match fc::sha256: " << failures
                << " failing case(s)" << std::endl;
      return EXIT_FAILURE;
   }

   std::cout << "restartable_sha256 matches fc::sha256 for all tested inputs" << std::endl;
   return 0;
}
