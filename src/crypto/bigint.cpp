#include <fc/crypto/bigint.hpp>
#include <fc/utility.hpp>
#include <fc/variant.hpp>
#include <fc/crypto/base64.hpp>

#include <fc/exception/exception.hpp>

#include <iterator>
#include <limits>

#include <boost/multiprecision/cpp_int.hpp>

namespace fc {
      using boost::multiprecision::cpp_int;

      struct bigint::impl {
        cpp_int n;
      };

      namespace {
        cpp_int from_big_endian( const char* bige, size_t l )
        {
          cpp_int n;
          if( l != 0 )
            boost::multiprecision::import_bits( n, (const unsigned char*)bige, (const unsigned char*)bige + l, 8 );
          return n;
        }

        cpp_int quotient( const cpp_int& a, const cpp_int& b )
        {
          return b == 0 ? cpp_int() : cpp_int( a / b );
        }

        cpp_int remainder( const cpp_int& a, const cpp_int& b )
        {
          return b == 0 ? cpp_int() : cpp_int( a % b );
        }

        cpp_int magnitude( const cpp_int& a )
        {
          return a < 0 ? cpp_int( -a ) : a;
        }

        cpp_int shift_magnitude( const cpp_int& a, uint32_t i, bool left )
        {
          cpp_int r = magnitude( a );
          if( left )
            r <<= i;
          else
            r >>= i;
          return a < 0 ? cpp_int( -r ) : r;
        }
      }

      bigint::bigint( const char* bige, uint32_t l )
        : my( std::make_unique<impl>( impl{ from_big_endian( bige, l ) } ) )
      {
      }
      bigint::bigint( const std::vector<char>& bige )
        : my( std::make_unique<impl>( impl{ from_big_endian( bige.data(), bige.size() ) } ) )
      {
      }
      bigint::bigint( )
        : my( std::make_unique<impl>() )
      { }

      bigint::bigint(uint64_t value)
        : my( std::make_unique<impl>( impl{ cpp_int( value ) } ) )
      {
      }

      bigint::bigint( const bigint& c )
        : my( std::make_unique<impl>( *c.my ) )
      {
      }

      bigint::bigint( bigint&& b )
        : my( std::move( b.my ) )
      {
      }

      bigint::~bigint() {}

      bool bigint::is_negative()const { return my->n < 0; }

      int64_t bigint::to_int64() const
      {
        FC_ASSERT(log2() <= 63);
        return my->n.convert_to<int64_t>();
      }

      int64_t bigint::log2()const
      {
        if( my->n == 0 )
          return 0;
        return int64_t( boost::multiprecision::msb( magnitude( my->n ) ) ) + 1;
      }
      bool bigint::operator < ( const bigint& c )const {
        return my->n < c.my->n;
      }
      bool bigint::operator > ( const bigint& c )const {
        return my->n > c.my->n;
      }
      bool bigint::operator >= ( const bigint& c )const {
        return my->n >= c.my->n;
      }
      bool bigint::operator == ( const bigint& c )const {
        return my->n == c.my->n;
      }
      bool bigint::operator != ( const bigint& c )const {
        return my->n != c.my->n;
      }
      bigint::operator bool()const
      {
        return my->n != 0;
      }
      bigint bigint::operator++(int)
      {
        bigint tmp = *this;
        *this = *this + bigint(1);
        return tmp;
      }
      bigint& bigint::operator++()
      {
        return *this = *this + bigint(1);
      }
      bigint bigint::operator--(int)
      {
        bigint tmp = *this;
        *this = *this - bigint(1);
        return tmp;
      }
      bigint& bigint::operator--()
      {
        return *this = *this - bigint(1);
      }

      bigint bigint::operator + ( const bigint& a )const {
        bigint tmp;
        tmp.my->n = my->n + a.my->n;
        return tmp;
      }
      bigint& bigint::operator += ( const bigint& a ){
        my->n += a.my->n;
        return *this;
      }
      bigint& bigint::operator -= ( const bigint& a ){
        my->n -= a.my->n;
        return *this;
      }


      bigint bigint::operator * ( const bigint& a )const {
        bigint tmp;
        tmp.my->n = my->n * a.my->n;
        return tmp;
      }
      bigint bigint::operator / ( const bigint& a ) const {
        bigint tmp;
        tmp.my->n = quotient( my->n, a.my->n );
        return tmp;
      }
      bigint bigint::operator % ( const bigint& a ) const {
        bigint tmp;
        tmp.my->n = remainder( my->n, a.my->n );
        return tmp;
      }

      bigint bigint::operator /= ( const bigint& a ) {
        bigint tmp( *this );
        my->n = quotient( my->n, a.my->n );
        return tmp;
      }
      bigint bigint::operator *= ( const bigint& a ) {
        my->n *= a.my->n;
        return *this;
      }
      bigint& bigint::operator >>= ( uint32_t i )
      {
         my->n = shift_magnitude( my->n, i, false );
         return *this;
      }

      bigint& bigint::operator <<= ( uint32_t i )
      {
         my->n = shift_magnitude( my->n, i, true );
         return *this;
      }

      bigint bigint::operator - ( const bigint& a )const {
        bigint tmp;
        tmp.my->n = my->n - a.my->n;
        return tmp;
      }
      bigint bigint::exp( const bigint& a )const
      {
        const cpp_int power = magnitude( a.my->n );
        FC_ASSERT( power <= std::numeric_limits<unsigned>::max() );
        bigint tmp;
        tmp.my->n = boost::multiprecision::pow( my->n, power.convert_to<unsigned>() );
        return tmp;
      }


      bigint& bigint::operator = ( bigint&& a ) {
        fc_swap( a.my, my );
        return *this;
      }
      bigint& bigint::operator = ( const bigint& a ) {
        if( &a == this )
          return *this;
        my = std::make_unique<impl>( *a.my );
        return *this;
      }
      bigint::operator fc::string()const {
        return my->n.str();
      }

      bigint::operator std::vector<char>()const {
        std::vector<char> to;
        if( my->n != 0 )
          boost::multiprecision::export_bits( magnitude( my->n ), std::back_inserter( to ), 8 );
        return to;
      }

  /** encodes the big int as base64 string, or a number */
  void to_variant( const bigint& bi, variant& v )
  {
    std::vector<char> ve = bi;
    v = fc::variant(base64_encode((unsigned char*)ve.data(),ve.size()));
  }

  /** decodes the big int as base64 string, or a number */
  void from_variant( const variant& v, bigint& bi )
  {
    if( v.is_numeric() ) bi = bigint( static_cast<unsigned long>(v.as_uint64()) );
    else
    {
        std::string b64 = v.as_string();
        std::string bin = base64_decode(b64);
        bi = bigint(bin.c_str(), bin.size() );
    }
  }

} // namespace fc
