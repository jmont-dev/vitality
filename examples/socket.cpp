#include <vitality/vitality.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <complex>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <system_error>
#include <vector>

int main() {
    std::vector<std::complex<float>> tx_samples = {
        {1.0f, -1.0f},
        {2.5f, 0.25f},
        {-3.0f, 4.0f},
        {0.0f, 0.5f},
    };

    vita::packet::signal packet;
    packet.set_stream_id(0x12345678u);
    packet.set_payload_view(vita::as_bytes_view(tx_samples));
    const auto wire_bytes = packet.to_bytes();

    const int rx_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    const int tx_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (rx_fd < 0 || tx_fd < 0) {
        throw std::system_error(errno, std::generic_category(), "socket");
    }

    sockaddr_in rx_addr{};
    rx_addr.sin_family = AF_INET;
    rx_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    rx_addr.sin_port = htons(0);

    if (::bind(rx_fd, reinterpret_cast<const sockaddr*>(&rx_addr), sizeof(rx_addr)) != 0) {
        throw std::system_error(errno, std::generic_category(), "bind");
    }

    socklen_t rx_addr_len = sizeof(rx_addr);
    if (::getsockname(rx_fd, reinterpret_cast<sockaddr*>(&rx_addr), &rx_addr_len) != 0) {
        throw std::system_error(errno, std::generic_category(), "getsockname");
    }

    const auto sent = ::sendto(tx_fd,
                               wire_bytes.data(),
                               wire_bytes.size(),
                               0,
                               reinterpret_cast<const sockaddr*>(&rx_addr),
                               sizeof(rx_addr));
    if (sent < 0 || static_cast<std::size_t>(sent) != wire_bytes.size()) {
        throw std::system_error(errno, std::generic_category(), "sendto");
    }

    std::vector<vita::byte> recv_buffer(2048);
    sockaddr_in src_addr{};
    socklen_t src_addr_len = sizeof(src_addr);
    const auto received = ::recvfrom(rx_fd,
                                     recv_buffer.data(),
                                     recv_buffer.size(),
                                     0,
                                     reinterpret_cast<sockaddr*>(&src_addr),
                                     &src_addr_len);
    if (received < 0) {
        throw std::system_error(errno, std::generic_category(), "recvfrom");
    }
    recv_buffer.resize(static_cast<std::size_t>(received));

    const auto view = vita::view::signal::parse(vita::as_bytes_view(recv_buffer));

    std::vector<std::complex<float>> rx_samples(view.payload().size() / sizeof(std::complex<float>));
    std::memcpy(rx_samples.data(), view.payload().data(), view.payload().size());

    // Vitality byte-swaps VITA metadata fields for you.
    // Payload byte order is application-defined, so swap payload words only when
    // your stream convention requires it.
    // Example for float32 IQ payloads received in opposite byte order:
    // vita::byteswap_inplace(std::span<std::complex<float>>(rx_samples));

    std::cout << "stream id: 0x" << std::hex << view.stream_id().value_or(0u) << std::dec << "\n";
    for (const auto& sample : rx_samples) {
        std::cout << sample.real() << ", " << sample.imag() << "\n";
    }

    ::close(tx_fd);
    ::close(rx_fd);
    return 0;
}
