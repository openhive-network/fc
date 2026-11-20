#include <fc/string.hpp>
#include <fc/utility.hpp>
#include <fc/fwd_impl.hpp>
#include <fc/exception/exception.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include <algorithm>
#include <clocale>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sstream>
#include <iomanip>
#include <locale>
#include <limits>

/**
 *  Implemented with std::string for now.
 */

namespace fc  {
   class comma_numpunct : public std::numpunct<char>
   {
      protected:
         virtual char do_thousands_sep() const { return ','; }
         virtual std::string do_grouping() const { return "\03"; }
   };

  std::string to_pretty_string( int64_t value )
  {
     std::stringstream ss;
     ss.imbue( {std::locale(), new comma_numpunct} );
     ss << std::fixed << value;
     return ss.str();
  }

#ifdef USE_FC_STRING
  string::string(const char* s, int l) :my(s,l){ }
  string::string(){}
  string::string( const fc::string& c ):my(*c.my) { }
  string::string( string&& m ):my(fc::move(*m.my)) {}
  string::string( const char* c ):my(c){}
  string::string( const_iterator b, const_iterator e ):my(b,e){}
  string::string( const std::string& s ):my(s) {}
  string::string( std::string&& s ):my(fc::move(s)) {}
  string::~string() { }
  string::operator std::string&() { return *my; }
  string::operator const std::string&()const { return *my; }
  char* string::data() { return &my->front(); }

  string::iterator string::begin()            { return &(*this)[0]; }
  string::iterator string::end()              { return &(*this)[size()]; }
  string::const_iterator string::begin()const { return my->c_str(); }
  string::const_iterator string::end()const   { return my->c_str() + my->size(); }

  char&       string::operator[](size_t idx)      { return (*my)[idx]; }
  const char& string::operator[](size_t idx)const { return (*my)[idx]; }

  void       string::reserve(size_t r)           { my->reserve(r); }
  size_t   string::size()const                   { return my->size(); }
  size_t   string::find(char c, size_t p)const { return my->find(c,p); }
  size_t   string::find(const fc::string& str, size_t pos /* = 0 */) const { return my->find(str, pos); }
  size_t   string::find(const char* s, size_t pos /* = 0 */) const { return my->find(s,pos); }
  size_t   string::rfind(char c, size_t p)const { return my->rfind(c,p); }
  size_t   string::rfind(const char* c, size_t p)const { return my->rfind(c,p); }
  size_t   string::rfind(const fc::string& c, size_t p)const { return my->rfind(c,p); }
  size_t   string::find_first_of(const fc::string& str, size_t pos /* = 0 */) const { return my->find_first_of(str, pos); }
  size_t   string::find_first_of(const char* s, size_t pos /* = 0 */) const { return my->find_first_of(s, pos); }

  fc::string& string::replace(size_t pos,  size_t len,  const string& str) { my->replace(pos, len, str); return *this; }
  fc::string& string::replace(size_t pos,  size_t len,  const char* s) { my->replace(pos, len, s); return *this; }

  void       string::clear()                       { my->clear(); }
  void       string::resize( size_t s )          { my->resize(s); }
                                            
  fc::string string::substr( size_t start, size_t len )const { return my->substr(start,len); }
  const char* string::c_str()const                        { return my->c_str(); }

  bool    string::operator == ( const char* s )const   { return *my == s; }
  bool    string::operator == ( const string& s )const { return *my == *s.my; }
  bool    string::operator != ( const string& s )const { return *my != *s.my; }

  string& string::operator =( const string& c )          { *my = *c.my; return *this; }
  string& string::operator =( string&& c )               { *my = fc::move( *c.my ); return *this; }

  string& string::operator+=( const string& s )          { *my += *s.my; return *this; }
  string& string::operator+=( char c )                   { *my += c; return *this; }

  bool operator < ( const string& a, const string& b )   { return *a.my < *b.my; } 
  string operator + ( const string& s, const string& c ) { return string(s) += c; }
  string operator + ( const string& s, char c ) 	 { return string(s) += c; }
#endif // USE_FC_STRING


  int64_t    to_int64( const fc::string& i )
  {
    try
    {
      return boost::lexical_cast<int64_t>(i.c_str());
    }
    catch( const boost::bad_lexical_cast& e )
    {
      FC_THROW_EXCEPTION( parse_error_exception, "Couldn't parse int64_t" );
    }
    FC_RETHROW_EXCEPTIONS( warn, "${i} => int64_t", ("i",i) )
  }

  uint64_t   to_uint64( const fc::string& i )
  { try {
    try
    {
      return boost::lexical_cast<uint64_t>(i.c_str());
    }
    catch( const boost::bad_lexical_cast& e )
    {
      FC_THROW_EXCEPTION( parse_error_exception, "Couldn't parse uint64_t" );
    }
    FC_RETHROW_EXCEPTIONS( warn, "${i} => uint64_t", ("i",i) )
  } FC_CAPTURE_AND_RETHROW( (i) ) }

  namespace
  {
    /// The decimal point of the C locale of the process: '.' unless the program changed LC_NUMERIC.
    char c_decimal_point()
    {
      const char* point = localeconv()->decimal_point;
      return point != nullptr && point[0] != '\0' ? point[0] : '.';
    }

    bool equal_ignoring_case( const char* text, const char* lowercase, size_t size )
    {
      for( size_t i = 0; i < size; ++i )
        if( text[i] != lowercase[i] && text[i] != lowercase[i] - 'a' + 'A' )
          return false;
      return true;
    }

    bool is_digit( char c ) { return c >= '0' && c <= '9'; }

    /// The rules of boost::lexical_cast<double> on the streams of the classic locale: inf, infinity or nan, or nan
    /// followed by anything in parentheses (any case, an optional sign), or a decimal number with an optional sign,
    /// point and exponent, nothing before or after it; a value above the range of double fails, one below it becomes
    /// a subnormal or zero.
    bool parse_double( const char* begin, double& value )
    {
      const char* end = begin + strlen( begin );
      if( begin == end )
        return false;

      const char* p = begin;
      const bool minus = *p == '-';
      if( minus || *p == '+' )
        ++p;
      if( end - p >= 3 )
      {
        if( equal_ignoring_case( p, "nan", 3 ) )
        {
          const char* rest = p + 3;
          if( rest != end && ( end - rest < 2 || *rest != '(' || *( end - 1 ) != ')' ) )
            return false;
          value = minus ? -std::numeric_limits<double>::quiet_NaN() : std::numeric_limits<double>::quiet_NaN();
          return true;
        }
        if( ( end - p == 3 && equal_ignoring_case( p, "infinity", 3 ) ) ||
            ( end - p == 8 && equal_ignoring_case( p, "infinity", 8 ) ) )
        {
          value = minus ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
          return true;
        }
      }

      bool mantissa_digits = false;
      while( p != end && is_digit( *p ) ) { ++p; mantissa_digits = true; }
      if( p != end && *p == '.' )
      {
        ++p;
        while( p != end && is_digit( *p ) ) { ++p; mantissa_digits = true; }
      }
      if( !mantissa_digits )
        return false;
      if( p != end && ( *p == 'e' || *p == 'E' ) )
      {
        ++p;
        if( p != end && ( *p == '+' || *p == '-' ) )
          ++p;
        if( p == end || !is_digit( *p ) )
          return false;
        while( p != end && is_digit( *p ) )
          ++p;
      }
      if( p != end )
        return false;

      std::string number( begin, end );
      const char point = c_decimal_point();
      if( point != '.' )
        std::replace( number.begin(), number.end(), '.', point );
      value = strtod( number.c_str(), nullptr );
      return !std::isinf( value );
    }
  }

  double     to_double( const fc::string& i)
  {
    try
    {
      double value = 0;
      if( !parse_double( i.c_str(), value ) )
        throw boost::bad_lexical_cast();
      return value;
    }
    catch( const boost::bad_lexical_cast& e )
    {
      FC_THROW_EXCEPTION( parse_error_exception, "Couldn't parse double" );
    }
    FC_RETHROW_EXCEPTIONS( warn, "${i} => double", ("i",i) )
  }

  fc::string to_string(double d)
  {
    // +2 is required to ensure that the double is rounded correctly when read back in.  http://docs.oracle.com/cd/E19957-01/806-3568/ncg_goldberg.html
    // "%.17f" in the C locale is what a stream with setprecision(17) and fixed writes
    const int precision = std::numeric_limits<double>::digits10 + 2;
    const int size = snprintf( nullptr, 0, "%.*f", precision, d );
    std::string result( size, '\0' );
    snprintf( &result[0], size + 1, "%.*f", precision, d );
    const char point = c_decimal_point();
    if( point != '.' )
      std::replace( result.begin(), result.end(), point, '.' );
    return result;
  }

  fc::string to_string(unsigned long long d)
  {
    return boost::lexical_cast<std::string>(d);
  }

  fc::string to_string( int64_t d)
  {
    return boost::lexical_cast<std::string>(d);
  }
  fc::string to_string( uint16_t d)
  {
    return boost::lexical_cast<std::string>(d);
  }
  namespace
  {
    /// The white space of the classic locale.
    bool is_space( char c )
    {
      return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
    }
  }

  std::string trim( const std::string& s )
  {
      size_t first = 0;
      size_t last = s.size();
      while( first < last && is_space( s[first] ) )
        ++first;
      while( last > first && is_space( s[last - 1] ) )
        --last;
      return s.substr( first, last - first );
  }
  std::string to_lower( const std::string& s )
  {
     auto tmp = s;
     for( char& c : tmp )
       if( c >= 'A' && c <= 'Z' )
         c = char( c - 'A' + 'a' );
     return tmp;
  }
  string trim_and_normalize_spaces( const string& s )
  {
     string result = trim( s );
     while( result.find( "  " ) != result.npos )
       boost::algorithm::replace_all( result, "  ", " " );
     return result;
  }

  /**
   * Parses a size including an optional multiplicative suffix.
   *
   * M   -> 1024*1024 bytes
   * MB  -> 1000*1000 bytes
   * MiB -> 1024*1024 bytes
   *
   * The 'M' may be any of KMGTPEZY (upper or lower case)
   */
  uint64_t parse_size( const string& s )
  {
     try
     {
        size_t i = 0, n = s.size(), suffix_start = n;
        for( i=0; i<n; i++ )
        {
           if( !((s[i] >= '0') && (s[i] <= '9')) )
           {
              suffix_start = i;
              break;
           }
        }
        uint64_t u = to_uint64( s.substr( 0, suffix_start ) );

        FC_ASSERT( n - suffix_start <= 3 );

        uint64_t m = 1;
        uint64_t thousand = 1024;

        if( suffix_start == n )
        {
           return u;
        }
        else if( suffix_start == n-1 )
        {
        }
        else if( suffix_start == n-2 )
        {
           FC_ASSERT( (s[suffix_start+1] == 'b') || (s[suffix_start+1] == 'B') );
           thousand = 1000;
        }
        else if( suffix_start == n-3 )
        {
           FC_ASSERT( (s[suffix_start+1] == 'i') || (s[suffix_start+1] == 'I') );
           FC_ASSERT( (s[suffix_start+2] == 'b') || (s[suffix_start+2] == 'B') );
        }
        switch( s[suffix_start] )
        {
           case 'y':
           case 'Y':
              m *= thousand;
           case 'z':
           case 'Z':
              m *= thousand;
           case 'e':
           case 'E':
              m *= thousand;
           case 'p':
           case 'P':
              m *= thousand;
           case 't':
           case 'T':
              m *= thousand;
           case 'g':
           case 'G':
              m *= thousand;
           case 'm':
           case 'M':
              m *= thousand;
           case 'k':
           case 'K':
              m *= thousand;
              break;
           default:
              FC_ASSERT( false );
        }
        return u*m;
     }
     catch( const fc::exception& e )
     {
        FC_THROW_EXCEPTION( parse_error_exception, "Couldn't parse size" );
     }
  }


} // namespace fc


