// EH3D-NODE-BUNGARD-AUX - more host tests: the hashes at the edges of their blocks, base64 as the standard has it, the conversions of the temperature sensors at their limits, the ring of
// pulses across the wrap of its counter, the reading of a JPEG that is not quite what a camera makes, and the text rules that every node of the family shares.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "../core/civil_time.hpp"
#include "../core/hash.hpp"
#include "../core/net_text.hpp"
#include "../core/node_id.hpp"
#include "../core/pulse_ring.hpp"
#include "../core/rtp_jpeg.hpp"
#include "../core/semver.hpp"
#include "../core/thermo.hpp"

static int failures = 0;
static int checks = 0;
#define CHECK(condition)                                                \
  do {                                                                  \
    ++checks;                                                           \
    if (!(condition)) {                                                 \
      ++failures;                                                       \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #condition);  \
    }                                                                   \
  } while (0)

using namespace bungard_aux;
using namespace eh3d;

// ---- the hashes -------------------------------------------------------------------------------------------------------------------------

static std::string sha1_hex(const std::string& m) {
  const auto d = sha1_bytes(m);
  return to_hex(d.data(), d.size());
}

static void md5_known_answers_of_the_standard() {
  CHECK(md5_hex("") == "d41d8cd98f00b204e9800998ecf8427e");
  CHECK(md5_hex("a") == "0cc175b9c0f1b6a831c399e269772661");
  CHECK(md5_hex("abc") == "900150983cd24fb0d6963f7d28e17f72");
  CHECK(md5_hex("message digest") == "f96b697d7cb7938d525a2f31aaf161d0");
  CHECK(md5_hex("abcdefghijklmnopqrstuvwxyz") == "c3fcd3d76192e4007dfb496cca67e13b");
  CHECK(md5_hex("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") == "d174ab98d277d9f5a5611c2c9f419d9f");
  CHECK(md5_hex("12345678901234567890123456789012345678901234567890123456789012345678901234567890") == "57edf4a22be3c955ac49da2e2107b67a");
}

static void md5_across_the_edges_of_a_block() {
  // 55 bytes fit with the padding in one block, 56 need a second one, 64 are a block and a padding block
  CHECK(md5_hex(std::string(55, 'a')) == "ef1772b6dff9a122358552954ad0df65");
  CHECK(md5_hex(std::string(56, 'a')) == "3b0c8ac703f828b04c6c197006d17218");
  CHECK(md5_hex(std::string(57, 'a')) == "652b906d60af96844ebd21b674f35e93");
  CHECK(md5_hex(std::string(63, 'a')) == "b06521f39153d618550606be297466d5");
  CHECK(md5_hex(std::string(64, 'a')) == "014842d480b571495a4a0363793f7367");
  CHECK(md5_hex(std::string(65, 'a')) == "c743a45e0d2e6a95cb859adae0248435");
  CHECK(md5_hex(std::string(1000000, 'a')) == "7707d6ae4e027c70eea2a935c2296f21");
  // a byte that is zero or high is a byte like any other
  CHECK(md5_hex(std::string("a\0b", 3)) != md5_hex("a") && md5_hex(std::string("\xff\xfe", 2)) != md5_hex(""));
  CHECK(md5_hex(std::string("a\0b", 3)).size() == 32);
}

static void sha1_known_answers_of_the_standard() {
  CHECK(sha1_hex("") == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
  CHECK(sha1_hex("abc") == "a9993e364706816aba3e25717850c26c9cd0d89d");
  CHECK(sha1_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") == "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
  CHECK(sha1_hex("The quick brown fox jumps over the lazy dog") == "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12");
  CHECK(sha1_hex(std::string(1000000, 'a')) == "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
  CHECK(sha1_hex(std::string(55, 'a')) != sha1_hex(std::string(56, 'a')) && sha1_hex(std::string(63, 'a')) != sha1_hex(std::string(64, 'a')));
  // the handshake example of RFC 6455: the key and the fixed text give this accept value
  const auto accept = sha1_bytes("dGhlIHNhbXBsZSBub25jZQ==258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
  CHECK(base64_encode(accept.data(), accept.size()) == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
}

static void base64_as_the_standard_writes_it() {
  const char* plain[] = {"", "f", "fo", "foo", "foob", "fooba", "foobar"};
  const char* coded[] = {"", "Zg==", "Zm8=", "Zm9v", "Zm9vYg==", "Zm9vYmE=", "Zm9vYmFy"};
  for (int i = 0; i < 7; ++i) {
    CHECK(base64_encode(std::string(plain[i])) == coded[i]);
    CHECK(base64_decode(coded[i]) == plain[i]);
  }
  CHECK(base64_decode("Zm9v\r\nYmFy") == "foobar" && base64_decode(" Zm9v YmFy ") == "foobar");   // the breaks of a long line are not part of it
  CHECK(base64_decode("Zm9v!YmFy").empty() && base64_decode("Zm9v-YmFy").empty() && base64_decode("Zm9v_YmFy").empty());   // not the alphabet of this one
  std::string all;
  for (int i = 0; i < 256; ++i) all += static_cast<char>(i);
  for (std::size_t n : {std::size_t(0), std::size_t(1), std::size_t(2), std::size_t(3), std::size_t(254), std::size_t(255), std::size_t(256)}) {
    const std::string part = all.substr(0, n);
    const std::string coded_part = base64_encode(part);
    CHECK(coded_part.size() % 4 == 0 && base64_decode(coded_part) == part);
  }
  CHECK(base64_encode("\xfb\xff\xbf") == "+/+/");
}

static void secrets_are_compared_whole() {
  CHECK(same_secret("", "") && same_secret("abc", "abc") && same_secret(std::string("a\0b", 3), std::string("a\0b", 3)));
  CHECK(!same_secret("abc", "abd") && !same_secret("abc", "abcd") && !same_secret("abcd", "abc") && !same_secret("", "a") && !same_secret("a", ""));
  CHECK(!same_secret(std::string("a\0b", 3), std::string("a\0c", 3)));
}

// ---- the sensors of temperature ----------------------------------------------------------------------------------------------------------

static bool pad(std::uint8_t lo, std::uint8_t hi, double& c, bool stamp = true) {
  std::uint8_t s[9] = {lo, hi, 0x4B, 0x46, 0x7F, stamp ? static_cast<std::uint8_t>(0x0C) : static_cast<std::uint8_t>(0x0C), 0x0C, 0x10, 0};
  s[8] = onewire_crc8(s, 8);
  return ds18b20_celsius(s, c);
}

static void the_ds18b20_at_its_limits() {
  double c = 1234.0;
  CHECK(pad(0xD0, 0x07, c) && c == 125.0);              // the top of the range
  CHECK(pad(0x90, 0xFC, c) && c == -55.0);              // the bottom
  CHECK(pad(0x91, 0x01, c) && c == 25.0625);
  CHECK(pad(0xA2, 0x00, c) && c == 10.125);
  CHECK(pad(0x08, 0x00, c) && c == 0.5);
  CHECK(pad(0xF8, 0xFF, c) && c == -0.5);
  CHECK(pad(0xFF, 0xFF, c) && c == -0.0625);
  CHECK(pad(0x00, 0x00, c) && c == 0.0);
  c = 1234.0;
  CHECK(!pad(0xD1, 0x07, c) && c == 1234.0);            // 125.0625 is past the range, and the answer is left alone
  CHECK(!pad(0x8F, 0xFC, c) && c == 1234.0);            // -55.0625
  CHECK(!pad(0x00, 0x80, c) && c == 1234.0);            // -2048
  // 85 degrees with the bytes of the power-on state is the answer of a sensor that did not convert; 85 with other bytes is a reading
  std::uint8_t boot[9] = {0x50, 0x05, 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10, 0};
  boot[8] = onewire_crc8(boot, 8);
  CHECK(!ds18b20_celsius(boot, c));
  CHECK(pad(0x50, 0x05, c) && c == 85.0);
  // one bit wrong anywhere in the nine bytes is a bad reading
  std::uint8_t good[9] = {0x91, 0x01, 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10, 0};
  good[8] = onewire_crc8(good, 8);
  CHECK(ds18b20_celsius(good, c) && c == 25.0625);
  for (int byte = 0; byte < 9; ++byte)
    for (int bit = 0; bit < 8; ++bit) {
      std::uint8_t copy[9];
      for (int i = 0; i < 9; ++i) copy[i] = good[i];
      copy[byte] = static_cast<std::uint8_t>(copy[byte] ^ (1 << bit));
      double x = 0.0;
      CHECK(!ds18b20_celsius(copy, x));
    }
}

static void the_crc_of_the_one_wire_bus_and_the_smbus_packet_check() {
  const std::uint8_t rom[8] = {0x02, 0x1C, 0xB8, 0x01, 0x00, 0x00, 0x00, 0xA2};   // the example of the manufacturer's note
  CHECK(onewire_crc8(rom, 7) == 0xA2 && onewire_crc8(rom, 8) == 0);
  CHECK(onewire_crc8(rom, 0) == 0);
  const std::uint8_t text[9] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  CHECK(smbus_pec(text, 9) == 0xF4 && smbus_pec(text, 0) == 0);
  const std::uint8_t one[1] = {0x01};
  CHECK(smbus_pec(one, 1) == 0x07);
}

static void the_thermistor_follows_its_curve_and_refuses_what_cannot_be_one() {
  double c = 0.0;
  // 10k at 25 degrees, beta 3950: half the supply is exactly 25; half the resistance is about 41.45
  CHECK(ntc_celsius(1.65, 3.3, 10000.0, 10000.0, 3950.0, c) && std::fabs(c - 25.0) < 1e-9);
  CHECK(ntc_celsius(3.3 / 3.0, 3.3, 10000.0, 10000.0, 3950.0, c) && std::fabs(c - 41.45) < 0.1);
  // the hotter it is, the lower its resistance and the lower the voltage across it; the whole range is in order
  double last = 1000.0;
  for (double v = 0.2; v < 3.2; v += 0.2) {
    CHECK(ntc_celsius(v, 3.3, 10000.0, 10000.0, 3950.0, c) || v < 0.3 || v > 3.1);
    if (ntc_celsius(v, 3.3, 10000.0, 10000.0, 3950.0, c)) { CHECK(c < last); last = c; }
  }
  CHECK(!ntc_celsius(0.001, 3.3, 10000.0, 10000.0, 3950.0, c));        // a short circuit
  CHECK(!ntc_celsius(3.2999, 3.3, 10000.0, 10000.0, 3950.0, c));       // an open wire
  CHECK(!ntc_celsius(1.0, 0.0, 10000.0, 10000.0, 3950.0, c) && !ntc_celsius(-1.0, 3.3, 10000.0, 10000.0, 3950.0, c));
  CHECK(!ntc_celsius(1.0, 3.3, 0.0, 10000.0, 3950.0, c) && !ntc_celsius(1.0, 3.3, 10000.0, 0.0, 3950.0, c) && !ntc_celsius(1.0, 3.3, 10000.0, 10000.0, 0.0, c));
  CHECK(!ntc_celsius(1.0, 3.3, -10000.0, 10000.0, 3950.0, c) && !ntc_celsius(1.0, 3.3, 10000.0, 10000.0, -3950.0, c));
}

static void the_infrared_thermometer_has_a_range_and_an_error_flag() {
  double c = 0.0;
  CHECK(mlx90614_celsius(14657, c) && std::fabs(c - (14657 * 0.02 - 273.15)) < 1e-9);
  CHECK(mlx90614_celsius(32657, c) && c < 380.0);
  CHECK(!mlx90614_celsius(32658, c));
  CHECK(mlx90614_celsius(10158, c) && c > -70.0);
  CHECK(!mlx90614_celsius(10157, c));
  CHECK(!mlx90614_celsius(0xFFFF, c) && !mlx90614_celsius(0x8000, c) && !mlx90614_celsius(0x8001, c));
}

// ---- the ring of pulses -----------------------------------------------------------------------------------------------------------------

static void the_ring_keeps_the_last_pulses_and_a_reader_that_fell_behind_skips_to_the_oldest() {
  PulseRing<8> ring;
  for (std::uint64_t t = 0; t < 12; ++t) ring.push(1000 + t);
  CHECK(ring.head() == 12);
  for (std::uint32_t i = 4; i < 12; ++i) CHECK(ring.at(i) == 1000 + i);          // the last eight are there
  CHECK(PulseRing<8>::resume_from(0, 12) == 4);                                   // more than eight behind: the oldest that is left
  CHECK(PulseRing<8>::resume_from(4, 12) == 4 && PulseRing<8>::resume_from(5, 12) == 5 && PulseRing<8>::resume_from(12, 12) == 12);
  CHECK(PulseRing<8>::resume_from(3, 12) == 4);                                   // nine behind
  PulseRing<8> wrap(0xFFFFFFFDu);
  for (std::uint64_t t = 0; t < 6; ++t) wrap.push(t);
  CHECK(wrap.head() == 3);                                                        // the counter went round
  CHECK(PulseRing<8>::resume_from(0xFFFFFFFDu, wrap.head()) == 0xFFFFFFFDu);      // six behind across the wrap: nothing is lost
  CHECK(PulseRing<8>::resume_from(0xFFFFFFF0u, wrap.head()) == wrap.head() - 8u);
  CHECK(wrap.at(0xFFFFFFFDu) == 0 && wrap.at(2) == 5);
  CHECK(PulseRing<2>::kSize == 2 && PulseRing<1024>::kSize == 1024);
}

// ---- a JPEG that a camera does not make --------------------------------------------------------------------------------------------------

static void put16(std::vector<std::uint8_t>& v, int x) { v.push_back(static_cast<std::uint8_t>(x >> 8)); v.push_back(static_cast<std::uint8_t>(x)); }

static std::vector<std::uint8_t> jpeg(int w, int h, std::uint8_t sampling, std::size_t data_bytes, std::uint8_t sof = 0xC0, bool with_chroma = true, bool restart = false, std::uint8_t precision = 0) {
  std::vector<std::uint8_t> v = {0xFF, 0xD8};
  for (int id = 0; id < (with_chroma ? 2 : 1); ++id) {
    v.push_back(0xFF); v.push_back(0xDB); put16(v, 67);
    v.push_back(static_cast<std::uint8_t>((precision << 4) | id));
    for (int i = 0; i < 64; ++i) v.push_back(static_cast<std::uint8_t>(id * 100 + i + 1));
  }
  if (restart) { v.push_back(0xFF); v.push_back(0xDD); put16(v, 4); put16(v, 16); }
  v.push_back(0xFF); v.push_back(sof); put16(v, 17); v.push_back(8); put16(v, h); put16(v, w); v.push_back(3);
  const std::uint8_t comps[9] = {1, sampling, 0, 2, 0x11, 1, 3, 0x11, 1};
  v.insert(v.end(), comps, comps + 9);
  v.push_back(0xFF); v.push_back(0xDA); put16(v, 12); v.push_back(3);
  const std::uint8_t scan[9] = {1, 0, 2, 0x11, 3, 0x11, 0, 63, 0};
  v.insert(v.end(), scan, scan + 9);
  for (std::size_t i = 0; i < data_bytes; ++i) v.push_back(static_cast<std::uint8_t>(i % 251 == 0 ? 0x00 : 0x11 + i % 200));
  v.push_back(0xFF); v.push_back(0xD9);
  return v;
}

static void a_jpeg_is_read_for_its_size_tables_and_data() {
  const auto j = jpeg(640, 480, 0x21, 3000);
  const JpegInfo i = parse_jpeg(j.data(), j.size());
  CHECK(i.ok && i.width == 640 && i.height == 480 && i.type == 0 && !i.restart);
  CHECK(i.luma.size() == 64 && i.luma[0] == 1 && i.luma[63] == 64 && i.chroma.size() == 64 && i.chroma[0] == 101);
  CHECK(i.data_end - i.data_start == 3000 && i.data_end == j.size() - 2);
  CHECK(parse_jpeg(jpeg(2040, 2040, 0x22, 10).data(), jpeg(2040, 2040, 0x22, 10).size()).ok);
  CHECK(parse_jpeg(jpeg(2040, 8, 0x22, 10).data(), jpeg(2040, 8, 0x22, 10).size()).type == 1);
}

static void a_jpeg_that_cannot_go_in_the_packets_is_refused_with_its_reason_unspoken() {
  const auto refused = [](const std::vector<std::uint8_t>& v) { const JpegInfo i = parse_jpeg(v.data(), v.size()); return !i.ok; };
  CHECK(refused(jpeg(2041, 480, 0x21, 100)) && refused(jpeg(640, 2041, 0x21, 100)));      // past what a packet header can say
  CHECK(refused(jpeg(0, 480, 0x21, 100)) && refused(jpeg(640, 0, 0x21, 100)));
  CHECK(refused(jpeg(640, 480, 0x11, 100)));                                              // 4:4:4 is not in the format
  CHECK(refused(jpeg(640, 480, 0x21, 100, 0xC2)));                                        // progressive
  CHECK(refused(jpeg(640, 480, 0x21, 100, 0xC0, false)));                                 // one table only
  CHECK(refused(jpeg(640, 480, 0x21, 100, 0xC0, true, false, 1)));                        // 16-bit tables
  CHECK(refused(jpeg(640, 480, 0x21, 0)));                                                // no data
  const auto restart = jpeg(640, 480, 0x21, 100, 0xC0, true, true);
  const JpegInfo r = parse_jpeg(restart.data(), restart.size());
  CHECK(r.ok && r.restart);
  CHECK(rtp_jpeg_packets(restart.data(), restart.size(), 0, 0, 0).empty());              // it is read, and not sent
  const std::vector<std::uint8_t> not_jpeg = {0x89, 'P', 'N', 'G', 0, 0, 0, 0};
  CHECK(refused(not_jpeg) && refused({}) && refused({0xFF, 0xD8}));
}

static void a_cut_or_padded_jpeg_is_read_up_to_the_last_end_marker() {
  auto j = jpeg(320, 240, 0x22, 500);
  const std::size_t whole = j.size();
  for (std::size_t cut : {std::size_t(1), std::size_t(2), std::size_t(3), std::size_t(30), std::size_t(300)}) {
    const JpegInfo i = parse_jpeg(j.data(), whole - cut);
    CHECK(!i.ok || i.data_end <= whole - cut);                                           // a cut one never reads past what it has
  }
  CHECK(!parse_jpeg(j.data(), whole - 2).ok);                                            // without the end marker
  for (int i = 0; i < 16; ++i) j.push_back(0x00);                                        // zeros after the end, as a camera buffer has them
  const JpegInfo p = parse_jpeg(j.data(), j.size());
  CHECK(p.ok && p.data_end == whole - 2 && p.data_end - p.data_start == 500);
  // a table whose length lies, or a header that claims more than the file has, is not read past the end
  std::vector<std::uint8_t> lie = {0xFF, 0xD8, 0xFF, 0xDB, 0xFF, 0xFF, 0x00};
  CHECK(!parse_jpeg(lie.data(), lie.size()).ok);
  std::vector<std::uint8_t> short_table = {0xFF, 0xD8, 0xFF, 0xDB, 0x00, 0x05, 0x00, 0x01, 0x02};
  CHECK(!parse_jpeg(short_table.data(), short_table.size()).ok);
}

static void the_packets_carry_every_byte_once_in_order_and_say_where_the_picture_ends() {
  const auto j = jpeg(640, 480, 0x21, 5000);
  const JpegInfo info = parse_jpeg(j.data(), j.size());
  for (std::size_t payload_max : {std::size_t(200), std::size_t(576), std::size_t(1400), std::size_t(1500)}) {
    const auto packets = rtp_jpeg_packets(j.data(), j.size(), 65530, 90000, 0xDEADBEEF, payload_max);
    CHECK(!packets.empty());
    std::size_t carried = 0;
    bool in_order = true, one_marker = true, sizes_ok = true, header_ok = true;
    for (std::size_t k = 0; k < packets.size(); ++k) {
      const auto& p = packets[k];
      const bool first = k == 0, last = k + 1 == packets.size();
      const std::size_t head = 12 + 8 + (first ? 4 + 128 : 0);
      if (p.size() > 12 + payload_max || p.size() <= head) sizes_ok = false;
      if (p[0] != 0x80 || (p[1] & 0x7F) != 26 || ((p[1] & 0x80) != 0) != last) one_marker = false;
      const std::uint16_t seq = static_cast<std::uint16_t>((p[2] << 8) | p[3]);
      if (seq != static_cast<std::uint16_t>(65530 + k)) in_order = false;
      const std::uint32_t ts = (static_cast<std::uint32_t>(p[4]) << 24) | (p[5] << 16) | (p[6] << 8) | p[7];
      const std::uint32_t ssrc = (static_cast<std::uint32_t>(p[8]) << 24) | (p[9] << 16) | (p[10] << 8) | p[11];
      if (ts != 90000 || ssrc != 0xDEADBEEFu) header_ok = false;
      const std::size_t offset = (static_cast<std::size_t>(p[13]) << 16) | (p[14] << 8) | p[15];
      if (offset != carried) in_order = false;
      if (p[16] != 0 || p[17] != 255 || p[18] != 80 || p[19] != 60) header_ok = false;
      if (first && (p[20] != 0 || p[21] != 0 || p[22] != 0 || p[23] != 128 || p[24] != info.luma[0] || p[24 + 64] != info.chroma[0])) header_ok = false;
      // the bytes of the picture are those of the file, in order
      const std::size_t at = info.data_start + carried;
      for (std::size_t b = 0; b < p.size() - head; ++b)
        if (p[head + b] != j[at + b]) in_order = false;
      carried += p.size() - head;
    }
    CHECK(carried == info.data_end - info.data_start && in_order && one_marker && sizes_ok && header_ok);
  }
}

static void the_sequence_number_wraps_and_a_picture_that_fits_goes_in_one_packet() {
  const auto big = jpeg(640, 480, 0x22, 4000);
  const auto packets = rtp_jpeg_packets(big.data(), big.size(), 0xFFFF, 1, 2);
  CHECK(packets.size() >= 3 && packets[0][2] == 0xFF && packets[0][3] == 0xFF && packets[1][2] == 0 && packets[1][3] == 0);
  const std::size_t fit = 1400 - (8 + 4 + 128);                                           // what the first packet can carry
  const auto exact = jpeg(64, 64, 0x21, fit);
  CHECK(rtp_jpeg_packets(exact.data(), exact.size(), 0, 0, 0).size() == 1);
  const auto over = jpeg(64, 64, 0x21, fit + 1);
  const auto two = rtp_jpeg_packets(over.data(), over.size(), 0, 0, 0);
  CHECK(two.size() == 2 && two[1].size() == 12 + 8 + 1 && ((two[1][13] << 16) | (two[1][14] << 8) | two[1][15]) == static_cast<int>(fit));
  CHECK((two[0][1] & 0x80) == 0 && (two[1][1] & 0x80) != 0);
  CHECK(rtp_jpeg_packets(big.data(), big.size(), 0, 0, 0, 199).empty() && !rtp_jpeg_packets(big.data(), big.size(), 0, 0, 0, 200).empty());
}

// ---- the rules every node of the family shares ----

static void plain_versions_only() {
  for (const char* good : {"0.0.1", "v1.2.3", "0.7.9.3", "10.20.30.40", "999999.999999.999999.999999", "1.0.0"}) CHECK(semver::parse(good).ok);
  for (const char* bad : {"", "v", "1", "1.2", "1.2.3.4.5", "1..2", ".1.2", "1.2.", "1.2.3.", "1.2.3-rc1", "1.2.3+b5", "a.b.c", "V1.2.3", " 1.2.3", "1.2.3 ", "vv1.2.3", "1234567.1.1", "1.2.3.x"})
    CHECK(!semver::parse(bad).ok);
  const semver::Version v = semver::parse("v3.14.15.92");
  CHECK(v.major == 3 && v.minor == 14 && v.patch == 15 && v.build == 92);
  CHECK(semver::parse("1.2.3").build == 0);
}

static void newer_is_by_number_not_by_text() {
  CHECK(semver::is_newer("0.10.0", "0.9.0") && !semver::is_newer("0.9.0", "0.10.0"));
  CHECK(semver::is_newer("0.8.0.1", "0.8.0") && !semver::is_newer("0.8.0", "0.8.0.1"));
  CHECK(!semver::is_newer("0.8.0", "0.8.0.0") && !semver::is_newer("0.8.0.0", "0.8.0"));   // the same version written two ways
  CHECK(semver::is_newer("1.0.0", "0.99.99.99") && semver::is_newer("0.9.0", "0.8.9.9"));
  CHECK(!semver::is_newer("2.0.0-rc1", "1.0.0") && !semver::is_newer("1.0.0", "garbage") && !semver::is_newer("garbage", "1.0.0"));
  const semver::Version a = semver::parse("1.2.3"), b = semver::parse("1.2.3.1");
  CHECK(!semver::less(a, a) && semver::less(a, b) && !semver::less(b, a));
}

static void node_names() {
  for (const char* good : {"a", "node-1", "a_b", "0abc", "bungard-aux-01"}) CHECK(node_id_is_valid(good));
  CHECK(node_id_is_valid(std::string(64, 'a')) && !node_id_is_valid(std::string(65, 'a')));
  for (const char* bad : {"", "-a", "_a", "A", "Node", "a b", "a.b", "a/b", "a\\b", "\xC3\xB1"}) CHECK(!node_id_is_valid(bad));
  CHECK(!node_id_is_valid(std::string_view("a\0b", 3)));
}

static void known_moments() {
  CHECK(civil::days_from_civil(1970, 1, 1) == 0 && civil::days_from_civil(1969, 12, 31) == -1 && civil::days_from_civil(1970, 3, 1) == 59);
  CHECK(civil::days_from_civil(2000, 2, 29) == 11016);            // 2000 is a leap year
  CHECK(civil::days_from_civil(2100, 3, 1) - civil::days_from_civil(2100, 2, 28) == 1);   // 2100 is not
  CHECK(civil::seconds_as_utc(2038, 1, 19, 3, 14, 7) == 2147483647LL);
  CHECK(civil::seconds_as_utc(1969, 12, 31, 23, 59, 59) == -1);
  CHECK(civil::seconds_as_utc(2024, 2, 29, 12, 0, 0) - civil::seconds_as_utc(2024, 2, 28, 12, 0, 0) == 86400);
}

static void the_offset_of_a_time_zone_from_the_local_and_the_utc_clock() {
  const std::int64_t utc = civil::seconds_as_utc(2024, 6, 1, 10, 0, 0);
  CHECK(civil::utc_offset_minutes(utc, 2024, 6, 1, 12, 0, 0) == 120);
  CHECK(civil::utc_offset_minutes(utc, 2024, 6, 1, 5, 0, 0) == -300);
  CHECK(civil::utc_offset_minutes(utc, 2024, 6, 1, 15, 45, 0) == 345);       // Nepal
  CHECK(civil::utc_offset_minutes(utc, 2024, 6, 1, 10, 0, 0) == 0);
  const std::int64_t late = civil::seconds_as_utc(2024, 12, 31, 23, 30, 0);
  CHECK(civil::utc_offset_minutes(late, 2025, 1, 1, 0, 30, 0) == 60);        // the local day is already the next year
}

static void addresses_are_exactly_four_numbers_without_extra_zeros() {
  std::uint32_t v = 0xDEADBEEF;
  CHECK(net::parse_ipv4("0.0.0.0", v) && v == 0);
  CHECK(net::parse_ipv4("255.255.255.255", v) && v == 0xFFFFFFFFu);
  CHECK(net::parse_ipv4("192.168.1.10", v) && v == 0xC0A8010Au);
  v = 0xDEADBEEF;
  for (const char* bad : {"", "1.2.3", "1.2.3.4.5", "256.1.1.1", "1.2.3.256", "1.2.3.04", "01.2.3.4", "10.0.0.00", "1.2.3.", ".1.2.3", "1..2.3", " 1.2.3.4", "1.2.3.4 ", "1.2.3.4\n", "a.b.c.d", "1.2.3.-4", "1111.2.3.4", "1.2.3.4abc", "1,2,3,4"}) CHECK(!net::parse_ipv4(bad, v));
  CHECK(v == 0xDEADBEEF);                                            // a refusal leaves the answer alone
  for (std::uint32_t x : {0u, 1u, 0x7F000001u, 0xC0A80001u, 0xFFFFFFFFu, 0x0A00FF00u}) { std::uint32_t back = 0; CHECK(net::parse_ipv4(net::ipv4_text(x), back) && back == x); }
  CHECK(net::ipv4_text(0x0A000102u) == "10.0.1.2");
}

static void masks_are_runs_of_ones_with_room_for_two_hosts() {
  for (const char* good : {"255.255.255.0", "255.255.255.252", "255.255.0.0", "255.0.0.0", "128.0.0.0", "255.255.254.0"}) { std::uint32_t m = 0; CHECK(net::parse_ipv4(good, m) && net::valid_netmask(m)); }
  for (const char* bad : {"255.255.255.254", "255.255.255.255", "0.0.0.0", "255.0.255.0", "0.255.255.255", "255.255.255.253", "1.0.0.0"}) { std::uint32_t m = 0; CHECK(net::parse_ipv4(bad, m) && !net::valid_netmask(m)); }
}

static void host_addresses_leave_out_loopback_multicast_and_the_ends_of_a_subnet() {
  CHECK(net::usable_host_address(0x0A000001u) && net::usable_host_address(0xDFFFFFFFu));
  CHECK(!net::usable_host_address(0) && !net::usable_host_address(0x7F000001u) && !net::usable_host_address(0xE0000001u) && !net::usable_host_address(0xFFFFFFFFu));
  const std::uint32_t mask = 0xFFFFFF00u;
  CHECK(net::is_network_or_broadcast(0xC0A80100u, mask) && net::is_network_or_broadcast(0xC0A801FFu, mask) && !net::is_network_or_broadcast(0xC0A80101u, mask));
  CHECK(net::same_subnet(0xC0A80101u, 0xC0A801FEu, mask) && !net::same_subnet(0xC0A80101u, 0xC0A80201u, mask));
}

static void names_and_hosts() {
  CHECK(net::valid_hostname("node-1") && net::valid_hostname(std::string(32, 'a')) && !net::valid_hostname(std::string(33, 'a')));
  for (const char* bad : {"", "-a", "a-", "A", "a_b", "a.b", "a b", "\xC3\xB1"}) CHECK(!net::valid_hostname(bad));
  CHECK(net::valid_host("Time.Example_org") && net::valid_host("10.0.0.1") && net::valid_host(std::string(253, 'a')) && !net::valid_host(std::string(254, 'a')));
  for (const char* bad : {"", ".a", "-a", "a.", "a b", "a/b", "a:b", "a@b"}) CHECK(!net::valid_host(bad));
}

static void text_is_checked_as_utf8_strictly() {
  std::size_t n = 99;
  CHECK(net::valid_utf8("", &n) && n == 0);
  CHECK(net::valid_utf8("abc", &n) && n == 3);
  CHECK(net::valid_utf8("\xC3\xB1", &n) && n == 1);                    // n with a tilde
  CHECK(net::valid_utf8("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E", &n) && n == 3);
  CHECK(net::valid_utf8("\xF0\x9F\x98\x80", &n) && n == 1);
  CHECK(net::valid_utf8("a\xC3\xB1" "b\xE6\x97\xA5", &n) && n == 4);
  CHECK(net::valid_utf8("\xF4\x8F\xBF\xBF") && net::valid_utf8("\xED\x9F\xBF") && net::valid_utf8("\xC2\x80"));   // the last of each range
  for (const char* bad : {"\xC0\x80", "\xC1\xBF", "\xE0\x80\x80", "\xE0\x9F\xBF", "\xF0\x80\x80\x80", "\xED\xA0\x80", "\xED\xBF\xBF", "\xF4\x90\x80\x80", "\xF5\x80\x80\x80", "\xFF", "\x80", "\xBF",
                          "\xE6\x97", "\xC3", "\xF0\x9F\x98", "\xE6\x97" "A", "\xC3" "A"})
    CHECK(!net::valid_utf8(bad));
}

static void wifi_names_and_keys() {
  CHECK(net::valid_ssid("Casa") && net::valid_ssid(std::string(32, 'x')) && net::valid_ssid("Wi-Fi \xC3\xB1") && !net::valid_ssid("") && !net::valid_ssid(std::string(33, 'x')));
  CHECK(!net::valid_ssid("a\tb") && !net::valid_ssid("a\nb") && !net::valid_ssid("a\x7f"));
  CHECK(net::valid_wpa_passphrase("12345678") && net::valid_wpa_passphrase(std::string(63, 'k')) && net::valid_wpa_passphrase("with spaces ok"));
  CHECK(!net::valid_wpa_passphrase("1234567") && !net::valid_wpa_passphrase(std::string(64, 'k')) && !net::valid_wpa_passphrase("tab\there!!") && !net::valid_wpa_passphrase("ca\xC3\xB1" "on1234"));
}


int main() {
  md5_known_answers_of_the_standard();
  md5_across_the_edges_of_a_block();
  sha1_known_answers_of_the_standard();
  base64_as_the_standard_writes_it();
  secrets_are_compared_whole();
  the_ds18b20_at_its_limits();
  the_crc_of_the_one_wire_bus_and_the_smbus_packet_check();
  the_thermistor_follows_its_curve_and_refuses_what_cannot_be_one();
  the_infrared_thermometer_has_a_range_and_an_error_flag();
  the_ring_keeps_the_last_pulses_and_a_reader_that_fell_behind_skips_to_the_oldest();
  a_jpeg_is_read_for_its_size_tables_and_data();
  a_jpeg_that_cannot_go_in_the_packets_is_refused_with_its_reason_unspoken();
  a_cut_or_padded_jpeg_is_read_up_to_the_last_end_marker();
  the_packets_carry_every_byte_once_in_order_and_say_where_the_picture_ends();
  the_sequence_number_wraps_and_a_picture_that_fits_goes_in_one_packet();
  plain_versions_only();
  newer_is_by_number_not_by_text();
  node_names();
  known_moments();
  the_offset_of_a_time_zone_from_the_local_and_the_utc_clock();
  addresses_are_exactly_four_numbers_without_extra_zeros();
  masks_are_runs_of_ones_with_room_for_two_hosts();
  host_addresses_leave_out_loopback_multicast_and_the_ends_of_a_subnet();
  names_and_hosts();
  text_is_checked_as_utf8_strictly();
  wifi_names_and_keys();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
