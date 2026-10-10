#include <boost/test/unit_test.hpp>

#include <fc/filesystem.hpp>
#include <fc/locale.hpp>
#include <fc/variant.hpp>

#include <boost/filesystem/path.hpp>

#include <cstdlib>
#include <cwchar>
#include <iostream>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>
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

#ifndef _WIN32

namespace {

// A locale name that no system has installed.
const std::string uninstalled_locale = "xx_XX.UTF-8";

bool locale_can_be_loaded( const char* name )
{
  try
  {
    const std::locale probe( name );
    return true;
  }
  catch( const std::exception& )
  {
    return false;
  }
}

std::string environment_value( const char* name )
{
  const char* value = std::getenv( name );
  BOOST_REQUIRE_MESSAGE( value != nullptr, name << " is not set" );
  return value;
}

bool ends_with( const std::string& text, const std::string& end )
{
  return text.size() >= end.size() && text.compare( text.size() - end.size(), end.size(), end ) == 0;
}

// Captures std::cerr for its lifetime.
class captured_cerr
{
public:
  captured_cerr() : _previous( std::cerr.rdbuf( _buffer.rdbuf() ) ) {}
  ~captured_cerr() { std::cerr.rdbuf( _previous ); }
  std::string text() const { return _buffer.str(); }

private:
  std::ostringstream _buffer;  // declared first: _previous is initialized with its buffer
  std::streambuf* _previous;
};

} // namespace

BOOST_AUTO_TEST_SUITE( sanitize_locale_environment_tests )

BOOST_AUTO_TEST_CASE( usable_environment_is_left_alone )
{
  clean_locale_environment environment;

  BOOST_CHECK( fc::sanitize_locale_environment( false ).empty() );  // nothing set at all

  setenv( "LANG", "C", 1 );
  BOOST_CHECK( fc::sanitize_locale_environment( false ).empty() );
  BOOST_CHECK( std::getenv( "LC_ALL" ) == nullptr );
  BOOST_CHECK_EQUAL( environment_value( "LANG" ), "C" );

  if( locale_can_be_loaded( "C.UTF-8" ) )  // a usable locale other than C, which libstdc++ short-circuits
  {
    setenv( "LANG", "C.UTF-8", 1 );
    BOOST_CHECK( fc::sanitize_locale_environment( false ).empty() );
    BOOST_CHECK( std::getenv( "LC_ALL" ) == nullptr );
  }
}

BOOST_AUTO_TEST_CASE( empty_values_count_as_unset )
{
  clean_locale_environment environment;
  setenv( "LC_ALL", "", 1 );
  setenv( "LC_TIME", "", 1 );
  setenv( "LANG", "C", 1 );

  BOOST_CHECK( fc::sanitize_locale_environment( false ).empty() );
  BOOST_CHECK_EQUAL( environment_value( "LC_ALL" ), "" );
}

BOOST_AUTO_TEST_CASE( usable_lc_all_hides_unusable_variables )
{
  clean_locale_environment environment;
  setenv( "LC_ALL", "C", 1 );
  setenv( "LANG", uninstalled_locale.c_str(), 1 );

  BOOST_CHECK( fc::sanitize_locale_environment( false ).empty() );
  BOOST_CHECK_EQUAL( environment_value( "LC_ALL" ), "C" );
}

BOOST_AUTO_TEST_CASE( unusable_lang_is_replaced )
{
  clean_locale_environment environment;
  setenv( "LANG", uninstalled_locale.c_str(), 1 );
  BOOST_REQUIRE_THROW( std::locale( "" ), std::runtime_error );

  const std::string description = fc::sanitize_locale_environment( false );

  BOOST_CHECK_MESSAGE( description.find( "LANG=" + uninstalled_locale ) != std::string::npos, description );
  BOOST_CHECK_NO_THROW( std::locale( "" ) );
  const std::string lc_all = environment_value( "LC_ALL" );
  BOOST_CHECK( lc_all == "C.UTF-8" || lc_all == "C" );
}

BOOST_AUTO_TEST_CASE( unusable_lc_all_is_replaced )
{
  clean_locale_environment environment;
  setenv( "LC_ALL", uninstalled_locale.c_str(), 1 );
  BOOST_REQUIRE_THROW( std::locale( "" ), std::runtime_error );

  const std::string description = fc::sanitize_locale_environment( false );

  BOOST_CHECK_MESSAGE( description.find( "LC_ALL=" + uninstalled_locale ) != std::string::npos, description );
  BOOST_CHECK_NO_THROW( std::locale( "" ) );
}

BOOST_AUTO_TEST_CASE( a_single_unusable_category_is_detected )
{
  clean_locale_environment environment;
  setenv( "LANG", "C", 1 );
  setenv( "LC_TIME", uninstalled_locale.c_str(), 1 );
  BOOST_REQUIRE_THROW( std::locale( "" ), std::runtime_error );

  const std::string description = fc::sanitize_locale_environment( false );

  BOOST_CHECK_MESSAGE( description.find( "LC_TIME=" + uninstalled_locale ) != std::string::npos, description );
  BOOST_CHECK_MESSAGE( description.find( "LANG=" ) == std::string::npos, description );
  BOOST_CHECK_NO_THROW( std::locale( "" ) );
}

BOOST_AUTO_TEST_CASE( the_warning_is_printed_only_when_asked_to )
{
  clean_locale_environment environment;

  setenv( "LC_ALL", uninstalled_locale.c_str(), 1 );
  {
    captured_cerr captured;
    BOOST_CHECK( !fc::sanitize_locale_environment( false ).empty() );
    BOOST_CHECK_EQUAL( captured.text(), "" );
  }

  setenv( "LC_ALL", uninstalled_locale.c_str(), 1 );
  {
    captured_cerr captured;
    const std::string description = fc::sanitize_locale_environment();
    BOOST_CHECK( !description.empty() );
    BOOST_CHECK_EQUAL( captured.text(), "Warning: " + description + "\n" );
  }

  setenv( "LC_ALL", "C", 1 );
  {
    captured_cerr captured;
    BOOST_CHECK( fc::sanitize_locale_environment().empty() );
    BOOST_CHECK_EQUAL( captured.text(), "" );
  }
}

BOOST_AUTO_TEST_CASE( the_next_fallback_is_used_when_one_cannot_be_loaded )
{
  clean_locale_environment environment;
  setenv( "LC_ALL", uninstalled_locale.c_str(), 1 );

  const std::string description = fc::detail::replace_unusable_locale( { "xx_YY.UTF-8", "C" } );

  BOOST_CHECK_MESSAGE( ends_with( description, ", using LC_ALL=C" ), description );
  BOOST_CHECK_EQUAL( environment_value( "LC_ALL" ), "C" );
  BOOST_CHECK_NO_THROW( std::locale( "" ) );
}

BOOST_AUTO_TEST_CASE( lc_all_is_restored_when_no_fallback_can_be_loaded )
{
  clean_locale_environment environment;
  setenv( "LC_ALL", uninstalled_locale.c_str(), 1 );

  const std::string description = fc::detail::replace_unusable_locale( { "xx_YY.UTF-8", "xx_ZZ.UTF-8" } );

  const std::string expected_end = "none of the fallbacks (xx_YY.UTF-8, xx_ZZ.UTF-8) could be loaded either";
  BOOST_CHECK_MESSAGE( ends_with( description, expected_end ), description );
  BOOST_CHECK_EQUAL( environment_value( "LC_ALL" ), uninstalled_locale );

  unsetenv( "LC_ALL" );
  setenv( "LANG", uninstalled_locale.c_str(), 1 );
  fc::detail::replace_unusable_locale( { "xx_YY.UTF-8" } );
  BOOST_CHECK( std::getenv( "LC_ALL" ) == nullptr );
}

BOOST_AUTO_TEST_SUITE_END()

#endif // _WIN32
