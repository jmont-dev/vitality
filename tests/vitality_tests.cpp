#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <vitality/vitality.hpp>

#include <algorithm>
#include <array>
#include <complex>
#include <cstring>
#include <span>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#include <system_error>
#endif

namespace {

[[nodiscard]] std::uint8_t u8(vita::byte value) {
    return std::to_integer<std::uint8_t>(value);
}

#if defined(__unix__) || defined(__APPLE__)
class socket_handle {
public:
    explicit socket_handle(int fd = -1) noexcept : fd_(fd) {}
    ~socket_handle() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    socket_handle(const socket_handle&) = delete;
    socket_handle& operator=(const socket_handle&) = delete;

    socket_handle(socket_handle&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }

    [[nodiscard]] int get() const noexcept { return fd_; }

private:
    int fd_ = -1;
};

struct udp_pair {
    socket_handle sender;
    socket_handle receiver;
    sockaddr_in receiver_addr{};
};

udp_pair make_udp_pair() {
    udp_pair pair{socket_handle{::socket(AF_INET, SOCK_DGRAM, 0)},
                  socket_handle{::socket(AF_INET, SOCK_DGRAM, 0)},
                  {}};

    if (pair.sender.get() < 0 || pair.receiver.get() < 0) {
        throw std::system_error(errno, std::generic_category(), "socket");
    }

    pair.receiver_addr.sin_family = AF_INET;
    pair.receiver_addr.sin_port = htons(0);
    pair.receiver_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (::bind(pair.receiver.get(), reinterpret_cast<const sockaddr*>(&pair.receiver_addr), sizeof(pair.receiver_addr)) != 0) {
        throw std::system_error(errno, std::generic_category(), "bind");
    }

    socklen_t size = sizeof(pair.receiver_addr);
    if (::getsockname(pair.receiver.get(), reinterpret_cast<sockaddr*>(&pair.receiver_addr), &size) != 0) {
        throw std::system_error(errno, std::generic_category(), "getsockname");
    }

    return pair;
}

void send_bytes(int fd, const sockaddr_in& addr, vita::bytes_view bytes) {
    const auto sent = ::sendto(fd,
                               bytes.data(),
                               bytes.size(),
                               0,
                               reinterpret_cast<const sockaddr*>(&addr),
                               sizeof(addr));
    if (sent < 0) {
        throw std::system_error(errno, std::generic_category(), "sendto");
    }
    REQUIRE(static_cast<std::size_t>(sent) == bytes.size());
}

std::vector<vita::byte> recv_bytes(int fd) {
    std::array<vita::byte, 4096> buffer{};
    sockaddr_in source{};
    socklen_t size = sizeof(source);
    const auto received = ::recvfrom(fd,
                                     buffer.data(),
                                     buffer.size(),
                                     0,
                                     reinterpret_cast<sockaddr*>(&source),
                                     &size);
    if (received < 0) {
        throw std::system_error(errno, std::generic_category(), "recvfrom");
    }
    return {buffer.begin(), buffer.begin() + received};
}
#endif

} // namespace

TEST_CASE("signal packet round-trip keeps payload as a view") {
    std::vector<std::complex<float>> samples = {
        {1.0f, 2.0f},
        {-3.5f, 4.5f},
    };

    vita::timestamp ts;
    ts.set_integer_type(vita::integer_timestamp_type::UTC);
    ts.set_fractional_type(vita::fractional_timestamp_type::Picoseconds);
    ts.set_integer_seconds(42u);
    ts.set_fractional(99u);

    vita::signal::packet packet;
    packet.header().set_sequence(7u);
    packet.set_stream_id(0x12345678u);
    packet.set_timestamp(ts);
    packet.set_payload_view(vita::as_bytes_view(samples));

    const auto bytes = packet.to_bytes();
    const auto view = vita::signal::view::parse(vita::as_bytes_view(bytes));

    CHECK(view.header().sequence() == 7u);
    CHECK(view.stream_id().value() == 0x12345678u);
    CHECK(view.timestamp().integer_seconds() == 42u);
    CHECK(view.payload().size() == samples.size() * sizeof(std::complex<float>));
    CHECK(view.payload().data() == bytes.data() + 20);
}

TEST_CASE("context packet round-trip preserves common fields") {
    vita::timestamp ts;
    ts.set_integer_type(vita::integer_timestamp_type::GPS);
    ts.set_fractional_type(vita::fractional_timestamp_type::SampleCount);
    ts.set_integer_seconds(0x01020304u);
    ts.set_fractional(0x1112131415161718ULL);

    vita::state_event_indicators state;
    state.set_reference_lock(true);
    state.set_over_range(true);
    state.set_user_bits(0x5Au);

    vita::signal::format format;
    format.set_packing_method(vita::packing_method::ProcessingEfficient);
    format.set_real_complex_type(vita::real_complex_type::ComplexCartesian);
    format.set_data_item_format(vita::data_item_format::IEEE754Single);
    format.set_item_packing_field_size(32);
    format.set_data_item_size(32);
    format.set_vector_size(8);

    vita::context::packet packet;
    packet.set_stream_id(0xCAFEBABEu);
    packet.set_timestamp(ts);
    packet.set_change_indicator(true);
    packet.set_bandwidth_hz(20.5e6);
    packet.set_rf_reference_frequency_hz(915.25e6);
    packet.set_reference_level_dbm(-12.5);
    packet.set_sample_rate_sps(30.72e6);
    packet.set_state_event_indicators(state);
    packet.set_signal_data_format(format);

    const auto bytes = packet.to_bytes();
    const auto view = vita::context::view::parse(vita::as_bytes_view(bytes));

    CHECK(view.stream_id().value() == 0xCAFEBABEu);
    CHECK(view.change_indicator());
    CHECK(view.bandwidth_hz() == doctest::Approx(20.5e6));
    CHECK(view.rf_reference_frequency_hz() == doctest::Approx(915.25e6));
    CHECK(view.reference_level_dbm() == doctest::Approx(-12.5));
    CHECK(view.sample_rate_sps() == doctest::Approx(30.72e6));
    CHECK(view.state_event_indicators().reference_lock());
    CHECK(view.state_event_indicators().over_range());
    CHECK(view.signal_data_format().data_item_format() == vita::data_item_format::IEEE754Single);
}

TEST_CASE("packet parse dispatch returns the expected view type") {
    std::vector<vita::byte> raw = {vita::byte{0}, vita::byte{1}, vita::byte{2}, vita::byte{3}};

    vita::signal::packet signal;
    signal.set_stream_id(1u);
    signal.set_payload_view(vita::as_bytes_view(raw));
    const auto signal_bytes = signal.to_bytes();
    const auto signal_any = vita::packet::parse(vita::as_bytes_view(signal_bytes));
    CHECK(std::holds_alternative<vita::signal::view>(signal_any));

    vita::context::packet context;
    context.set_stream_id(2u);
    const auto context_bytes = context.to_bytes();
    const auto context_any = vita::packet::parse(vita::as_bytes_view(context_bytes));
    CHECK(std::holds_alternative<vita::context::view>(context_any));
}

TEST_CASE("byteswap helpers cover common wire types") {
    CHECK(vita::byteswap16(std::uint16_t{0x1234u}) == 0x3412u);
    CHECK(vita::byteswap32(std::uint32_t{0x01020304u}) == 0x04030201u);
    CHECK(vita::byteswap64(std::uint64_t{0x0102030405060708ULL}) == 0x0807060504030201ULL);

    float f = 1.0f;
    vita::byteswap_inplace(f);
    vita::byteswap_inplace(f);
    CHECK(f == doctest::Approx(1.0f));

    std::vector<std::complex<float>> samples = {{1.0f, -2.0f}, {3.5f, 4.5f}};
    const auto original = samples;
    vita::byteswap_inplace(std::span<std::complex<float>>(samples));
    vita::byteswap_inplace(std::span<std::complex<float>>(samples));
    CHECK(samples == original);
}

TEST_CASE("metadata words are serialized as big-endian") {
    std::vector<vita::byte> raw = {vita::byte{0xAA}, vita::byte{0xBB}, vita::byte{0xCC}, vita::byte{0xDD}};

    vita::signal::packet packet;
    packet.set_stream_id(0x01020304u);
    packet.set_payload_view(vita::as_bytes_view(raw));
    const auto bytes = packet.to_bytes();

    CHECK(u8(bytes[4]) == 0x01u);
    CHECK(u8(bytes[5]) == 0x02u);
    CHECK(u8(bytes[6]) == 0x03u);
    CHECK(u8(bytes[7]) == 0x04u);
}

#if defined(__unix__) || defined(__APPLE__)
TEST_CASE("localhost sockets can send and parse context then signal packets") {
    const auto sockets = make_udp_pair();

    vita::context::packet context;
    context.set_stream_id(0xABCDEF01u);
    context.set_bandwidth_hz(1.25e6);
    const auto context_bytes = context.to_bytes();
    send_bytes(sockets.sender.get(), sockets.receiver_addr, vita::as_bytes_view(context_bytes));

    std::vector<std::complex<float>> tx_samples = {
        {1.0f, -1.0f},
        {2.0f, -2.0f},
        {3.0f, -3.0f},
    };

    vita::signal::packet signal;
    signal.set_stream_id(0xABCDEF01u);
    signal.set_payload_view(vita::as_bytes_view(tx_samples));
    const auto signal_bytes = signal.to_bytes();
    send_bytes(sockets.sender.get(), sockets.receiver_addr, vita::as_bytes_view(signal_bytes));

    const auto context_rx = recv_bytes(sockets.receiver.get());
    const auto signal_rx = recv_bytes(sockets.receiver.get());

    const auto context_view = vita::context::view::parse(vita::as_bytes_view(context_rx));
    const auto signal_view = vita::signal::view::parse(vita::as_bytes_view(signal_rx));

    CHECK(context_view.stream_id().value() == 0xABCDEF01u);
    CHECK(context_view.bandwidth_hz() == doctest::Approx(1.25e6));
    CHECK(signal_view.stream_id().value() == 0xABCDEF01u);

    std::vector<std::complex<float>> rx_samples(signal_view.payload().size() / sizeof(std::complex<float>));
    std::memcpy(rx_samples.data(), signal_view.payload().data(), signal_view.payload().size());
    CHECK(rx_samples == tx_samples);
}
#endif
