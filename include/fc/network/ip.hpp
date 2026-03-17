#pragma once
#include <fc/string.hpp>
#include <fc/crypto/sha1.hpp>
#include <fc/io/raw_fwd.hpp>
#include <fc/crypto/city.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/static_variant.hpp>
#include <fc/array.hpp>
#include <array>
#include <cstdint>

namespace fc {

  namespace ip {

    struct ipv4_address {
      uint32_t addr = 0;

      ipv4_address() = default;
      explicit ipv4_address(uint32_t a) : addr(a) {}

      bool operator==(const ipv4_address& other) const { return addr == other.addr; }
      bool operator!=(const ipv4_address& other) const { return addr != other.addr; }
      bool operator<(const ipv4_address& other) const { return addr < other.addr; }
    };

    struct ipv6_address {
      fc::array<uint8_t, 16> addr;

      ipv6_address() = default;
      explicit ipv6_address(const std::array<uint8_t, 16>& a) {
        std::copy(a.begin(), a.end(), addr.begin());
      }

      bool operator==(const ipv6_address& other) const { return addr == other.addr; }
      bool operator!=(const ipv6_address& other) const { return addr != other.addr; }
      bool operator<(const ipv6_address& other) const { return addr < other.addr; }
    };

    class address {
      public:
        address();
        explicit address(uint32_t ip);
        explicit address(const fc::string& s);
        address(const ipv4_address& v4);
        address(const ipv6_address& v6);

        address& operator=(const fc::string& s);
        operator fc::string() const;
        operator uint32_t() const; // throws if IPv6

        bool is_ipv4() const;
        bool is_ipv6() const;
        const ipv4_address& get_ipv4() const;
        const ipv6_address& get_ipv6() const;

        friend bool operator==(const address& a, const address& b);
        friend bool operator!=(const address& a, const address& b);
        friend bool operator<(const address& a, const address& b);

        bool is_private_address() const;
        bool is_multicast_address() const;
        bool is_loopback_address() const;
        bool is_public_address() const;

        /// Returns true if this is an IPv4-mapped IPv6 address (::ffff:x.x.x.x)
        bool is_ipv4_mapped_ipv6() const;
        /// If this is an IPv4-mapped IPv6 address, returns the IPv4 equivalent.
        /// If already IPv4, returns *this. Throws if native IPv6.
        address to_ipv4_address() const;

      private:
        fc::static_variant<ipv4_address, ipv6_address> _addr;
    };

    class endpoint {
      public:
        endpoint();
        endpoint(const address& i, uint16_t p = 0);
        endpoint(const fc::string& addr_str, uint16_t p);

        static endpoint from_string(const string& s);
        operator string() const;

        void set_port(uint16_t p) { _port = p; }
        uint16_t port() const;
        const address& get_address() const;

        friend bool operator==(const endpoint& a, const endpoint& b);
        friend bool operator!=(const endpoint& a, const endpoint& b);
        friend bool operator<(const endpoint& a, const endpoint& b);

      private:
        uint32_t _port = 0;
        address  _ip;
    };

    /**
     * Legacy types that preserve the old wire format (IPv4-only).
     * Used by legacy_* message types for backward compatibility.
     */
    class legacy_address {
      public:
        legacy_address() : _ip(0) {}
        explicit legacy_address(uint32_t ip) : _ip(ip) {}

        /// Convert from new address type (throws if IPv6)
        explicit legacy_address(const address& addr);

        operator uint32_t() const { return _ip; }

        /// Convert to new address type
        address to_address() const { return address(_ip); }

        friend bool operator==(const legacy_address& a, const legacy_address& b) { return a._ip == b._ip; }
        friend bool operator!=(const legacy_address& a, const legacy_address& b) { return a._ip != b._ip; }

      private:
        uint32_t _ip;
    };

    class legacy_endpoint {
      public:
        legacy_endpoint() : _port(0) {}
        legacy_endpoint(const legacy_address& addr, uint16_t p) : _port(p), _ip(addr) {}

        /// Convert from new endpoint type (throws if IPv6)
        explicit legacy_endpoint(const endpoint& ep);

        uint16_t port() const { return _port; }
        const legacy_address& get_address() const { return _ip; }

        /// Convert to new endpoint type
        endpoint to_endpoint() const { return endpoint(_ip.to_address(), _port); }

        friend bool operator==(const legacy_endpoint& a, const legacy_endpoint& b) {
          return a._port == b._port && a._ip == b._ip;
        }
        friend bool operator!=(const legacy_endpoint& a, const legacy_endpoint& b) { return !(a == b); }

      private:
        uint16_t _port;
        legacy_address _ip;
    };

  } // namespace ip

  class variant;
  void to_variant(const ip::endpoint& var, variant& vo);
  void from_variant(const variant& var, ip::endpoint& vo);

  void to_variant(const ip::address& var, variant& vo);
  void from_variant(const variant& var, ip::address& vo);

  void to_variant(const ip::ipv4_address& var, variant& vo);
  void from_variant(const variant& var, ip::ipv4_address& vo);

  void to_variant(const ip::ipv6_address& var, variant& vo);
  void from_variant(const variant& var, ip::ipv6_address& vo);

  namespace raw
  {
    // Pack/unpack for ipv4_address
    template<typename Stream>
    inline void pack(Stream& s, const ip::ipv4_address& v)
    {
      fc::raw::pack(s, v.addr);
    }

    template<typename Stream>
    inline void unpack(Stream& s, ip::ipv4_address& v, uint32_t depth, bool limit_is_disabled)
    {
      depth++;
      fc::raw::unpack(s, v.addr, depth);
    }

    // Pack/unpack for ipv6_address
    template<typename Stream>
    inline void pack(Stream& s, const ip::ipv6_address& v)
    {
      fc::raw::pack(s, v.addr);
    }

    template<typename Stream>
    inline void unpack(Stream& s, ip::ipv6_address& v, uint32_t depth, bool limit_is_disabled)
    {
      depth++;
      fc::raw::unpack(s, v.addr, depth);
    }

    // Pack/unpack for address (new format: 1 byte family + address bytes)
    template<typename Stream>
    inline void pack(Stream& s, const ip::address& v)
    {
      if (v.is_ipv4()) {
        fc::raw::pack(s, uint8_t(0x04));
        fc::raw::pack(s, v.get_ipv4());
      } else {
        fc::raw::pack(s, uint8_t(0x06));
        fc::raw::pack(s, v.get_ipv6());
      }
    }

    template<typename Stream>
    inline void unpack(Stream& s, ip::address& v, uint32_t depth, bool limit_is_disabled)
    {
      depth++;
      uint8_t family;
      fc::raw::unpack(s, family, depth);
      if (family == 0x04) {
        ip::ipv4_address v4;
        fc::raw::unpack(s, v4, depth);
        v = ip::address(v4);
      } else if (family == 0x06) {
        ip::ipv6_address v6;
        fc::raw::unpack(s, v6, depth);
        v = ip::address(v6);
      } else {
        FC_THROW_EXCEPTION(fc::parse_error_exception, "Invalid IP address family: ${f}", ("f", int(family)));
      }
    }

    // Pack/unpack for endpoint (uses new address format)
    template<typename Stream>
    inline void pack(Stream& s, const ip::endpoint& v)
    {
      fc::raw::pack(s, v.get_address());
      fc::raw::pack(s, v.port());
    }

    template<typename Stream>
    inline void unpack(Stream& s, ip::endpoint& v, uint32_t depth, bool limit_is_disabled)
    {
      depth++;
      ip::address a;
      uint16_t p;
      fc::raw::unpack(s, a, depth);
      fc::raw::unpack(s, p, depth);
      v = ip::endpoint(a, p);
    }

    // Pack/unpack for legacy_address (old wire format: just uint32_t)
    template<typename Stream>
    inline void pack(Stream& s, const ip::legacy_address& v)
    {
      fc::raw::pack(s, uint32_t(v));
    }

    template<typename Stream>
    inline void unpack(Stream& s, ip::legacy_address& v, uint32_t depth, bool limit_is_disabled)
    {
      depth++;
      uint32_t ip;
      fc::raw::unpack(s, ip, depth);
      v = ip::legacy_address(ip);
    }

    // Pack/unpack for legacy_endpoint (old wire format: address uint32 + port uint16)
    template<typename Stream>
    inline void pack(Stream& s, const ip::legacy_endpoint& v)
    {
      fc::raw::pack(s, v.get_address());
      fc::raw::pack(s, v.port());
    }

    template<typename Stream>
    inline void unpack(Stream& s, ip::legacy_endpoint& v, uint32_t depth, bool limit_is_disabled)
    {
      depth++;
      ip::legacy_address a;
      uint16_t p;
      fc::raw::unpack(s, a, depth);
      fc::raw::unpack(s, p, depth);
      v = ip::legacy_endpoint(a, p);
    }

  } // namespace raw

} // namespace fc

FC_REFLECT(fc::ip::ipv4_address, (addr))
FC_REFLECT(fc::ip::ipv6_address, (addr))
FC_REFLECT_TYPENAME(fc::ip::address)
FC_REFLECT_TYPENAME(fc::ip::endpoint)
FC_REFLECT_TYPENAME(fc::ip::legacy_address)
FC_REFLECT_TYPENAME(fc::ip::legacy_endpoint)

namespace std
{
  template<>
  struct hash<fc::ip::endpoint>
  {
    size_t operator()(const fc::ip::endpoint& e) const;
  };

  template<>
  struct hash<fc::ip::address>
  {
    size_t operator()(const fc::ip::address& a) const;
  };
}
