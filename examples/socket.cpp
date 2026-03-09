#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <system_error>
#include <vector>

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#else
#error "examples/socket.cpp currently requires POSIX sockets"
#endif

#include "vitality/vitality.hpp"

namespace {

using vitality::byte;
using vitality::bytes_view;

struct Socket {
    explicit Socket(int fd = -1) noexcept : fd(fd) {}
    ~Socket() {
        if (fd >= 0) {
            ::close(fd);
        }
    }

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept : fd(other.fd) { other.fd = -1; }
    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            if (fd >= 0) {
                ::close(fd);
            }
            fd = other.fd;
            other.fd = -1;
        }
        return *this;
    }

    int fd;
};

[[nodiscard]] Socket make_udp_socket() {
    const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        throw std::system_error(errno, std::generic_category(), "socket");
    }
    return Socket(fd);
}

[[nodiscard]] sockaddr_in bind_loopback_receiver(int fd) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(0);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (::bind(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
        throw std::system_error(errno, std::generic_category(), "bind");
    }

    socklen_t size = sizeof(address);
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&address), &size) != 0) {
        throw std::system_error(errno, std::generic_category(), "getsockname");
    }

    return address;
}

[[nodiscard]] std::vector<byte> copy_to_payload_bytes(const std::vector<std::complex<float>>& samples) {
    std::vector<byte> payload(samples.size() * sizeof(std::complex<float>));
    std::memcpy(payload.data(), samples.data(), payload.size());
    return payload;
}

[[nodiscard]] std::vector<std::complex<float>> copy_from_payload_bytes(bytes_view payload) {
    if (payload.size() % sizeof(std::complex<float>) != 0) {
        throw std::runtime_error("payload size is not a whole number of std::complex<float> samples");
    }

    std::vector<std::complex<float>> samples(payload.size() / sizeof(std::complex<float>));
    std::memcpy(samples.data(), payload.data(), payload.size());
    return samples;
}

[[nodiscard]] std::uint32_t byteswap32(std::uint32_t value) {
    return ((value & 0x000000FFu) << 24u) |
           ((value & 0x0000FF00u) << 8u) |
           ((value & 0x00FF0000u) >> 8u) |
           ((value & 0xFF000000u) >> 24u);
}

void byteswap_float32_components_inplace(std::vector<byte>& payload) {
    if (payload.size() % sizeof(std::uint32_t) != 0) {
        throw std::runtime_error("float32 payload must be a multiple of 4 bytes");
    }

    for (std::size_t offset = 0; offset < payload.size(); offset += sizeof(std::uint32_t)) {
        std::uint32_t word = 0;
        std::memcpy(&word, payload.data() + offset, sizeof(word));
        word = byteswap32(word);
        std::memcpy(payload.data() + offset, &word, sizeof(word));
    }
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
    if (static_cast<std::size_t>(sent) != bytes.size()) {
        throw std::runtime_error("short UDP send");
    }
}

[[nodiscard]] std::vector<byte> receive_datagram(int fd) {
    std::array<byte, 2048> buffer{};
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

} // namespace

int main() {
    using namespace vitality;

    try {
        // Start with normal in-memory IQ samples.
        std::vector<std::complex<float>> tx_samples = {
            {1.0f, -1.0f},
            {0.25f, 0.5f},
            {-0.75f, 0.125f},
        };

        // Vitality exposes payloads as bytes so it does not guess the sample format.
        // For localhost on the same machine, copying the native std::complex<float>
        // bytes is usually enough for a simple example.
        std::vector<byte> payload = copy_to_payload_bytes(tx_samples);

        // If your radio or protocol profile expects float32 components in big-endian
        // order, byte-swap each 32-bit I/Q component before sending:
        //
        //   byteswap_float32_components_inplace(payload);
        //
        // And after receiving, run the same helper again before copying back into
        // std::complex<float> values on a little-endian host.

        SignalDataPacket packet;
        packet.set_stream_id(0x12345678u);
        packet.set_payload_view(bytes_view{payload.data(), payload.size()});

        Socket receiver = make_udp_socket();
        const sockaddr_in receiver_address = bind_loopback_receiver(receiver.fd);
        Socket sender = make_udp_socket();

        const auto wire_bytes = packet.to_bytes();
        send_datagram(sender.fd, receiver_address, as_bytes_view(wire_bytes));

        std::vector<byte> received_bytes = receive_datagram(receiver.fd);
        SignalDataPacketView received = SignalDataPacketView::parse(as_bytes_view(received_bytes));

        std::cout << "stream id: 0x" << std::hex << received.stream_id().value() << std::dec << "\n";
        std::cout << "payload bytes: " << received.payload().size() << "\n";

        // This copies into typed samples safely. Vitality itself already parsed the
        // packet zero-copy; this extra copy is only to materialize std::complex<float>.
        std::vector<std::complex<float>> rx_samples = copy_from_payload_bytes(received.payload());

        // If the payload was byte-swapped before send, undo it first:
        //
        //   std::vector<byte> payload_copy(received.payload().begin(), received.payload().end());
        //   byteswap_float32_components_inplace(payload_copy);
        //   rx_samples = copy_from_payload_bytes(bytes_view{payload_copy.data(), payload_copy.size()});

        for (std::size_t i = 0; i < rx_samples.size(); ++i) {
            std::cout << "sample[" << i << "] = ("
                      << rx_samples[i].real() << ", "
                      << rx_samples[i].imag() << ")\n";
        }

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "socket example failed: " << ex.what() << '\n';
        return 1;
    }
}
