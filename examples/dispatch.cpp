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
    };

    vita::packet::signal signal_packet;
    signal_packet.set_stream_id(0x12345678u);
    signal_packet.set_payload_view(vita::as_bytes_view(tx_samples));
    const auto signal_wire_bytes = signal_packet.to_bytes();

    vita::packet::context context_packet;
    context_packet.set_stream_id(0xABCDEF01u);
    context_packet.set_change_indicator(true);
    context_packet.set_sample_rate_sps(30.72e6);
    context_packet.set_temperature_celsius(41.5);
    const auto context_wire_bytes = context_packet.to_bytes();

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

    const auto send_packet = [&](vita::bytes_view bytes) {
        const auto sent = ::sendto(tx_fd,
                                   bytes.data(),
                                   bytes.size(),
                                   0,
                                   reinterpret_cast<const sockaddr*>(&rx_addr),
                                   sizeof(rx_addr));
        if (sent < 0 || static_cast<std::size_t>(sent) != bytes.size()) {
            throw std::system_error(errno, std::generic_category(), "sendto");
        }
    };

    send_packet(vita::as_bytes_view(signal_wire_bytes));
    send_packet(vita::as_bytes_view(context_wire_bytes));

    const auto signal_handler = [](const vita::view::signal& view) {
        std::vector<std::complex<float>> samples(view.payload().size() / sizeof(std::complex<float>));
        std::memcpy(samples.data(), view.payload().data(), view.payload().size());

        std::cout << "signal stream id: 0x" << std::hex << view.stream_id().value_or(0u) << std::dec << "\n";
        for (const auto& sample : samples) {
            std::cout << "  " << sample.real() << ", " << sample.imag() << "\n";
        }
    };

    const auto context_handler = [](const vita::view::context& view) {
        std::cout << "context stream id: 0x" << std::hex << view.stream_id().value_or(0u) << std::dec
                  << ", sample-rate=" << (view.has_sample_rate_sps() ? "present" : "missing")
                  << ", temperature=" << (view.has_temperature_celsius() ? "present" : "missing") << "\n";
    };

    for (int i = 0; i < 2; ++i) {
        std::vector<vita::byte> recv_buffer(2048);
        const auto received = ::recv(rx_fd, recv_buffer.data(), recv_buffer.size(), 0);
        if (received < 0) {
            throw std::system_error(errno, std::generic_category(), "recv");
        }
        recv_buffer.resize(static_cast<std::size_t>(received));

        vita::packet::dispatch(vita::as_bytes_view(recv_buffer), signal_handler, context_handler);
    }

    ::close(tx_fd);
    ::close(rx_fd);
    return 0;
}
