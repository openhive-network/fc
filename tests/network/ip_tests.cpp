#include <boost/test/unit_test.hpp>

#include <fc/network/ip.hpp>
#include <fc/io/raw.hpp>
#include <fc/variant.hpp>

BOOST_AUTO_TEST_SUITE(ip_tests)

// ============ ipv4_address ============

BOOST_AUTO_TEST_CASE(ipv4_address_basic)
{
  fc::ip::ipv4_address a(0x01020304); // 1.2.3.4
  BOOST_CHECK_EQUAL(a.addr, 0x01020304u);
  BOOST_CHECK_EQUAL((std::string)a, "1.2.3.4");
}

BOOST_AUTO_TEST_CASE(ipv4_address_from_string)
{
  fc::ip::ipv4_address a("192.168.1.1");
  BOOST_CHECK_EQUAL((std::string)a, "192.168.1.1");
  BOOST_CHECK_EQUAL(a.addr, (192u << 24) | (168u << 16) | (1u << 8) | 1u);
}

// ============ ipv6_address ============

BOOST_AUTO_TEST_CASE(ipv6_loopback)
{
  fc::ip::ipv6_address lo; // fc::array<unsigned char, 16> is default zero-initialized
  lo.addr.data[15] = 1;
  BOOST_CHECK_EQUAL((std::string)lo, "::1");
}

BOOST_AUTO_TEST_CASE(ipv6_from_string)
{
  fc::ip::ipv6_address a("::1");
  BOOST_CHECK_EQUAL(a.addr.data[15], 1);
  for (int i = 0; i < 15; ++i)
    BOOST_CHECK_EQUAL(a.addr.data[i], 0);
}

BOOST_AUTO_TEST_CASE(ipv6_full_address)
{
  fc::ip::ipv6_address a("2001:db8::1");
  // 2001:0db8:0000:0000:0000:0000:0000:0001
  BOOST_CHECK_EQUAL(a.addr.data[0], 0x20);
  BOOST_CHECK_EQUAL(a.addr.data[1], 0x01);
  BOOST_CHECK_EQUAL(a.addr.data[2], 0x0d);
  BOOST_CHECK_EQUAL(a.addr.data[3], 0xb8);
  BOOST_CHECK_EQUAL(a.addr.data[15], 0x01);
}

// ============ address (polymorphic) ============

BOOST_AUTO_TEST_CASE(address_ipv4)
{
  fc::ip::address a(fc::ip::ipv4_address("10.0.0.1"));
  BOOST_CHECK(a.is_ipv4());
  BOOST_CHECK(!a.is_ipv6());
  BOOST_CHECK_EQUAL((std::string)a, "10.0.0.1");
}

BOOST_AUTO_TEST_CASE(address_ipv6)
{
  fc::ip::address a(fc::ip::ipv6_address("::1"));
  BOOST_CHECK(!a.is_ipv4());
  BOOST_CHECK(a.is_ipv6());
  BOOST_CHECK_EQUAL((std::string)a, "::1");
}

BOOST_AUTO_TEST_CASE(address_from_string_ipv4)
{
  fc::ip::address a("1.2.3.4");
  BOOST_CHECK(a.is_ipv4());
  BOOST_CHECK_EQUAL((std::string)a, "1.2.3.4");
}

BOOST_AUTO_TEST_CASE(address_from_string_ipv6)
{
  fc::ip::address a("::1");
  BOOST_CHECK(a.is_ipv6());
  BOOST_CHECK_EQUAL((std::string)a, "::1");
}

BOOST_AUTO_TEST_CASE(address_loopback)
{
  fc::ip::address lo4("127.0.0.1");
  BOOST_CHECK(lo4.is_loopback_address());

  fc::ip::address lo6("::1");
  BOOST_CHECK(lo6.is_loopback_address());

  fc::ip::address not_lo("8.8.8.8");
  BOOST_CHECK(!not_lo.is_loopback_address());
}

// ============ endpoint ============

BOOST_AUTO_TEST_CASE(endpoint_ipv4)
{
  fc::ip::endpoint ep(fc::ip::address("1.2.3.4"), 8080);
  BOOST_CHECK_EQUAL(ep.port(), 8080);
  BOOST_CHECK(ep.get_address().is_ipv4());
}

BOOST_AUTO_TEST_CASE(endpoint_ipv6)
{
  fc::ip::endpoint ep(fc::ip::address("::1"), 9090);
  BOOST_CHECK_EQUAL(ep.port(), 9090);
  BOOST_CHECK(ep.get_address().is_ipv6());
}

BOOST_AUTO_TEST_CASE(endpoint_from_string_ipv4)
{
  fc::ip::endpoint ep = fc::ip::endpoint::from_string("1.2.3.4:8080");
  BOOST_CHECK_EQUAL(ep.port(), 8080);
  BOOST_CHECK_EQUAL((std::string)ep.get_address(), "1.2.3.4");
}

BOOST_AUTO_TEST_CASE(endpoint_from_string_ipv6)
{
  fc::ip::endpoint ep = fc::ip::endpoint::from_string("[::1]:8080");
  BOOST_CHECK_EQUAL(ep.port(), 8080);
  BOOST_CHECK(ep.get_address().is_ipv6());
  BOOST_CHECK_EQUAL((std::string)ep.get_address(), "::1");
}

// ============ legacy_address ============

BOOST_AUTO_TEST_CASE(legacy_address_roundtrip)
{
  fc::ip::legacy_address la(0x01020304);
  BOOST_CHECK_EQUAL(la.ip(), 0x01020304u);

  fc::ip::address a = la.to_address();
  BOOST_CHECK(a.is_ipv4());
  BOOST_CHECK_EQUAL(a.get_ipv4().addr, 0x01020304u);
}

BOOST_AUTO_TEST_CASE(legacy_address_from_address)
{
  fc::ip::address a(fc::ip::ipv4_address(0xC0A80101)); // 192.168.1.1
  fc::ip::legacy_address la(a);
  BOOST_CHECK_EQUAL(la.ip(), 0xC0A80101u);
}

BOOST_AUTO_TEST_CASE(legacy_address_rejects_ipv6)
{
  fc::ip::address a(fc::ip::ipv6_address("::1"));
  BOOST_CHECK_THROW(fc::ip::legacy_address(a), fc::exception);
}

// ============ legacy_endpoint ============

BOOST_AUTO_TEST_CASE(legacy_endpoint_roundtrip)
{
  fc::ip::endpoint ep(fc::ip::address(fc::ip::ipv4_address(0x01020304)), 8080);
  fc::ip::legacy_endpoint le(ep);
  BOOST_CHECK_EQUAL(le.port(), 8080);

  fc::ip::endpoint ep2 = le.to_endpoint();
  BOOST_CHECK_EQUAL(ep2.port(), 8080);
  BOOST_CHECK(ep2.get_address().is_ipv4());
  BOOST_CHECK_EQUAL(ep2.get_address().get_ipv4().addr, 0x01020304u);
}

BOOST_AUTO_TEST_CASE(legacy_endpoint_rejects_ipv6)
{
  fc::ip::endpoint ep(fc::ip::address(fc::ip::ipv6_address("::1")), 8080);
  BOOST_CHECK_THROW(fc::ip::legacy_endpoint(ep), fc::exception);
}

// ============ Serialization ============

BOOST_AUTO_TEST_CASE(address_serialization_ipv4_roundtrip)
{
  fc::ip::address original(fc::ip::ipv4_address(0x01020304));
  auto packed = fc::raw::pack(original);
  // IPv4: 1 byte family (0x04) + 4 bytes address = 5 bytes
  BOOST_CHECK_EQUAL(packed.size(), 5u);
  BOOST_CHECK_EQUAL(packed[0], 0x04);

  fc::ip::address unpacked;
  fc::raw::unpack(packed, unpacked);
  BOOST_CHECK(unpacked.is_ipv4());
  BOOST_CHECK_EQUAL(unpacked.get_ipv4().addr, 0x01020304u);
}

BOOST_AUTO_TEST_CASE(address_serialization_ipv6_roundtrip)
{
  fc::ip::address original(fc::ip::ipv6_address("2001:db8::1"));
  auto packed = fc::raw::pack(original);
  // IPv6: 1 byte family (0x06) + 16 bytes address = 17 bytes
  BOOST_CHECK_EQUAL(packed.size(), 17u);
  BOOST_CHECK_EQUAL(packed[0], 0x06);

  fc::ip::address unpacked;
  fc::raw::unpack(packed, unpacked);
  BOOST_CHECK(unpacked.is_ipv6());
  BOOST_CHECK_EQUAL((std::string)unpacked.get_ipv6(), "2001:db8::1");
}

BOOST_AUTO_TEST_CASE(legacy_address_serialization_roundtrip)
{
  fc::ip::legacy_address original(0xC0A80101);
  auto packed = fc::raw::pack(original);
  // Legacy: just 4 bytes (uint32_t), same as old wire format
  BOOST_CHECK_EQUAL(packed.size(), 4u);

  fc::ip::legacy_address unpacked;
  fc::raw::unpack(packed, unpacked);
  BOOST_CHECK_EQUAL(unpacked.ip(), 0xC0A80101u);
}

BOOST_AUTO_TEST_CASE(endpoint_serialization_ipv4_roundtrip)
{
  fc::ip::endpoint original(fc::ip::address(fc::ip::ipv4_address(0x01020304)), 8080);
  auto packed = fc::raw::pack(original);

  fc::ip::endpoint unpacked;
  fc::raw::unpack(packed, unpacked);
  BOOST_CHECK_EQUAL(unpacked.port(), 8080);
  BOOST_CHECK(unpacked.get_address().is_ipv4());
  BOOST_CHECK_EQUAL(unpacked.get_address().get_ipv4().addr, 0x01020304u);
}

BOOST_AUTO_TEST_CASE(endpoint_serialization_ipv6_roundtrip)
{
  fc::ip::endpoint original(fc::ip::address(fc::ip::ipv6_address("::1")), 9090);
  auto packed = fc::raw::pack(original);

  fc::ip::endpoint unpacked;
  fc::raw::unpack(packed, unpacked);
  BOOST_CHECK_EQUAL(unpacked.port(), 9090);
  BOOST_CHECK(unpacked.get_address().is_ipv6());
  BOOST_CHECK_EQUAL((std::string)unpacked.get_address(), "::1");
}

BOOST_AUTO_TEST_CASE(legacy_endpoint_serialization_roundtrip)
{
  fc::ip::legacy_endpoint original(fc::ip::legacy_address(0x01020304), 8080);
  auto packed = fc::raw::pack(original);
  // Legacy endpoint: 4 bytes address + 2 bytes port = 6 bytes
  BOOST_CHECK_EQUAL(packed.size(), 6u);

  fc::ip::legacy_endpoint unpacked;
  fc::raw::unpack(packed, unpacked);
  BOOST_CHECK_EQUAL(unpacked.port(), 8080);
  BOOST_CHECK_EQUAL(unpacked.get_address().ip(), 0x01020304u);
}

// ============ Variant (JSON) roundtrip ============

BOOST_AUTO_TEST_CASE(address_variant_ipv4)
{
  fc::ip::address original("1.2.3.4");
  fc::variant v;
  fc::to_variant(original, v);
  BOOST_CHECK_EQUAL(v.as_string(), "1.2.3.4");

  fc::ip::address restored;
  fc::from_variant(v, restored);
  BOOST_CHECK(restored.is_ipv4());
  BOOST_CHECK_EQUAL((std::string)restored, "1.2.3.4");
}

BOOST_AUTO_TEST_CASE(address_variant_ipv6)
{
  fc::ip::address original("::1");
  fc::variant v;
  fc::to_variant(original, v);

  fc::ip::address restored;
  fc::from_variant(v, restored);
  BOOST_CHECK(restored.is_ipv6());
  BOOST_CHECK_EQUAL((std::string)restored, "::1");
}

BOOST_AUTO_TEST_CASE(endpoint_variant_ipv4)
{
  fc::ip::endpoint original(fc::ip::address("1.2.3.4"), 8080);
  fc::variant v;
  fc::to_variant(original, v);

  fc::ip::endpoint restored;
  fc::from_variant(v, restored);
  BOOST_CHECK_EQUAL(restored.port(), 8080);
  BOOST_CHECK_EQUAL((std::string)restored.get_address(), "1.2.3.4");
}

BOOST_AUTO_TEST_CASE(endpoint_variant_ipv6)
{
  fc::ip::endpoint original(fc::ip::address("::1"), 9090);
  fc::variant v;
  fc::to_variant(original, v);

  fc::ip::endpoint restored;
  fc::from_variant(v, restored);
  BOOST_CHECK_EQUAL(restored.port(), 9090);
  BOOST_CHECK(restored.get_address().is_ipv6());
}

BOOST_AUTO_TEST_SUITE_END()
