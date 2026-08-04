
#include <fc/crypto/restartable_sha256.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/exception/exception.hpp>

#include <openssl/sha.h>
#include <string.h>

namespace fc {

namespace {

// Rebuild an OpenSSL SHA-256 context from the reflected mid-hash state.
//
// This relies on the classic low-level SHA256_CTX field API and its semantics:
// h    - the Merkle-Damgard chaining value,
// Nl/Nh- the processed message length in bits (low/high 32-bit halves),
// num  - the number of buffered (not-yet-processed) input bytes,
// data - the buffer holding those bytes.
//
// What this does and does NOT decouple:
//  - our *serialized* state (the reflected _h/_length/_data) is our own format
//    and portable: _h is the standard chaining value, so it round-trips through
//    any correct SHA-256 implementation, not just this OpenSSL build;
//  - the *code below* is nonetheless coupled to the SHA256_CTX field layout. If a
//    future OpenSSL made the context opaque this would fail to compile (loud, and
//    acceptable). The static_asserts pin the field sizes we depend on.
SHA256_CTX load_context( const restartable_sha256& s )
{
   SHA256_CTX ctx;
   SHA256_Init( &ctx );
   static_assert( sizeof( ctx.h ) == sizeof( s._h ), "sha256 state size mismatch" );
   static_assert( sizeof( ctx.data ) == sizeof( s._data ), "sha256 buffer size mismatch" );
   static_assert( sizeof( ctx.Nl ) == 4 && sizeof( ctx.Nh ) == 4, "sha256 length field size mismatch" );
   memcpy( ctx.h, s._h.begin(), sizeof( ctx.h ) );
   uint64_t bits = s._length << 3;
   ctx.Nl = (uint32_t) ( bits & 0xFFFFFFFF );
   ctx.Nh = (uint32_t) ( bits >> 32 );
   ctx.num = (unsigned int) ( s._length & 0x3F );
   memcpy( ctx.data, s._data.begin(), sizeof( ctx.data ) );
   return ctx;
}

}

restartable_sha256::restartable_sha256()
{
   _h[0] = 0x6a09e667;
   _h[1] = 0xbb67ae85;
   _h[2] = 0x3c6ef372;
   _h[3] = 0xa54ff53a;
   _h[4] = 0x510e527f;
   _h[5] = 0x9b05688c;
   _h[6] = 0x1f83d9ab;
   _h[7] = 0x5be0cd19;
   // _data is zero-initialized by fc::array<unsigned char,N>'s constructor
}

void restartable_sha256::update( const void* data, size_t count )
{
   // after finish() _h holds the digest, not a chaining value, so resuming would
   // silently produce a meaningless result - reject it instead
   FC_ASSERT( !_finished, "cannot update a finished restartable_sha256" );

   if( count == 0 )
      return;

   SHA256_CTX ctx = load_context( *this );
   SHA256_Update( &ctx, data, count );

   memcpy( _h.begin(), ctx.h, sizeof( _h ) );
   _length += uint64_t( count );

   // keep only the pending tail, leaving the rest of the buffer zeroed, so the
   // reflected state stays canonical regardless of how the data was chunked
   size_t num = size_t( _length & 0x3F );
   _data = fc::array< unsigned char, 64 >(); // zero-fills via its constructor
   memcpy( _data.begin(), ctx.data, num );
}

void restartable_sha256::finish()
{
   if( _finished )
      return;

   SHA256_CTX ctx = load_context( *this );
   // once finished, the 32 bytes of _h are the standard big-endian digest, exactly
   // the byte layout fc::sha256 stores in _hash - so str()/hexdigest() match and
   // to_sha256() is a raw copy
   SHA256_Final( (unsigned char*) _h.begin(), &ctx );
   _finished = true;
}

std::string restartable_sha256::hexdigest()const
{
   static const char hex_digit[] = "0123456789abcdef";

   if( !_finished )
   {
      restartable_sha256 temp = *this;
      temp.finish();
      return temp.hexdigest();
   }

   char buf[65];
   const char* h = (const char *)&_h;
   for( int i=0,j=0; i<32; i++,j+=2 )
   {
      char c = h[i];
      buf[j  ] = hex_digit[ (c >> 4) & 0x0F ];
      buf[j+1] = hex_digit[  c       & 0x0F ];
   }
   buf[64] = '\0';
   return std::string( buf );
}

sha256 restartable_sha256::to_sha256()const
{
   if( !_finished )
   {
      restartable_sha256 temp = *this;
      temp.finish();
      return temp.to_sha256();
   }

   // once finished, the 32 bytes of _h are the standard big-endian digest, exactly
   // the byte layout fc::sha256 stores in _hash - so a raw copy yields str() == hexdigest()
   return sha256( (const char*)&_h, sizeof( _h ) );
}

}
