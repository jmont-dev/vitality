#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <variant>
#include <vector>

#include "vitality/vitality.hpp"

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace {

using vita::byte;
using vita::bytes_view;

std::uint8_t u8(byte b) {
    return std::to_integer<std::uint8_t>(b);
}

void expect_parse_error(const std::function<void()>& fn, vita::parse_error_code code) {
    try {
        fn();
        FAIL("expected vita::parse_error");
    } catch (const vita::parse_error& ex) {
        CHECK(static_cast<int>(ex.code()) == static_cast<int>(code));
    }
}

void expect_invalid_argument(const std::function<void()>& fn) {
    CHECK_THROWS_AS(fn(), std::invalid_argument);
}

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
int make_udp_socket() {
    const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        throw std::runtime_error("socket failed");
    }
    return fd;
}

sockaddr_in bind_loopback_receiver(int fd) {
    timeval timeout{};
    timeout.tv_sec = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
        throw std::runtime_error("setsockopt failed");
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(0);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (::bind(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
        throw std::runtime_error("bind failed");
    }

    socklen_t size = sizeof(address);
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&address), &size) != 0) {
        throw std::runtime_error("getsockname failed");
    }

    return address;
}

void send_datagram(int fd, const sockaddr_in& destination, bytes_view bytes) {
    const auto sent = ::sendto(fd,
                               reinterpret_cast<const void*>(bytes.data()),
                               bytes.size(),
                               0,
                               reinterpret_cast<const sockaddr*>(&destination),
                               sizeof(destination));
    REQUIRE(sent >= 0);
    REQUIRE(static_cast<std::size_t>(sent) == bytes.size());
}

std::vector<byte> receive_datagram(int fd) {
    std::array<byte, 65536> buffer{};
    sockaddr_in source{};
    socklen_t source_size = sizeof(source);

    const auto received = ::recvfrom(fd,
                                     reinterpret_cast<void*>(buffer.data()),
                                     buffer.size(),
                                     0,
                                     reinterpret_cast<sockaddr*>(&source),
                                     &source_size);
    REQUIRE(received >= 0);
    return std::vector<byte>(buffer.begin(), buffer.begin() + received);
}
#endif

} // namespace

TEST_CASE("signal packet round trips with zero-copy payload view") {
    std::vector<byte> payload = {
        byte{0x10}, byte{0x11}, byte{0x12}, byte{0x13},
        byte{0x20}, byte{0x21}, byte{0x22}, byte{0x23},
    };

    vita::timestamp ts;
    ts.set_integer_type(vita::integer_timestamp_type::UTC);
    ts.set_fractional_type(vita::fractional_timestamp_type::Picoseconds);
    ts.set_integer_seconds(0x11223344u);
    ts.set_fractional(0x0102030405060708ULL);

    vita::packet::signal packet;
    packet.set_stream_id(0x01020304u);
    packet.set_class_id(vita::class_id{0xAAu, 0x00BCDEu, 0x1357u, 0x2468u});
    packet.set_timestamp(ts);
    packet.set_payload_view(bytes_view{payload.data(), payload.size()});
    packet.set_trailer(vita::trailer{0xA1B2C3D4u});
    packet.set_spectrum_mode(true);
    packet.header().set_sequence(9u);

    const auto bytes = packet.to_bytes();
    REQUIRE(bytes.size() == packet.serialized_size_bytes());
    CHECK(u8(bytes[4]) == 0x01u);
    CHECK(u8(bytes[5]) == 0x02u);
    CHECK(u8(bytes[6]) == 0x03u);
    CHECK(u8(bytes[7]) == 0x04u);

    const auto parsed = vita::view::signal::parse(vita::as_bytes_view(bytes));

    CHECK(static_cast<int>(parsed.header().packet_type()) == static_cast<int>(vita::packet_type::SignalData));
    CHECK(parsed.header().has_stream_id());
    CHECK(parsed.header().class_id_included());
    CHECK(parsed.header().trailer_included());
    CHECK(parsed.header().spectrum_mode());
    CHECK(parsed.header().sequence() == 9u);
    REQUIRE(parsed.stream_id().has_value());
    CHECK(parsed.stream_id().value() == 0x01020304u);
    REQUIRE(parsed.class_id().has_value());
    CHECK(parsed.class_id()->oui() == 0x00BCDEu);
    CHECK(parsed.timestamp().integer_seconds() == 0x11223344u);
    CHECK(parsed.timestamp().fractional() == 0x0102030405060708ULL);
    REQUIRE(parsed.trailer().has_value());
    CHECK(parsed.trailer()->raw() == 0xA1B2C3D4u);
    CHECK(parsed.payload().data() == bytes.data() + 28);

    for (std::size_t i = 0; i < payload.size(); ++i) {
        CHECK(u8(parsed.payload()[i]) == u8(payload[i]));
    }
}

TEST_CASE("signal packet without stream id round trips") {
    std::vector<byte> payload = {byte{0xDE}, byte{0xAD}, byte{0xBE}, byte{0xEF}};

    vita::packet::signal packet;
    packet.set_include_stream_id(false);
    packet.set_payload_view(bytes_view{payload.data(), payload.size()});

    const auto bytes = packet.to_bytes();
    const auto parsed = vita::view::signal::parse(vita::as_bytes_view(bytes));

    CHECK(static_cast<int>(parsed.header().packet_type()) == static_cast<int>(vita::packet_type::SignalDataNoStreamId));
    CHECK(!parsed.header().has_stream_id());
    CHECK(!parsed.stream_id().has_value());
    CHECK(parsed.payload().size() == 4u);
}

TEST_CASE("context packet round trips supported common fields") {
    vita::timestamp ts;
    ts.set_integer_type(vita::integer_timestamp_type::GPS);
    ts.set_fractional_type(vita::fractional_timestamp_type::SampleCount);
    ts.set_integer_seconds(0x01020304u);
    ts.set_fractional(0x1112131415161718ULL);

    vita::state_event_indicators sei;
    sei.set_calibrated_time_enabled(true);
    sei.set_valid_data_enabled(true);
    sei.set_reference_lock(true);
    sei.set_over_range(true);
    sei.set_user_bits(0x5Au);

    vita::signal::format fmt;
    fmt.set_packing_method(vita::packing_method::ProcessingEfficient);
    fmt.set_real_complex_type(vita::real_complex_type::ComplexPolar);
    fmt.set_data_item_format(vita::data_item_format::SignedFixedPoint);
    fmt.set_sample_component_repeat(true);
    fmt.set_event_tag_size(3);
    fmt.set_channel_tag_size(7);
    fmt.set_data_item_fraction_size(4);
    fmt.set_item_packing_field_size(16);
    fmt.set_data_item_size(12);
    fmt.set_repeat_count(2);
    fmt.set_vector_size(64);

    vita::packet::context packet;
    packet.set_stream_id(0xCAFEBABEu);
    packet.set_class_id(vita::class_id{0x00u, 0x00ABCDu, 0x1234u, 0x5678u});
    packet.set_timestamp(ts);
    packet.set_timestamp_mode_general(true);
    packet.set_change_indicator(true);
    packet.set_reference_point_id(0x10203040u);
    packet.set_bandwidth_hz(20.5e6);
    packet.set_if_reference_frequency_hz(-2.5e6);
    packet.set_rf_reference_frequency_hz(915.25e6);
    packet.set_rf_reference_frequency_offset_hz(-1250.5);
    packet.set_if_band_offset_hz(250.75);
    packet.set_reference_level_dbm(-12.5);
    packet.set_gain_db(std::pair<double, double>{3.5, -1.0});
    packet.set_over_range_count(7u);
    packet.set_sample_rate_sps(30.72e6);
    packet.set_timestamp_adjustment_femtoseconds(-123456789);
    packet.set_timestamp_calibration_time_seconds(99u);
    packet.set_temperature_celsius(42.25);
    packet.set_device_identifier(0x11223344u);
    packet.set_state_event_indicators(sei);
    packet.set_signal_data_format(fmt);

    const auto bytes = packet.to_bytes();
    const auto parsed = vita::view::context::parse(vita::as_bytes_view(bytes));

    CHECK(static_cast<int>(parsed.header().packet_type()) == static_cast<int>(vita::packet_type::Context));
    CHECK(parsed.header().timestamp_mode_general());
    REQUIRE(parsed.stream_id().has_value());
    CHECK(parsed.stream_id().value() == 0xCAFEBABEu);
    REQUIRE(parsed.class_id().has_value());
    CHECK(parsed.class_id()->oui() == 0x00ABCDu);
    CHECK(parsed.change_indicator());
    CHECK(parsed.reference_point_id() == 0x10203040u);
    CHECK(parsed.bandwidth_hz() == doctest::Approx(20.5e6));
    CHECK(parsed.sample_rate_sps() == doctest::Approx(30.72e6));
    REQUIRE(parsed.has_state_event_indicators());
    REQUIRE(parsed.has_signal_data_format());
    CHECK(parsed.state_event_indicators().reference_lock());
    CHECK(parsed.state_event_indicators().over_range());
    CHECK(parsed.state_event_indicators().user_bits() == 0x5Au);
    CHECK(static_cast<int>(parsed.signal_data_format().packing_method()) == static_cast<int>(vita::packing_method::ProcessingEfficient));
    CHECK(parsed.signal_data_format().vector_size() == 64u);
}

TEST_CASE("packet parse dispatches to expected variant") {
    vita::packet::context ctx;
    ctx.set_stream_id(0x12345678u);
    const auto ctx_variant = vita::packet::parse(vita::as_bytes_view(ctx.to_bytes()));
    CHECK(std::holds_alternative<vita::view::context>(ctx_variant));

    std::vector<byte> payload = {byte{0}, byte{1}, byte{2}, byte{3}};
    vita::packet::signal sig;
    sig.set_stream_id(0x01020304u);
    sig.set_payload_view(bytes_view{payload.data(), payload.size()});
    const auto sig_variant = vita::packet::parse(vita::as_bytes_view(sig.to_bytes()));
    CHECK(std::holds_alternative<vita::view::signal>(sig_variant));
}

TEST_CASE("signal packet rejects non word aligned payload on serialize") {
    std::vector<byte> payload = {byte{1}, byte{2}, byte{3}};
    vita::packet::signal packet;
    packet.set_stream_id(1u);
    packet.set_payload_view(bytes_view{payload.data(), payload.size()});

    expect_invalid_argument([&] { (void)packet.to_bytes(); });
}

TEST_CASE("context packet requires stream id on serialize") {
    vita::packet::context packet;
    expect_invalid_argument([&] { (void)packet.to_bytes(); });
}

TEST_CASE("parse rejects invalid packet size field") {
    std::vector<byte> payload = {byte{0}, byte{1}, byte{2}, byte{3}};
    vita::packet::signal packet;
    packet.set_stream_id(0xAABBCCDDu);
    packet.set_payload_view(bytes_view{payload.data(), payload.size()});
    auto bytes = packet.to_bytes();

    bytes[2] = byte{0x00};
    bytes[3] = byte{0x01};

    expect_parse_error([&] { (void)vita::view::signal::parse(vita::as_bytes_view(bytes)); },
                       vita::parse_error_code::InvalidPacketSize);
}

TEST_CASE("parse rejects unsupported context indicators") {
    vita::packet::context packet;
    packet.set_stream_id(0x01020304u);
    auto bytes = packet.to_bytes();

    bytes[8] = byte{0x00};
    bytes[9] = byte{0x00};
    bytes[10] = byte{0x40};
    bytes[11] = byte{0x00};

    expect_parse_error([&] { (void)vita::view::context::parse(vita::as_bytes_view(bytes)); },
                       vita::parse_error_code::UnsupportedContextIndicators);
}

TEST_CASE("context view missing field accessor throws") {
    vita::packet::context packet;
    packet.set_stream_id(0x01020304u);
    packet.set_bandwidth_hz(1.0e6);
    const auto bytes = packet.to_bytes();
    const auto parsed = vita::view::context::parse(vita::as_bytes_view(bytes));

    expect_parse_error([&] { (void)parsed.reference_point_id(); },
                       vita::parse_error_code::MissingRequiredField);
}

TEST_CASE("byteswap helpers support common VITA payload types") {
    CHECK(vita::byteswap16(std::uint16_t{0x1234u}) == std::uint16_t{0x3412u});
    CHECK(vita::byteswap32(std::uint32_t{0x11223344u}) == std::uint32_t{0x44332211u});
    CHECK(vita::byteswap64(std::uint64_t{0x0102030405060708ULL}) == std::uint64_t{0x0807060504030201ULL});

    std::vector<std::complex<float>> samples = {{1.0f, -2.0f}, {3.5f, -4.5f}};
    const auto original = samples;
    vita::byteswap_inplace(std::span<std::complex<float>>(samples));
    vita::byteswap_inplace(std::span<std::complex<float>>(samples));
    CHECK(samples == original);
}

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
TEST_CASE("localhost UDP send receive parses context and signal packets") {
    const int receiver_fd = make_udp_socket();
    const int sender_fd = make_udp_socket();
    const auto receiver = bind_loopback_receiver(receiver_fd);

    std::vector<std::complex<float>> tx_samples = {{1.0f, -1.0f}, {2.5f, 0.25f}};

    vita::packet::context context;
    context.set_stream_id(0x12345678u);
    context.set_sample_rate_sps(7.68e6);
    context.set_rf_reference_frequency_hz(915.0e6);

    vita::packet::signal signal;
    signal.set_stream_id(0x12345678u);
    signal.set_payload_view(vita::as_bytes_view(tx_samples));

    send_datagram(sender_fd, receiver, vita::as_bytes_view(context.to_bytes()));
    send_datagram(sender_fd, receiver, vita::as_bytes_view(signal.to_bytes()));

    const auto received_context = receive_datagram(receiver_fd);
    const auto received_signal = receive_datagram(receiver_fd);

    const auto context_packet = vita::packet::parse(vita::as_bytes_view(received_context));
    const auto signal_packet = vita::packet::parse(vita::as_bytes_view(received_signal));

    REQUIRE(std::holds_alternative<vita::view::context>(context_packet));
    REQUIRE(std::holds_alternative<vita::view::signal>(signal_packet));

    const auto& context_view = std::get<vita::view::context>(context_packet);
    const auto& signal_view = std::get<vita::view::signal>(signal_packet);

    CHECK(context_view.stream_id().value() == 0x12345678u);
    CHECK(context_view.sample_rate_sps() == doctest::Approx(7.68e6));
    CHECK(context_view.rf_reference_frequency_hz() == doctest::Approx(915.0e6));
    CHECK(signal_view.stream_id().value() == 0x12345678u);

    std::vector<std::complex<float>> rx_samples(signal_view.payload().size() / sizeof(std::complex<float>));
    std::memcpy(rx_samples.data(), signal_view.payload().data(), signal_view.payload().size());
    CHECK(rx_samples == tx_samples);

    ::close(sender_fd);
    ::close(receiver_fd);
}
#endif
