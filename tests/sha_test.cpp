
#include <fc/crypto/restartable_sha256.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/io/raw.hpp>

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
   // There was a bug that corrupted the buffer whenever the final fill
   // (length & 0x3F) landed in [57,63], so those lengths yielded a wrong digest.
   for( size_t len = 0; len <= 300; ++len )
      if( !check( slice( len ), 0, "single-update", reported ) )
         ++failures;

   // Test 2: pattern used f.e. in Hive - repeated fixed 8-byte updates
   for( size_t len = 0; len <= 304; len += 8 )
      if( !check( slice( len ), 8, "8-byte-chunks", reported ) )
         ++failures;

   // Test 3: many different chunk sizes crossing block boundaries, lengths 0..300.
   for( size_t chunk = 1; chunk <= 20; ++chunk )
      for( size_t len = 0; len <= 300; ++len )
         if( !check( slice( len ), chunk, "split-update", reported ) )
            ++failures;

   // Test 4: serialize the mid-hash state (FC_REFLECT), reconstruct it, then keep
   // hashing. This is the whole point of the class - rc_stats_object::stamp is
   // persisted mid-hash in chain state and resumed after a node restart/replay -
   // and it is the one path the in-object tests above never exercise.
   {
      std::vector<uint8_t> msg = slice( 300 );
      std::string ref = reference_hex( msg );
      for( size_t split = 0; split <= msg.size(); ++split )
      {
         fc::restartable_sha256 a;
         a.update( msg.data(), split );

         std::vector<char> packed = fc::raw::pack_to_vector( a );
         fc::restartable_sha256 b;
         fc::raw::unpack_from_vector( packed, b );

         b.update( msg.data() + split, msg.size() - split );
         b.finish();

         if( b.hexdigest() != ref )
         {
            if( reported < 20 )
               std::cerr << "MISMATCH [serialize-resume] split=" << split << std::endl;
            ++reported;
            ++failures;
         }
      }
   }

   // Test 5: update() after finish() must throw rather than silently keep hashing
   // a finished digest.
   {
      fc::restartable_sha256 s;
      s.update( buf.data(), 10 );
      s.finish();
      bool threw = false;
      try { s.update( buf.data(), 1 ); }
      catch( ... ) { threw = true; }
      if( !threw )
      {
         std::cerr << "update() after finish() did not throw" << std::endl;
         ++failures;
      }
   }

   // Test 6: known-answer vectors (FIPS 180-4) - anchors against an independent
   // reference so a shared OpenSSL-side defect cannot pass unnoticed.
   {
      const std::pair<std::string, std::string> kat[] = {
         { "", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" },
         { "abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" },
         { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
           "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1" },
      };
      for( const auto& kv : kat )
      {
         fc::restartable_sha256 s;
         s.update( kv.first.data(), kv.first.size() );
         s.finish();
         if( s.hexdigest() != kv.second )
         {
            std::cerr << "KAT mismatch for \"" << kv.first << "\"\n  got " << s.hexdigest()
                      << "\n  exp " << kv.second << std::endl;
            ++failures;
         }
      }
   }

   if( failures != 0 )
   {
      std::cerr << "\nrestartable_sha256 does NOT match fc::sha256: " << failures
                << " failing case(s)" << std::endl;
      return EXIT_FAILURE;
   }

   std::cout << "restartable_sha256 matches fc::sha256 for all tested inputs" << std::endl;
   return 0;
}
