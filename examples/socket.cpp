#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#else
#error "examples/socket.cpp currently requires POSIX sockets"
#endif

#include "vitality/vitality.hpp"

namespace {

using vitality::byte;
using vitality::bytes_view;

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
    const auto* data = reinterpret_cast<const void*>(bytes.data());
    const auto sent = ::sendto(fd,
                               data,
                               bytes.size(),
                               0,
                               reinterpret_cast<const sockaddr*>(&destination),
                               sizeof(destination));
    if (sent < 0) {
        throw std::system_error(errno, std::generic_category(), "sendto");
    }
    if (static_cast<std::size_t>(sent) != bytes.size()) {
        throw std::runtime_error("short UDP send");
    }
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

std::string packet_kind_name(const vitality::ParsedPacket& packet) {
    if (std::holds_alternative<vitality::ContextPacketView>(packet)) {
        return "context";
    }
    return "signal";
}

} // namespace

int main() {
    using namespace vitality;

    try {
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
        format.set_data_item_format(DataItemFormat::SignedFixedPoint);
        format.set_data_item_size(16);
        format.set_item_packing_field_size(16);
        context.set_signal_data_format(format);

        std::vector<byte> iq_samples = {
            byte{0x00}, byte{0x01}, byte{0x00}, byte{0x02},
            byte{0x00}, byte{0x03}, byte{0x00}, byte{0x04},
        };

        SignalDataPacket signal;
        signal.set_stream_id(0x12345678u);
        signal.set_payload_view(bytes_view{iq_samples.data(), iq_samples.size()});

        const auto context_bytes = context.to_bytes();
        const auto signal_bytes = signal.to_bytes();

        send_datagram(sender.get(), receiver.address, as_bytes_view(context_bytes));
        send_datagram(sender.get(), receiver.address, as_bytes_view(signal_bytes));

        for (int i = 0; i < 2; ++i) {
            auto received = receive_datagram(receiver.socket.get());
            const auto parsed = parse_packet(as_bytes_view(received));

            std::cout << "received " << packet_kind_name(parsed) << " packet (" << received.size() << " bytes)\n";

            if (std::holds_alternative<ContextPacketView>(parsed)) {
                const auto& ctx = std::get<ContextPacketView>(parsed);
                std::cout << "  stream id: 0x" << std::hex << ctx.stream_id().value() << std::dec << "\n";
                std::cout << "  rf center: " << ctx.rf_reference_frequency_hz() << " Hz\n";
                std::cout << "  sample rate: " << ctx.sample_rate_sps() << " sps\n";
            } else {
                const auto& sig = std::get<SignalDataPacketView>(parsed);
                std::cout << "  stream id: 0x" << std::hex << sig.stream_id().value() << std::dec << "\n";
                std::cout << "  payload bytes: " << sig.payload().size() << "\n";
            }
        }

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "socket example failed: " << ex.what() << '\n';
        return 1;
    }
}
