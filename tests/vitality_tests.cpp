#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
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
        CHECK(static_cast<int>(ex.code()) == static_cast<int>(code));
    }
}

void expect_invalid_argument(const std::function<void()>& fn) {
    CHECK_THROWS_AS(fn(), std::invalid_argument);
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
    int fd_ = -1;
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

    socklen_t size = sizeof(receiver.address);
    if (::getsockname(receiver.socket.get(), reinterpret_cast<sockaddr*>(&receiver.address), &size) != 0) {
        throw std::system_error(errno, std::generic_category(), "getsockname");
    }

    return receiver;
}

void send_datagram(int fd, const sockaddr_in& destination, bytes_view bytes) {
    const auto sent = ::sendto(fd,
                               reinterpret_cast<const void*>(bytes.data()),
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
                                     reinterpret_cast<void*>(buffer.data()),
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

TEST_CASE("signal data packet round trips with big endian header and zero-copy payload view") {
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
    packet.set_payload_view(bytes_view{payload.data(), payload.size()});
    packet.set_trailer(Trailer{0xA1B2C3D4u});
    packet.set_spectrum_mode(true);
    packet.header().set_sequence(9u);

    auto bytes = packet.to_bytes();

    REQUIRE(bytes.size() == packet.serialized_size_bytes());
    CHECK(u8(bytes[4]) == 0x01u);
    CHECK(u8(bytes[5]) == 0x02u);
    CHECK(u8(bytes[6]) == 0x03u);
    CHECK(u8(bytes[7]) == 0x04u);
    CHECK(u8(bytes[16]) == 0x11u);
    CHECK(u8(bytes[17]) == 0x22u);
    CHECK(u8(bytes[18]) == 0x33u);
    CHECK(u8(bytes[19]) == 0x44u);

    auto parsed = SignalDataPacketView::parse(as_bytes_view(bytes));

    CHECK(static_cast<int>(parsed.header().packet_type()) == static_cast<int>(PacketType::SignalData));
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
    CHECK(static_cast<int>(parsed.timestamp().integer_type()) == static_cast<int>(IntegerTimestampType::UTC));
    CHECK(static_cast<int>(parsed.timestamp().fractional_type()) == static_cast<int>(FractionalTimestampType::Picoseconds));
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

TEST_CASE("signal data packet without stream id round trips") {
    using namespace vitality;

    std::vector<byte> payload = {byte{0xDE}, byte{0xAD}, byte{0xBE}, byte{0xEF}};

    SignalDataPacket packet;
    packet.set_include_stream_id(false);
    packet.set_payload_view(bytes_view{payload.data(), payload.size()});

    auto bytes = packet.to_bytes();
    auto parsed = SignalDataPacketView::parse(as_bytes_view(bytes));

    CHECK(static_cast<int>(parsed.header().packet_type()) == static_cast<int>(PacketType::SignalDataNoStreamId));
    CHECK(!parsed.header().has_stream_id());
    CHECK(!parsed.stream_id().has_value());
    CHECK(parsed.payload().size() == 4u);
    CHECK(u8(parsed.payload()[0]) == 0xDEu);
    CHECK(u8(parsed.payload()[1]) == 0xADu);
    CHECK(u8(parsed.payload()[2]) == 0xBEu);
    CHECK(u8(parsed.payload()[3]) == 0xEFu);
}

TEST_CASE("context packet round trips supported common fields") {
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

    auto bytes = packet.to_bytes();
    auto parsed = ContextPacketView::parse(as_bytes_view(bytes));

    CHECK(static_cast<int>(parsed.header().packet_type()) == static_cast<int>(PacketType::Context));
    CHECK(parsed.header().timestamp_mode_general());
    REQUIRE(parsed.stream_id().has_value());
    CHECK(parsed.stream_id().value() == 0xCAFEBABEu);
    REQUIRE(parsed.class_id().has_value());
    CHECK(parsed.class_id()->oui() == 0x00ABCDu);
    CHECK(parsed.change_indicator());
    CHECK(parsed.reference_point_id() == 0x10203040u);
    CHECK(parsed.bandwidth_hz() == doctest::Approx(20.5e6).epsilon(1e-12));
    CHECK(parsed.if_reference_frequency_hz() == doctest::Approx(-2.5e6).epsilon(1e-12));
    CHECK(parsed.rf_reference_frequency_hz() == doctest::Approx(915.25e6).epsilon(1e-12));
    CHECK(parsed.rf_reference_frequency_offset_hz() == doctest::Approx(-1250.5).epsilon(1e-12));
    CHECK(parsed.if_band_offset_hz() == doctest::Approx(250.75).epsilon(1e-12));
    CHECK(parsed.reference_level_dbm() == doctest::Approx(-12.5).epsilon(1e-12));
    CHECK(parsed.gain_stage1_db() == doctest::Approx(3.5).epsilon(1e-12));
    CHECK(parsed.gain_stage2_db() == doctest::Approx(-1.0).epsilon(1e-12));
    CHECK(parsed.over_range_count() == 7u);
    CHECK(parsed.sample_rate_sps() == doctest::Approx(30.72e6).epsilon(1e-12));
    CHECK(parsed.timestamp_adjustment_femtoseconds() == -123456789);
    CHECK(parsed.timestamp_calibration_time_seconds() == 99u);
    CHECK(parsed.temperature_celsius() == doctest::Approx(42.25).epsilon(1e-12));
    CHECK(parsed.device_identifier() == 0x11223344u);
    REQUIRE(parsed.has_state_event_indicators());
    REQUIRE(parsed.has_signal_data_format());
    const auto parsed_sei = parsed.state_event_indicators();
    CHECK(parsed_sei.reference_lock());
    CHECK(parsed_sei.over_range());
    CHECK(parsed_sei.user_bits() == 0x5Au);
    const auto parsed_fmt = parsed.signal_data_format();
    CHECK(static_cast<int>(parsed_fmt.packing_method()) == static_cast<int>(PackingMethod::ProcessingEfficient));
    CHECK(static_cast<int>(parsed_fmt.real_complex_type()) == static_cast<int>(RealComplexType::ComplexPolar));
    CHECK(static_cast<int>(parsed_fmt.data_item_format()) == static_cast<int>(DataItemFormat::SignedFixedPoint));
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
    auto ctx_bytes = ctx.to_bytes();
    auto ctx_variant = parse_packet(as_bytes_view(ctx_bytes));
    CHECK(std::holds_alternative<ContextPacketView>(ctx_variant));

    std::vector<byte> payload = {byte{0}, byte{1}, byte{2}, byte{3}};
    SignalDataPacket sig;
    sig.set_stream_id(0x01020304u);
    sig.set_payload_view(bytes_view{payload.data(), payload.size()});
    auto sig_bytes = sig.to_bytes();
    auto sig_variant = parse_packet(as_bytes_view(sig_bytes));
    CHECK(std::holds_alternative<SignalDataPacketView>(sig_variant));
}

TEST_CASE("signal packet rejects non word aligned payload on serialize") {
    using namespace vitality;

    std::vector<byte> payload = {byte{1}, byte{2}, byte{3}};
    SignalDataPacket packet;
    packet.set_stream_id(1u);
    packet.set_payload_view(bytes_view{payload.data(), payload.size()});

    expect_invalid_argument([&] { (void)packet.to_bytes(); });
}

TEST_CASE("context packet requires stream id on serialize") {
    using namespace vitality;

    ContextPacket packet;
    expect_invalid_argument([&] { (void)packet.to_bytes(); });
}

TEST_CASE("parse rejects invalid packet size field") {
    using namespace vitality;

    std::vector<byte> payload = {byte{0}, byte{1}, byte{2}, byte{3}};
    SignalDataPacket packet;
    packet.set_stream_id(0xAABBCCDDu);
    packet.set_payload_view(bytes_view{payload.data(), payload.size()});
    auto bytes = packet.to_bytes();

    bytes[2] = byte{0x00};
    bytes[3] = byte{0x01};

    expect_parse_error([&] { (void)SignalDataPacketView::parse(as_bytes_view(bytes)); },
                       ParseErrorCode::InvalidPacketSize);
}

TEST_CASE("parse rejects unsupported context cif0 indicators") {
    using namespace vitality;

    ContextPacket packet;
    packet.set_stream_id(0x01020304u);
    auto bytes = packet.to_bytes();

    bytes[8] = byte{0x00};
    bytes[9] = byte{0x00};
    bytes[10] = byte{0x40};
    bytes[11] = byte{0x00};

    expect_parse_error([&] { (void)ContextPacketView::parse(as_bytes_view(bytes)); },
                       ParseErrorCode::UnsupportedContextIndicators);
}

TEST_CASE("context view missing field accessor throws") {
    using namespace vitality;

    ContextPacket packet;
    packet.set_stream_id(0x01020304u);
    packet.set_bandwidth_hz(1.0e6);
    auto bytes = packet.to_bytes();
    auto parsed = ContextPacketView::parse(as_bytes_view(bytes));

    expect_parse_error([&] { (void)parsed.reference_point_id(); },
                       ParseErrorCode::MissingRequiredField);
}

TEST_CASE("socket loopback round trips context and signal packets") {
    using namespace vitality;

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
    auto receiver = bind_loopback_receiver();
    auto sender = make_udp_socket();

    ContextPacket context;
    context.set_stream_id(0x0A0B0C0Du);
    context.set_change_indicator(true);
    context.set_bandwidth_hz(2.5e6);
    context.set_rf_reference_frequency_hz(433.92e6);
    context.set_sample_rate_sps(1.25e6);

    SignalDataFormat format;
    format.set_packing_method(PackingMethod::ProcessingEfficient);
    format.set_real_complex_type(RealComplexType::ComplexCartesian);
    format.set_data_item_format(DataItemFormat::SignedFixedPoint);
    format.set_data_item_size(16);
    format.set_item_packing_field_size(16);
    context.set_signal_data_format(format);

    std::vector<byte> payload = {
        byte{0x12}, byte{0x34}, byte{0x56}, byte{0x78},
        byte{0x9A}, byte{0xBC}, byte{0xDE}, byte{0xF0},
    };

    Timestamp ts;
    ts.set_integer_type(IntegerTimestampType::UTC);
    ts.set_fractional_type(FractionalTimestampType::SampleCount);
    ts.set_integer_seconds(12345u);
    ts.set_fractional(67890u);

    SignalDataPacket signal;
    signal.set_stream_id(0x0A0B0C0Du);
    signal.set_timestamp(ts);
    signal.set_payload_view(bytes_view{payload.data(), payload.size()});

    const auto context_bytes = context.to_bytes();
    const auto signal_bytes = signal.to_bytes();

    send_datagram(sender.get(), receiver.address, as_bytes_view(context_bytes));
    send_datagram(sender.get(), receiver.address, as_bytes_view(signal_bytes));

    bool saw_context = false;
    bool saw_signal = false;

    for (int i = 0; i < 2; ++i) {
        auto received = receive_datagram(receiver.socket.get());
        const auto parsed = parse_packet(as_bytes_view(received));

        if (std::holds_alternative<ContextPacketView>(parsed)) {
            const auto& parsed_context = std::get<ContextPacketView>(parsed);
            saw_context = true;
            REQUIRE(parsed_context.stream_id().has_value());
            CHECK(parsed_context.stream_id().value() == 0x0A0B0C0Du);
            CHECK(parsed_context.change_indicator());
            CHECK(parsed_context.bandwidth_hz() == doctest::Approx(2.5e6).epsilon(1e-12));
            CHECK(parsed_context.rf_reference_frequency_hz() == doctest::Approx(433.92e6).epsilon(1e-12));
            CHECK(parsed_context.sample_rate_sps() == doctest::Approx(1.25e6).epsilon(1e-12));
            REQUIRE(parsed_context.has_signal_data_format());
            const auto parsed_format = parsed_context.signal_data_format();
            CHECK(static_cast<int>(parsed_format.real_complex_type()) == static_cast<int>(RealComplexType::ComplexCartesian));
            CHECK(static_cast<int>(parsed_format.data_item_format()) == static_cast<int>(DataItemFormat::SignedFixedPoint));
            CHECK(parsed_format.data_item_size() == 16u);
            CHECK(parsed_format.item_packing_field_size() == 16u);
        } else {
            const auto& parsed_signal = std::get<SignalDataPacketView>(parsed);
            saw_signal = true;
            REQUIRE(parsed_signal.stream_id().has_value());
            CHECK(parsed_signal.stream_id().value() == 0x0A0B0C0Du);
            CHECK(static_cast<int>(parsed_signal.timestamp().integer_type()) == static_cast<int>(IntegerTimestampType::UTC));
            CHECK(static_cast<int>(parsed_signal.timestamp().fractional_type()) == static_cast<int>(FractionalTimestampType::SampleCount));
            CHECK(parsed_signal.timestamp().integer_seconds() == 12345u);
            CHECK(parsed_signal.timestamp().fractional() == 67890u);
            REQUIRE(parsed_signal.payload().size() == payload.size());
            for (std::size_t j = 0; j < payload.size(); ++j) {
                CHECK(u8(parsed_signal.payload()[j]) == u8(payload[j]));
            }
        }
    }

    CHECK(saw_context);
    CHECK(saw_signal);
#else
    DOCTEST_SKIP("POSIX sockets are not available on this platform");
#endif
}
