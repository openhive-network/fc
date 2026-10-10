#include <boost/test/unit_test.hpp>

#include <fc/filesystem.hpp>
#include <fc/variant.hpp>

#include <boost/filesystem/path.hpp>

#include <cstdlib>
#include <cwchar>
#include <locale>
#include <map>
#include <string>
#include <vector>

namespace {

// Clears LANG and every LC_* variable for its lifetime, and restores them afterwards.
class clean_locale_environment
{
public:
  clean_locale_environment()
  {
    for( const char* name : _names )
    {
      if( const char* value = std::getenv( name ) )
        _saved.emplace( name, value );
      unsetenv( name );
    }
  }

  ~clean_locale_environment()
  {
    for( const char* name : _names )
      unsetenv( name );
    for( const auto& [ name, value ] : _saved )
      setenv( name.c_str(), value.c_str(), 1 );
  }

private:
  static constexpr const char* _names[] = { "LC_ALL", "LC_CTYPE", "LC_NUMERIC", "LC_TIME", "LC_COLLATE", "LC_MONETARY",
                                            "LC_MESSAGES", "LC_PAPER", "LC_NAME", "LC_ADDRESS", "LC_TELEPHONE",
                                            "LC_MEASUREMENT", "LC_IDENTIFICATION", "LANG" };
  std::map< std::string, std::string > _saved;
};

} // namespace

#ifndef BOOST_WINDOWS_API

namespace {

std::string hex( const std::string& bytes )
{
  static const char* digits = "0123456789abcdef";
  std::string result;
  for( unsigned char c : bytes )
  {
    result += digits[ c >> 4 ];
    result += digits[ c & 15 ];
  }
  return result;
}

// A codecvt facet that fails every conversion. Imbued into Boost.Filesystem, it makes any narrow <-> wide
// path conversion throw, so code that passes with it installed does not depend on the locale at all.
class failing_codecvt : public std::codecvt< wchar_t, char, std::mbstate_t >
{
protected:
  result do_out( state_type&, const intern_type*, const intern_type*, const intern_type*&,
                 extern_type*, extern_type*, extern_type*& ) const override
  {
    return error;
  }

  result do_in( state_type&, const extern_type*, const extern_type*, const extern_type*&,
                intern_type*, intern_type*, intern_type*& ) const override
  {
    return error;
  }
};

class failing_path_locale
{
public:
  failing_path_locale()
    : _previous( boost::filesystem::path::imbue( std::locale( std::locale::classic(), new failing_codecvt ) ) ) {}
  ~failing_path_locale() { boost::filesystem::path::imbue( _previous ); }

private:
  // Declared first: the first imbue() builds Boost's default path locale, std::locale(""), which must not
  // fail just because the test runs in an environment that names an uninstalled locale.
  clean_locale_environment _environment;
  std::locale _previous;
};

const std::vector< std::string > sample_paths = {
  "/tmp/ascii/file.txt",
  "relative/dir",
  "/tmp/za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87",  // UTF-8
  "/tmp/latin2-\xb1\xea"                      // ISO-8859-2 bytes, not valid UTF-8
};

} // namespace

BOOST_AUTO_TEST_SUITE( path_locale_tests )

BOOST_AUTO_TEST_CASE( path_conversions_do_not_depend_on_locale )
{
  failing_path_locale guard;

  for( size_t i = 0; i < sample_paths.size(); ++i )
  {
    BOOST_TEST_CONTEXT( "sample_paths[" << i << "]" )
    {
      const std::string& sample = sample_paths[ i ];
      const fc::path p( sample );

      fc::variant v;
      BOOST_CHECK_NO_THROW( fc::to_variant( p, v ) );
      BOOST_CHECK_EQUAL( hex( v.is_string() ? v.as_string() : std::string() ), hex( sample ) );

      fc::path round_trip;
      BOOST_CHECK_NO_THROW( fc::from_variant( fc::variant( sample ), round_trip ) );
      BOOST_CHECK_EQUAL( hex( round_trip.string() ), hex( sample ) );

      std::string native;
      BOOST_CHECK_NO_THROW( native = p.to_native_ansi_path() );
      BOOST_CHECK_EQUAL( hex( native ), hex( sample ) );
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()

#endif // BOOST_WINDOWS_API
