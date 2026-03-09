#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <vitality/vitality.hpp>

#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <optional>
#include <variant>
#include <vector>

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace {

std::uint8_t u8(vita::byte value) {
    return std::to_integer<std::uint8_t>(value);
}

void expect_parse_error(const std::function<void()>& fn, vita::ParseErrorCode expected) {
    try {
        fn();
        FAIL("expected vita::ParseError");
    } catch (const vita::ParseError& ex) {
        CHECK(ex.code() == expected);
    }
}

} // namespace

TEST_CASE("signal packet round trip preserves header metadata and zero-copy payload view") {
    std::vector<vita::byte> payload = {
        vita::byte{0x10}, vita::byte{0x11}, vita::byte{0x12}, vita::byte{0x13},
        vita::byte{0x20}, vita::byte{0x21}, vita::byte{0x22}, vita::byte{0x23},
    };

    vita::timestamp ts;
    ts.set_integer_type(vita::IntegerTimestampType::UTC);
    ts.set_fractional_type(vita::FractionalTimestampType::Picoseconds);
    ts.set_integer_seconds(0x11223344u);
    ts.set_fractional(0x0102030405060708ULL);

    vita::packet::signal packet;
    packet.set_stream_id(0x01020304u);
    packet.set_class_id(vita::class_id{0xAAu, 0x00BCDEu, 0x1357u, 0x2468u});
    packet.set_timestamp(ts);
    packet.set_payload_view(vita::bytes_view{payload.data(), payload.size()});
    packet.set_trailer(vita::trailer{0xA1B2C3D4u});
    packet.set_spectrum_mode(true);
    packet.header().set_sequence(9u);

    const auto bytes = packet.to_bytes();
    REQUIRE(bytes.size() == packet.serialized_size_bytes());
    CHECK(u8(bytes[4]) == 0x01u);
    CHECK(u8(bytes[5]) == 0x02u);
    CHECK(u8(bytes[6]) == 0x03u);
    CHECK(u8(bytes[7]) == 0x04u);
    CHECK(u8(bytes[16]) == 0x11u);
    CHECK(u8(bytes[17]) == 0x22u);
    CHECK(u8(bytes[18]) == 0x33u);
    CHECK(u8(bytes[19]) == 0x44u);

    const auto parsed = vita::view::signal::parse(vita::as_bytes_view(bytes));
    CHECK(parsed.header().packet_type() == vita::PacketType::SignalData);
    CHECK(parsed.header().has_stream_id());
    CHECK(parsed.header().class_id_included());
    CHECK(parsed.header().trailer_included());
    CHECK(parsed.header().spectrum_mode());
    CHECK(parsed.header().sequence() == 9u);
    REQUIRE(parsed.stream_id().has_value());
    CHECK(parsed.stream_id().value() == 0x01020304u);
    REQUIRE(parsed.class_id().has_value());
    CHECK(parsed.class_id()->reserved() == 0xAAu);
    CHECK(parsed.class_id()->oui() == 0x00BCDEu);
    CHECK(parsed.class_id()->information_class_code() == 0x1357u);
    CHECK(parsed.class_id()->packet_class_code() == 0x2468u);
    CHECK(parsed.timestamp().integer_type() == vita::IntegerTimestampType::UTC);
    CHECK(parsed.timestamp().fractional_type() == vita::FractionalTimestampType::Picoseconds);
    CHECK(parsed.timestamp().integer_seconds() == 0x11223344u);
    CHECK(parsed.timestamp().fractional() == 0x0102030405060708ULL);
    REQUIRE(parsed.trailer().has_value());
    CHECK(parsed.trailer()->raw() == 0xA1B2C3D4u);
    CHECK(parsed.payload().size() == payload.size());
    CHECK(parsed.payload().data() == bytes.data() + 28);
}

TEST_CASE("context packet round trip preserves supported CIF0 subset") {
    vita::timestamp ts;
    ts.set_integer_type(vita::IntegerTimestampType::GPS);
    ts.set_fractional_type(vita::FractionalTimestampType::SampleCount);
    ts.set_integer_seconds(55u);
    ts.set_fractional(1234u);

    vita::state_event_indicators sei;
    sei.set_reference_lock_enabled(true);
    sei.set_reference_lock(true);
    sei.set_over_range_enabled(true);
    sei.set_over_range(true);
    sei.set_user_bits(0x5Au);

    vita::signal::format fmt;
    fmt.set_packing_method(vita::PackingMethod::ProcessingEfficient);
    fmt.set_real_complex_type(vita::RealComplexType::ComplexPolar);
    fmt.set_data_item_format(vita::DataItemFormat::SignedFixedPoint);
    fmt.set_sample_component_repeat(true);
    fmt.set_event_tag_size(3u);
    fmt.set_channel_tag_size(7u);
    fmt.set_data_item_fraction_size(4u);
    fmt.set_item_packing_field_size(16u);
    fmt.set_data_item_size(12u);
    fmt.set_repeat_count(2u);
    fmt.set_vector_size(64u);

    vita::packet::context packet;
    packet.set_stream_id(0x10203040u);
    packet.set_class_id(vita::class_id{0x00u, 0x00ABCDu, 0x0011u, 0x0022u});
    packet.set_timestamp(ts);
    packet.set_change_indicator(true);
    packet.set_reference_point_id(0x11223344u);
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
    packet.set_device_identifier(0x55667788u);
    packet.set_state_event_indicators(sei);
    packet.set_signal_data_format(fmt);

    const auto bytes = packet.to_bytes();
    const auto parsed = vita::view::context::parse(vita::as_bytes_view(bytes));

    REQUIRE(parsed.stream_id().has_value());
    CHECK(parsed.stream_id().value() == 0x10203040u);
    CHECK(parsed.change_indicator());
    CHECK(parsed.reference_point_id() == 0x11223344u);
    CHECK(parsed.bandwidth_hz() == doctest::Approx(20.5e6));
    CHECK(parsed.if_reference_frequency_hz() == doctest::Approx(-2.5e6));
    CHECK(parsed.rf_reference_frequency_hz() == doctest::Approx(915.25e6));
    CHECK(parsed.rf_reference_frequency_offset_hz() == doctest::Approx(-1250.5));
    CHECK(parsed.if_band_offset_hz() == doctest::Approx(250.75));
    CHECK(parsed.reference_level_dbm() == doctest::Approx(-12.5));
    CHECK(parsed.gain_stage1_db() == doctest::Approx(3.5));
    CHECK(parsed.gain_stage2_db() == doctest::Approx(-1.0));
    CHECK(parsed.over_range_count() == 7u);
    CHECK(parsed.sample_rate_sps() == doctest::Approx(30.72e6));
    CHECK(parsed.timestamp_adjustment_femtoseconds() == -123456789);
    CHECK(parsed.timestamp_calibration_time_seconds() == 99u);
    CHECK(parsed.temperature_celsius() == doctest::Approx(42.25));
    CHECK(parsed.device_identifier() == 0x55667788u);

    const auto parsed_fmt = parsed.signal_data_format();
    CHECK(parsed_fmt.packing_method() == vita::PackingMethod::ProcessingEfficient);
    CHECK(parsed_fmt.real_complex_type() == vita::RealComplexType::ComplexPolar);
    CHECK(parsed_fmt.data_item_format() == vita::DataItemFormat::SignedFixedPoint);
    CHECK(parsed_fmt.sample_component_repeat());
    CHECK(parsed_fmt.event_tag_size() == 3u);
    CHECK(parsed_fmt.channel_tag_size() == 7u);
    CHECK(parsed_fmt.data_item_fraction_size() == 4u);
    CHECK(parsed_fmt.item_packing_field_size() == 16u);
    CHECK(parsed_fmt.data_item_size() == 12u);
    CHECK(parsed_fmt.repeat_count() == 2u);
    CHECK(parsed_fmt.vector_size() == 64u);
}

TEST_CASE("packet parse dispatch returns the correct variant") {
    vita::packet::context ctx;
    ctx.set_stream_id(0x12345678u);
    const auto ctx_bytes = ctx.to_bytes();
    const auto ctx_packet = vita::packet::parse(vita::as_bytes_view(ctx_bytes));
    CHECK(std::holds_alternative<vita::view::context>(ctx_packet));

    std::vector<vita::byte> payload = {vita::byte{0}, vita::byte{1}, vita::byte{2}, vita::byte{3}};
    vita::packet::signal sig;
    sig.set_stream_id(0x01020304u);
    sig.set_payload_view(vita::bytes_view{payload.data(), payload.size()});
    const auto sig_bytes = sig.to_bytes();
    const auto sig_packet = vita::packet::parse(vita::as_bytes_view(sig_bytes));
    CHECK(std::holds_alternative<vita::view::signal>(sig_packet));
}

TEST_CASE("byteswap helpers cover common wire types") {
    CHECK(vita::byteswap16(std::uint16_t{0x1234u}) == 0x3412u);
    CHECK(vita::byteswap32(std::uint32_t{0x11223344u}) == 0x44332211u);
    CHECK(vita::byteswap64(std::uint64_t{0x0102030405060708ULL}) == 0x0807060504030201ULL);

    const float original = 1.0f;
    const auto swapped = vita::byteswap(original);
    CHECK(vita::byteswap(swapped) == doctest::Approx(original));

    std::vector<std::complex<float>> samples = {{1.0f, -2.0f}, {3.5f, 4.25f}};
    vita::byteswap_inplace(std::span<std::complex<float>>(samples));
    vita::byteswap_inplace(std::span<std::complex<float>>(samples));
    CHECK(samples[0].real() == doctest::Approx(1.0f));
    CHECK(samples[0].imag() == doctest::Approx(-2.0f));
    CHECK(samples[1].real() == doctest::Approx(3.5f));
    CHECK(samples[1].imag() == doctest::Approx(4.25f));
}


TEST_CASE("signal data format setters reject out-of-range values") {
    vita::signal::format fmt;
    CHECK_THROWS_AS(fmt.set_event_tag_size(8u), std::invalid_argument);
    CHECK_THROWS_AS(fmt.set_channel_tag_size(16u), std::invalid_argument);
    CHECK_THROWS_AS(fmt.set_data_item_fraction_size(16u), std::invalid_argument);
    CHECK_THROWS_AS(fmt.set_item_packing_field_size(65u), std::invalid_argument);
    CHECK_THROWS_AS(fmt.set_data_item_size(65u), std::invalid_argument);
}

TEST_CASE("signal packet rejects non-word-aligned payloads") {
    std::vector<vita::byte> payload = {vita::byte{1}, vita::byte{2}, vita::byte{3}};
    vita::packet::signal packet;
    packet.set_stream_id(1u);
    packet.set_payload_view(vita::bytes_view{payload.data(), payload.size()});
    CHECK_THROWS_AS((void)packet.to_bytes(), std::invalid_argument);
}

TEST_CASE("context packet requires a stream id") {
    vita::packet::context packet;
    CHECK_THROWS_AS((void)packet.to_bytes(), std::invalid_argument);
}

TEST_CASE("parse rejects invalid packet-size fields") {
    std::vector<vita::byte> payload = {vita::byte{0}, vita::byte{1}, vita::byte{2}, vita::byte{3}};
    vita::packet::signal packet;
    packet.set_stream_id(0xAABBCCDDu);
    packet.set_payload_view(vita::bytes_view{payload.data(), payload.size()});
    auto bytes = packet.to_bytes();
    bytes[2] = vita::byte{0x00};
    bytes[3] = vita::byte{0x01};

    expect_parse_error([&] { (void)vita::view::signal::parse(vita::as_bytes_view(bytes)); },
                       vita::ParseErrorCode::InvalidPacketSize);
}

TEST_CASE("parse rejects unsupported context indicators") {
    vita::packet::context packet;
    packet.set_stream_id(0x01020304u);
    auto bytes = packet.to_bytes();

    // CIF0 starts after the 4-byte header and 4-byte stream ID for this packet.
    bytes[8] = vita::byte{0x00};
    bytes[9] = vita::byte{0x00};
    bytes[10] = vita::byte{0x40};
    bytes[11] = vita::byte{0x00};

    expect_parse_error([&] { (void)vita::view::context::parse(vita::as_bytes_view(bytes)); },
                       vita::ParseErrorCode::UnsupportedContextIndicators);
}

TEST_CASE("context accessors throw for fields that are not present") {
    vita::packet::context packet;
    packet.set_stream_id(0x01020304u);
    packet.set_bandwidth_hz(1.0e6);
    const auto bytes = packet.to_bytes();
    const auto parsed = vita::view::context::parse(vita::as_bytes_view(bytes));

    expect_parse_error([&] { (void)parsed.reference_point_id(); },
                       vita::ParseErrorCode::MissingRequiredField);
}

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
TEST_CASE("localhost UDP integration sends signal and context packets and parses them back") {
    const int rx_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    const int tx_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    REQUIRE(rx_fd >= 0);
    REQUIRE(tx_fd >= 0);

    timeval timeout{};
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    REQUIRE(::setsockopt(rx_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0);

    sockaddr_in rx_addr{};
    rx_addr.sin_family = AF_INET;
    rx_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    rx_addr.sin_port = htons(0);
    REQUIRE(::bind(rx_fd, reinterpret_cast<const sockaddr*>(&rx_addr), sizeof(rx_addr)) == 0);

    socklen_t rx_addr_len = sizeof(rx_addr);
    REQUIRE(::getsockname(rx_fd, reinterpret_cast<sockaddr*>(&rx_addr), &rx_addr_len) == 0);

    vita::packet::context ctx;
    ctx.set_stream_id(0x12345678u);
    ctx.set_change_indicator(true);
    ctx.set_bandwidth_hz(5.0e6);
    ctx.set_rf_reference_frequency_hz(915.0e6);
    ctx.set_sample_rate_sps(7.68e6);
    const auto ctx_bytes = ctx.to_bytes();

    std::vector<std::complex<float>> tx_samples = {
        {1.0f, -1.0f},
        {2.5f, 0.25f},
        {-3.0f, 4.0f},
        {0.0f, 0.5f},
    };
    vita::packet::signal sig;
    sig.set_stream_id(0x12345678u);
    sig.set_payload_view(vita::as_bytes_view(tx_samples));
    const auto sig_bytes = sig.to_bytes();

    REQUIRE(::sendto(tx_fd, ctx_bytes.data(), ctx_bytes.size(), 0,
                     reinterpret_cast<const sockaddr*>(&rx_addr), sizeof(rx_addr)) == static_cast<ssize_t>(ctx_bytes.size()));
    REQUIRE(::sendto(tx_fd, sig_bytes.data(), sig_bytes.size(), 0,
                     reinterpret_cast<const sockaddr*>(&rx_addr), sizeof(rx_addr)) == static_cast<ssize_t>(sig_bytes.size()));

    std::array<vita::byte, 2048> buffer{};
    bool saw_context = false;
    bool saw_signal = false;

    for (int i = 0; i < 2; ++i) {
        const auto received = ::recvfrom(rx_fd, buffer.data(), buffer.size(), 0, nullptr, nullptr);
        REQUIRE(received > 0);
        const auto parsed = vita::packet::parse(vita::bytes_view{buffer.data(), static_cast<std::size_t>(received)});

        if (std::holds_alternative<vita::view::context>(parsed)) {
            const auto& view = std::get<vita::view::context>(parsed);
            CHECK(view.stream_id().value() == 0x12345678u);
            CHECK(view.bandwidth_hz() == doctest::Approx(5.0e6));
            CHECK(view.rf_reference_frequency_hz() == doctest::Approx(915.0e6));
            CHECK(view.sample_rate_sps() == doctest::Approx(7.68e6));
            saw_context = true;
        } else {
            const auto& view = std::get<vita::view::signal>(parsed);
            CHECK(view.stream_id().value() == 0x12345678u);

            std::vector<std::complex<float>> rx_samples(view.payload().size() / sizeof(std::complex<float>));
            std::memcpy(rx_samples.data(), view.payload().data(), view.payload().size());
            REQUIRE(rx_samples.size() == tx_samples.size());
            for (std::size_t k = 0; k < tx_samples.size(); ++k) {
                CHECK(rx_samples[k].real() == doctest::Approx(tx_samples[k].real()));
                CHECK(rx_samples[k].imag() == doctest::Approx(tx_samples[k].imag()));
            }
            saw_signal = true;
        }
    }

    CHECK(saw_context);
    CHECK(saw_signal);

    ::close(tx_fd);
    ::close(rx_fd);
}
#endif
