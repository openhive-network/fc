#include <fc/time.hpp>
#include <fc/variant.hpp>
#include <boost/chrono/system_clocks.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <utility>
#include <fc/string.hpp>
#include <fc/io/sstream.hpp>
#include <fc/exception/exception.hpp>

namespace fc {

  namespace bch = boost::chrono;

  namespace
  {
    bool starts_with_ignoring_case( const std::string& s, const char* lowercase )
    {
      for( size_t i = 0; lowercase[i] != '\0'; ++i )
      {
        if( i == s.size() )
          return false;
        char c = s[i];
        if( c >= 'A' && c <= 'Z' )
          c = char( c - 'A' + 'a' );
        if( c != lowercase[i] )
          return false;
      }
      return true;
    }

    /// boost::posix_time::from_iso_string() (parse_iso_time()) without the special value parser that reads the
    /// string through a stream: the same special values, matched the same way (the beginning of the string, any case)
    boost::posix_time::ptime parse_iso_time( const std::string& s )
    {
      static const std::pair< const char*, boost::date_time::special_values > special_values[] = {
        { "not-a-date-time", boost::date_time::not_a_date_time }, { "-infinity", boost::date_time::neg_infin },
        { "+infinity", boost::date_time::pos_infin }, { "minimum-date-time", boost::date_time::min_date_time },
        { "maximum-date-time", boost::date_time::max_date_time } };
      if( !s.empty() && ( s[0] == '+' || s[0] == '-' || s[0] == 'n' || s[0] == 'm' ) )
        for( const auto& special : special_values )
          if( starts_with_ignoring_case( s, special.first ) )
            return boost::posix_time::ptime( special.second );

      std::string date_string, tod_string;
      boost::date_time::split( s, 'T', date_string, tod_string );
      boost::gregorian::date d = boost::date_time::parse_undelimited_date< boost::gregorian::date >( date_string );
      boost::posix_time::time_duration td =
        boost::date_time::parse_undelimited_time_duration< boost::posix_time::time_duration >( tod_string );
      return boost::posix_time::ptime( d, td );
    }

    /// The month number of boost's parse_date(): a number, or an English month name or its abbreviation in any case
    /// (lowercased in ASCII as the classic locale does); 13, out of range, for anything else.
    unsigned short month_from_string( const std::string& s )
    {
      if( ( s.at( 0 ) >= '0' ) && ( s.at( 0 ) <= '9' ) )
        return boost::lexical_cast< unsigned short >( s );
      static const char* const names[] = { "jan", "january", "feb", "february", "mar", "march", "apr", "april", "may",
        "may", "jun", "june", "jul", "july", "aug", "august", "sep", "september", "oct", "october", "nov", "november",
        "dec", "december" };
      for( unsigned short i = 0; i < 24; ++i )
        if( s.size() == strlen( names[i] ) && starts_with_ignoring_case( s, names[i] ) )
          return static_cast< unsigned short >( i / 2 + 1 );
      return 13;
    }

    /// boost::date_time::parse_date() (the year-month-day order) with month_from_string()
    boost::gregorian::date parse_date( const std::string& s )
    {
      unsigned pos = 0;
      unsigned short year(0), month(0), day(0);
      typedef boost::char_separator< char, std::string::traits_type > char_separator_type;
      typedef boost::tokenizer< char_separator_type, std::string::const_iterator, std::string > tokenizer;
      const char sep_char[] = {',','-','.',' ','/','\0'};
      char_separator_type sep( sep_char );
      tokenizer tok( s, sep );
      for( auto beg = tok.begin(); beg != tok.end() && pos < 3; ++beg, ++pos )
      {
        switch( pos )
        {
          case 0: year = boost::lexical_cast< unsigned short >( *beg ); break;
          case 1: month = month_from_string( *beg ); break;
          default: day = boost::lexical_cast< unsigned short >( *beg ); break;
        }
      }
      return boost::gregorian::date( year, month, day );
    }

    /// boost::date_time::parse_delimited_time() with parse_date()
    boost::posix_time::ptime parse_delimited_time( const std::string& s )
    {
      std::string date_string, tod_string;
      boost::date_time::split( s, 'T', date_string, tod_string );
      boost::gregorian::date d = parse_date( date_string );
      boost::posix_time::time_duration td =
        boost::date_time::parse_delimited_time_duration< boost::posix_time::time_duration >( tod_string );
      return boost::posix_time::ptime( d, td );
    }

    /// The date and time of day of a second since the epoch, as boost's to_iso_extended_string() (delimited) or
    /// to_iso_string() writes them for a whole second
    fc::string format_time( int32_t seconds, bool delimited )
    {
      const auto ptime = boost::posix_time::from_time_t( seconds );
      const auto date = ptime.date();
      const auto time = ptime.time_of_day();
      char buf[32];
      snprintf( buf, sizeof( buf ), delimited ? "%04d-%02d-%02dT%02d:%02d:%02d" : "%04d%02d%02dT%02d%02d%02d",
                int( date.year() ), int( date.month() ), int( date.day() ), int( time.hours() ), int( time.minutes() ),
                int( time.seconds() ) );
      return buf;
    }
  }

  time_point time_point::now()
  {
     return time_point( microseconds( bch::duration_cast<bch::microseconds>( bch::system_clock::now().time_since_epoch() ).count() ) );
  }

  fc::string time_point_sec::to_non_delimited_iso_string()const
  {
    return format_time( static_cast< int32_t >( sec_since_epoch() ), false );
  }

  fc::string time_point_sec::to_iso_string()const
  {
    return format_time( static_cast< int32_t >( sec_since_epoch() ), true );
  }

  time_point_sec::operator fc::string()const
  {
      return this->to_iso_string();
  }

  time_point_sec time_point_sec::from_iso_string( const fc::string& s )
  { try {
      static boost::posix_time::ptime epoch = boost::posix_time::from_time_t( 0 );
      boost::posix_time::ptime pt;
      if( s.size() >= 5 && s.at( 4 ) == '-' ) // http://en.wikipedia.org/wiki/ISO_8601
          pt = parse_delimited_time( s );
      else
          pt = parse_iso_time( s );

      FC_ASSERT( (pt - epoch).total_seconds() <= INT32_MAX, "Datetime overflow" );
      FC_ASSERT( (pt - epoch).total_seconds() >= INT32_MIN, "Datetime underflow" );
      return fc::time_point_sec( (pt - epoch).total_seconds() );
  } FC_RETHROW_EXCEPTIONS( warn, "unable to convert ISO-formatted string to fc::time_point_sec" ) }

  time_point::operator fc::string()const
  {
    return fc::string( time_point_sec( *this ) );
  }

  fc::string time_point::to_iso_string_in_seconds() const
  {
    return fc::string( time_point_sec( *this ) );
  }

  fc::string time_point::to_iso_string_in_milliseconds() const
  {
    uint64_t milliseconds = (time_since_epoch().count() % 1000000) / 1000;
    char buf[32];
    snprintf( buf, sizeof( buf ), ".%03llu", static_cast< unsigned long long >( milliseconds ) );
    return (std::string)(*this) + buf;
  }

  fc::string time_point::to_iso_string_in_microseconds() const
  {
    uint64_t microseconds = time_since_epoch().count() % 1000000;
    char buf[32];
    snprintf( buf, sizeof( buf ), ".%06llu", static_cast< unsigned long long >( microseconds ) );
    return (std::string)(*this) + buf;
  }

  time_point time_point::from_iso_string( const fc::string& s )
  { try {
      return time_point( time_point_sec::from_iso_string( s ) );
  } FC_RETHROW_EXCEPTIONS( warn, "unable to convert ISO-formatted string to fc::time_point" ) }

  void to_variant( const fc::time_point& t, variant& v ) {
    v = fc::string( t );
  }
  void from_variant( const fc::variant& v, fc::time_point& t ) {
    t = fc::time_point::from_iso_string( v.as_string() );
  }
  void to_variant( const fc::time_point_sec& t, variant& v ) {
    v = fc::string( t );
  }
  void from_variant( const fc::variant& v, fc::time_point_sec& t ) {
    t = fc::time_point_sec::from_iso_string( v.as_string() );
  }

  // inspired by show_date_relative() in git's date.c
  string get_approximate_relative_time_string(const time_point_sec& event_time,
                                              const time_point_sec& relative_to_time /* = fc::time_point::now() */,
                                              const std::string& default_ago /* = " ago" */) {


    string ago = default_ago;
    int32_t seconds_ago = relative_to_time.sec_since_epoch() - event_time.sec_since_epoch();
    if (seconds_ago < 0)
    {
       ago = " in the future";
       seconds_ago = -seconds_ago;
    }
    stringstream result;
    if (seconds_ago < 90)
    {
      result << seconds_ago << " second" << (seconds_ago > 1 ? "s" : "") << ago;
      return result.str();
    }
    uint32_t minutes_ago = (seconds_ago + 30) / 60;
    if (minutes_ago < 90)
    {
      result << minutes_ago << " minute" << (minutes_ago > 1 ? "s" : "") << ago;
      return result.str();
    }
    uint32_t hours_ago = (minutes_ago + 30) / 60;
    if (hours_ago < 90)
    {
      result << hours_ago << " hour" << (hours_ago > 1 ? "s" : "") << ago;
      return result.str();
    }
    uint32_t days_ago = (hours_ago + 12) / 24;
    if (days_ago < 90)
    {
      result << days_ago << " day" << (days_ago > 1 ? "s" : "") << ago;
      return result.str();
    }
    uint32_t weeks_ago = (days_ago + 3) / 7;
    if (weeks_ago < 70)
    {
      result << weeks_ago << " week" << (weeks_ago > 1 ? "s" : "") << ago;
      return result.str();
    }
    uint32_t months_ago = (days_ago + 15) / 30;
    if (months_ago < 12)
    {
      result << months_ago << " month" << (months_ago > 1 ? "s" : "") << ago;
      return result.str();
    }
    uint32_t years_ago = days_ago / 365;
    result << years_ago << " year" << (months_ago > 1 ? "s" : "");
    if (months_ago < 12 * 5)
    {
      uint32_t leftover_days = days_ago - (years_ago * 365);
      uint32_t leftover_months = (leftover_days + 15) / 30;
      if (leftover_months)
        result << leftover_months <<  " month" << (months_ago > 1 ? "s" : "");
    }
    result << ago;
    return result.str();
  }
  string get_approximate_relative_time_string(const time_point& event_time,
                                              const time_point& relative_to_time /* = fc::time_point::now() */,
                                              const std::string& ago /* = " ago" */) {
    return get_approximate_relative_time_string(time_point_sec(event_time), time_point_sec(relative_to_time), ago);
  }

  void to_variant( const microseconds& input_microseconds,  variant& output_variant )
  {
    output_variant = input_microseconds.count();
  }
  void from_variant( const variant& input_variant,  microseconds& output_microseconds )
  {
    output_microseconds = microseconds(input_variant.as_int64());
  }

} //namespace fc
