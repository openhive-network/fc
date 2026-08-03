#pragma once

#include <cstdint>
#include <string>

#include <fc/array.hpp>
#include <fc/reflect/reflect.hpp>

namespace fc {

class sha256;

class restartable_sha256
{
   public:
      restartable_sha256();
      void update( const void* data, size_t count );
      void finish();
      std::string hexdigest()const;
      // converts the (finished) digest into an fc::sha256; the result's str() matches hexdigest()
      sha256 to_sha256()const;

      fc::array< uint32_t, 8 >         _h;      // mid-hash: SHA-256 chaining value; finished: big-endian digest
      fc::array< unsigned char, 64 >   _data;   // pending input bytes ([0.._length&0x3F), rest zeroed)
      uint64_t                         _length = 0;
      bool                             _finished = false;
};

}

FC_REFLECT( fc::restartable_sha256,
   (_h)
   (_data)
   (_length)
   (_finished)
   )
