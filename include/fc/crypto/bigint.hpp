#pragma once
#include <stdint.h>
#include <fc/string.hpp>
#include <fc/vector.hpp>

#include <memory>

namespace fc {
  class bigint {
    public:
      bigint( const std::vector<char>& bige );
      bigint( const char* bige, uint32_t l );
      bigint(uint64_t value);
      bigint( );
      bigint( const bigint& c );
      bigint( bigint&& c );
      ~bigint();

      bigint& operator = ( const bigint& a );
      bigint& operator = ( bigint&& a );

      explicit operator bool()const;

      bool    is_negative()const;
      int64_t to_int64()const;

      int64_t log2()const;
      bigint  exp( const bigint& c )const;

      static bigint random( uint32_t bits, int t, int  );

      bool operator < ( const bigint& c )const;
      bool operator > ( const bigint& c )const;
      bool operator >= ( const bigint& c )const;
      bool operator == ( const bigint& c )const;
      bool operator != ( const bigint& c )const;

      bigint operator +  ( const bigint& a )const;
      bigint operator *  ( const bigint& a )const;
      bigint operator /  ( const bigint& a )const; 
      bigint operator %  ( const bigint& a )const; 
      bigint operator /= ( const bigint& a ); 
      bigint operator *= ( const bigint& a ); 
      bigint& operator += ( const bigint& a ); 
      bigint& operator -= ( const bigint& a ); 
      bigint& operator <<= ( uint32_t i ); 
      bigint& operator >>= ( uint32_t i ); 
      bigint operator -  ( const bigint& a )const;


      bigint operator++(int);
      bigint& operator++();
      bigint operator--(int);
      bigint& operator--();

      operator fc::string()const;

      // returns bignum as bigendian bytes
      operator std::vector<char>()const;

    private:
      struct impl;
      std::unique_ptr<impl> my;
  };

  class variant;
  /** encodes the big int as base64 string, or a number */
  void to_variant( const bigint& bi, variant& v );
  /** decodes the big int as base64 string, or a number */
  void from_variant( const variant& v, bigint& bi );
} // namespace fc

