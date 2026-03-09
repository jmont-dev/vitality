#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <array>
#include <bit>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include "vitality/vitality.hpp"

namespace {

using vitality::byte;
using vitality::bytes_view;

std::uint8_t u8(byte b) {
    return std::to_integer<std::uint8_t>(b);
}

void expect_parse_error(const std::function<void()>& fn, vitality::ParseErrorCode code) {
    try {
        fn();
        FAIL("expected vitality::ParseError");
    } catch (const vitality::ParseError& ex) {
        CHECK(ex.code() == code);
    }
}

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
class UdpSocket {
public:
    explicit UdpSocket(int fd = -1) noexcept : fd_(fd) {}

    ~UdpSocket() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    UdpSocket(UdpSocket&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
    UdpSocket& operator=(UdpSocket&& other) noexcept {
        if (this != &other) {
            if (fd_ >= 0) {
                ::close(fd_);
            }
            fd_ = std::exchange(other.fd_, -1);
        }
        return *this;
    }

    [[nodiscard]] int get() const noexcept { return fd_; }

private:
    int fd_;
};

struct BoundReceiver {
    UdpSocket socket;
    sockaddr_in address{};
};

[[nodiscard]] UdpSocket make_udp_socket() {
    const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        throw std::system_error(errno, std::generic_category(), "socket");
    }
    return UdpSocket(fd);
}

void set_receive_timeout(int fd, int seconds) {
    timeval timeout{};
    timeout.tv_sec = seconds;
    timeout.tv_usec = 0;
    if (::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
        throw std::system_error(errno, std::generic_category(), "setsockopt(SO_RCVTIMEO)");
    }
}

[[nodiscard]] BoundReceiver bind_loopback_receiver() {
    BoundReceiver receiver{make_udp_socket(), {}};
    set_receive_timeout(receiver.socket.get(), 1);

    receiver.address.sin_family = AF_INET;
    receiver.address.sin_port = htons(0);
    receiver.address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (::bind(receiver.socket.get(), reinterpret_cast<const sockaddr*>(&receiver.address), sizeof(receiver.address)) != 0) {
        throw std::system_error(errno, std::generic_category(), "bind");
    }

    socklen_t address_size = sizeof(receiver.address);
    if (::getsockname(receiver.socket.get(), reinterpret_cast<sockaddr*>(&receiver.address), &address_size) != 0) {
        throw std::system_error(errno, std::generic_category(), "getsockname");
    }

    return receiver;
}

void send_datagram(int fd, const sockaddr_in& destination, bytes_view bytes) {
    const auto sent = ::sendto(fd,
                               bytes.data(),
                               bytes.size(),
                               0,
                               reinterpret_cast<const sockaddr*>(&destination),
                               sizeof(destination));
    if (sent < 0) {
        throw std::system_error(errno, std::generic_category(), "sendto");
    }
    REQUIRE(static_cast<std::size_t>(sent) == bytes.size());
}

[[nodiscard]] std::vector<byte> receive_datagram(int fd) {
    std::array<byte, 65536> buffer{};
    sockaddr_in source{};
    socklen_t source_size = sizeof(source);

    const auto received = ::recvfrom(fd,
                                     buffer.data(),
                                     buffer.size(),
                                     0,
                                     reinterpret_cast<sockaddr*>(&source),
                                     &source_size);
    if (received < 0) {
        throw std::system_error(errno, std::generic_category(), "recvfrom");
    }

    return std::vector<byte>(buffer.begin(), buffer.begin() + received);
}
#endif

} // namespace

TEST_CASE("signal data packets round-trip with zero-copy payload views") {
    using namespace vitality;

    std::vector<byte> payload = {
        byte{0x10}, byte{0x11}, byte{0x12}, byte{0x13},
        byte{0x20}, byte{0x21}, byte{0x22}, byte{0x23},
    };

    Timestamp ts;
    ts.set_integer_type(IntegerTimestampType::UTC);
    ts.set_fractional_type(FractionalTimestampType::Picoseconds);
    ts.set_integer_seconds(0x11223344u);
    ts.set_fractional(0x0102030405060708ULL);

    SignalDataPacket packet;
    packet.set_stream_id(0x01020304u);
    packet.set_class_id(ClassId{0xAAu, 0x00BCDEu, 0x1357u, 0x2468u});
    packet.set_timestamp(ts);
    packet.set_payload_view(as_bytes_view(payload));
    packet.set_trailer(Trailer{0xA1B2C3D4u});
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

    const auto parsed = SignalDataPacketView::parse(as_bytes_view(bytes));

    CHECK(parsed.header().packet_type() == PacketType::SignalData);
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
    CHECK(parsed.timestamp().integer_type() == IntegerTimestampType::UTC);
    CHECK(parsed.timestamp().fractional_type() == FractionalTimestampType::Picoseconds);
    CHECK(parsed.timestamp().integer_seconds() == 0x11223344u);
    CHECK(parsed.timestamp().fractional() == 0x0102030405060708ULL);
    REQUIRE(parsed.trailer().has_value());
    CHECK(parsed.trailer()->raw() == 0xA1B2C3D4u);
    CHECK(parsed.payload().size() == payload.size());
    CHECK(parsed.payload().data() == bytes.data() + 28);

    for (std::size_t i = 0; i < payload.size(); ++i) {
        CHECK(u8(parsed.payload()[i]) == u8(payload[i]));
    }
}

TEST_CASE("context packets round-trip supported CIF0 fields") {
    using namespace vitality;

    Timestamp ts;
    ts.set_integer_type(IntegerTimestampType::GPS);
    ts.set_fractional_type(FractionalTimestampType::SampleCount);
    ts.set_integer_seconds(0x01020304u);
    ts.set_fractional(0x1112131415161718ULL);

    StateEventIndicators sei;
    sei.set_calibrated_time_enabled(true);
    sei.set_valid_data_enabled(true);
    sei.set_reference_lock(true);
    sei.set_over_range(true);
    sei.set_user_bits(0x5Au);

    SignalDataFormat fmt;
    fmt.set_packing_method(PackingMethod::ProcessingEfficient);
    fmt.set_real_complex_type(RealComplexType::ComplexPolar);
    fmt.set_data_item_format(DataItemFormat::SignedFixedPoint);
    fmt.set_sample_component_repeat(true);
    fmt.set_event_tag_size(3);
    fmt.set_channel_tag_size(7);
    fmt.set_data_item_fraction_size(4);
    fmt.set_item_packing_field_size(16);
    fmt.set_data_item_size(12);
    fmt.set_repeat_count(2);
    fmt.set_vector_size(64);

    ContextPacket packet;
    packet.set_stream_id(0xCAFEBABEu);
    packet.set_class_id(ClassId{0x00u, 0x00ABCDu, 0x1234u, 0x5678u});
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
    const auto parsed = ContextPacketView::parse(as_bytes_view(bytes));

    CHECK(parsed.header().packet_type() == PacketType::Context);
    CHECK(parsed.header().timestamp_mode_general());
    REQUIRE(parsed.stream_id().has_value());
    CHECK(parsed.stream_id().value() == 0xCAFEBABEu);
    REQUIRE(parsed.class_id().has_value());
    CHECK(parsed.class_id()->oui() == 0x00ABCDu);
    CHECK(parsed.change_indicator());
    CHECK(parsed.reference_point_id() == 0x10203040u);
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
    CHECK(parsed.device_identifier() == 0x11223344u);
    REQUIRE(parsed.has_state_event_indicators());
    REQUIRE(parsed.has_signal_data_format());
    const auto parsed_sei = parsed.state_event_indicators();
    CHECK(parsed_sei.reference_lock());
    CHECK(parsed_sei.over_range());
    CHECK(parsed_sei.user_bits() == 0x5Au);
    const auto parsed_fmt = parsed.signal_data_format();
    CHECK(parsed_fmt.packing_method() == PackingMethod::ProcessingEfficient);
    CHECK(parsed_fmt.real_complex_type() == RealComplexType::ComplexPolar);
    CHECK(parsed_fmt.data_item_format() == DataItemFormat::SignedFixedPoint);
    CHECK(parsed_fmt.sample_component_repeat());
    CHECK(parsed_fmt.event_tag_size() == 3u);
    CHECK(parsed_fmt.channel_tag_size() == 7u);
    CHECK(parsed_fmt.data_item_fraction_size() == 4u);
    CHECK(parsed_fmt.item_packing_field_size() == 16u);
    CHECK(parsed_fmt.data_item_size() == 12u);
    CHECK(parsed_fmt.repeat_count() == 2u);
    CHECK(parsed_fmt.vector_size() == 64u);
}

TEST_CASE("parse_packet dispatches to expected variant") {
    using namespace vitality;

    ContextPacket ctx;
    ctx.set_stream_id(0x12345678u);
    const auto ctx_bytes = ctx.to_bytes();
    const auto ctx_variant = parse_packet(as_bytes_view(ctx_bytes));
    CHECK(std::holds_alternative<ContextPacketView>(ctx_variant));

    std::vector<byte> payload = {byte{0}, byte{1}, byte{2}, byte{3}};
    SignalDataPacket sig;
    sig.set_stream_id(0x01020304u);
    sig.set_payload_view(as_bytes_view(payload));
    const auto sig_bytes = sig.to_bytes();
    const auto sig_variant = parse_packet(as_bytes_view(sig_bytes));
    CHECK(std::holds_alternative<SignalDataPacketView>(sig_variant));
}

TEST_CASE("byte swap helpers cover common VITA payload types") {
    using namespace vitality;

    CHECK(byteswap16(std::uint16_t{0x1234u}) == 0x3412u);
    CHECK(byteswap16(std::int16_t{0x1234}) == static_cast<std::int16_t>(0x3412));
    CHECK(byteswap32(std::uint32_t{0x01020304u}) == 0x04030201u);
    CHECK(byteswap32(std::int32_t{0x01020304}) == static_cast<std::int32_t>(0x04030201u));
    CHECK(byteswap64(std::uint64_t{0x0102030405060708ULL}) == 0x0807060504030201ULL);
    CHECK(byteswap64(std::int64_t{0x0102030405060708LL}) == static_cast<std::int64_t>(0x0807060504030201ULL));

    const float float_value = 1.0f;
    const auto swapped_float = byteswap(float_value);
    CHECK(std::bit_cast<std::uint32_t>(swapped_float) == 0x0000803Fu);
    CHECK(byteswap(swapped_float) == doctest::Approx(float_value));

    const double double_value = 1.0;
    const auto swapped_double = byteswap(double_value);
    CHECK(std::bit_cast<std::uint64_t>(swapped_double) == 0x000000000000F03FULL);
    CHECK(byteswap(swapped_double) == doctest::Approx(double_value));

    const std::complex<float> complex_value{1.0f, -2.0f};
    const auto swapped_complex = byteswap(complex_value);
    CHECK(std::bit_cast<std::uint32_t>(swapped_complex.real()) == 0x0000803Fu);
    CHECK(std::bit_cast<std::uint32_t>(swapped_complex.imag()) == 0x000000C0u);
    CHECK(byteswap(swapped_complex).real() == doctest::Approx(complex_value.real()));
    CHECK(byteswap(swapped_complex).imag() == doctest::Approx(complex_value.imag()));

    std::vector<std::uint16_t> words = {0x1122u, 0x3344u, 0xA0B0u};
    byteswap_inplace(std::span<std::uint16_t>{words});
    CHECK(words[0] == 0x2211u);
    CHECK(words[1] == 0x4433u);
    CHECK(words[2] == 0xB0A0u);

    std::vector<std::complex<float>> iq = {{1.0f, 2.0f}, {-3.0f, 4.0f}};
    const auto original = iq;
    byteswap_inplace(std::span<std::complex<float>>{iq});
    byteswap_inplace(std::span<std::complex<float>>{iq});
    CHECK(iq == original);
}

TEST_CASE("generic as_bytes_view handles complex sample vectors") {
    using namespace vitality;

    const std::vector<std::complex<float>> iq = {{1.0f, -1.0f}, {0.5f, 0.25f}};
    const auto view = as_bytes_view(iq);
    CHECK(view.size() == iq.size() * sizeof(std::complex<float>));
}

TEST_CASE("malformed packets raise parse errors") {
    using namespace vitality;

    expect_parse_error([] { (void)parse_packet(bytes_view{}); }, ParseErrorCode::BufferTooSmall);

    std::vector<byte> too_small = {byte{0x10}, byte{0x00}, byte{0x00}, byte{0x01}};
    expect_parse_error([&] { (void)SignalDataPacketView::parse(as_bytes_view(too_small)); }, ParseErrorCode::BufferTooSmall);

    ContextPacket ctx;
    ctx.set_stream_id(0x12345678u);
    auto ctx_bytes = ctx.to_bytes();
    ctx_bytes.resize(ctx_bytes.size() - 1);
    expect_parse_error([&] { (void)ContextPacketView::parse(as_bytes_view(ctx_bytes)); }, ParseErrorCode::InvalidPacketSize);

    ContextPacket unsupported_packet;
    unsupported_packet.set_stream_id(0x12345678u);
    auto unsupported_ctx = unsupported_packet.to_bytes();
    unsupported_ctx[11] = byte{0x01};
    expect_parse_error([&] { (void)ContextPacketView::parse(as_bytes_view(unsupported_ctx)); }, ParseErrorCode::UnsupportedContextIndicators);
}

TEST_CASE("serialization rejects missing required fields") {
    using namespace vitality;

    SignalDataPacket packet;
    std::vector<byte> payload = {byte{0x00}, byte{0x01}, byte{0x02}, byte{0x03}};
    packet.set_payload_view(as_bytes_view(payload));
    CHECK_THROWS_AS([&] { (void)packet.to_bytes(); }(), std::invalid_argument);
}

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
TEST_CASE("localhost UDP send receive parses context and signal packets") {
    using namespace vitality;

    auto receiver = bind_loopback_receiver();
    auto sender = make_udp_socket();

    ContextPacket context;
    context.set_stream_id(0x12345678u);
    context.set_change_indicator(true);
    context.set_bandwidth_hz(5.0e6);
    context.set_rf_reference_frequency_hz(915.0e6);
    context.set_sample_rate_sps(7.68e6);

    SignalDataFormat format;
    format.set_packing_method(PackingMethod::ProcessingEfficient);
    format.set_real_complex_type(RealComplexType::ComplexCartesian);
    format.set_data_item_format(DataItemFormat::IEEE754Single);
    format.set_data_item_size(32);
    format.set_item_packing_field_size(32);
    context.set_signal_data_format(format);

    std::vector<std::complex<float>> iq_samples = {
        {1.0f, -1.0f},
        {0.5f, 0.25f},
        {-0.75f, 0.125f},
    };

    SignalDataPacket signal;
    signal.set_stream_id(0x12345678u);
    signal.set_payload_view(as_bytes_view(iq_samples));

    const auto context_bytes = context.to_bytes();
    const auto signal_bytes = signal.to_bytes();

    send_datagram(sender.get(), receiver.address, as_bytes_view(context_bytes));
    send_datagram(sender.get(), receiver.address, as_bytes_view(signal_bytes));

    const auto received_context = receive_datagram(receiver.socket.get());
    const auto parsed_context = ContextPacketView::parse(as_bytes_view(received_context));
    REQUIRE(parsed_context.stream_id().has_value());
    CHECK(parsed_context.stream_id().value() == 0x12345678u);
    CHECK(parsed_context.bandwidth_hz() == doctest::Approx(5.0e6));
    CHECK(parsed_context.rf_reference_frequency_hz() == doctest::Approx(915.0e6));
    CHECK(parsed_context.sample_rate_sps() == doctest::Approx(7.68e6));
    REQUIRE(parsed_context.has_signal_data_format());
    CHECK(parsed_context.signal_data_format().data_item_format() == DataItemFormat::IEEE754Single);

    const auto received_signal = receive_datagram(receiver.socket.get());
    const auto parsed_signal = SignalDataPacketView::parse(as_bytes_view(received_signal));
    REQUIRE(parsed_signal.stream_id().has_value());
    CHECK(parsed_signal.stream_id().value() == 0x12345678u);
    CHECK(parsed_signal.payload().size() == iq_samples.size() * sizeof(std::complex<float>));

    std::vector<std::complex<float>> received_samples(iq_samples.size());
    std::memcpy(received_samples.data(), parsed_signal.payload().data(), parsed_signal.payload().size());
    CHECK(received_samples == iq_samples);
}
#endif
