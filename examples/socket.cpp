#include <vitality/vitality.hpp>

#include <array>
#include <cerrno>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <system_error>
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

namespace {

using vita::byte;
using vita::bytes_view;

int make_udp_socket() {
    const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        throw std::system_error(errno, std::generic_category(), "socket");
    }
    return fd;
}

sockaddr_in bind_loopback_receiver(int fd) {
    timeval timeout{};
    timeout.tv_sec = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
        throw std::system_error(errno, std::generic_category(), "setsockopt(SO_RCVTIMEO)");
    }

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
    if (received < 0) {
        throw std::system_error(errno, std::generic_category(), "recvfrom");
    }

    return std::vector<byte>(buffer.begin(), buffer.begin() + received);
}

} // namespace

int main() {
    int receiver_fd = -1;
    int sender_fd = -1;

    try {
        receiver_fd = make_udp_socket();
        sender_fd = make_udp_socket();
        const auto receiver = bind_loopback_receiver(receiver_fd);

        std::vector<std::complex<float>> tx_samples = {
            {1.0f, -1.0f},
            {2.5f, 0.25f},
        };

        vita::packet::context context;
        context.set_stream_id(0x12345678u);
        context.set_sample_rate_sps(7.68e6);
        context.set_rf_reference_frequency_hz(915.0e6);

        vita::packet::signal signal;
        signal.set_stream_id(0x12345678u);
        signal.set_payload_view(vita::as_bytes_view(tx_samples));

        const auto context_bytes = context.to_bytes();
        const auto signal_bytes = signal.to_bytes();

        send_datagram(sender_fd, receiver, vita::as_bytes_view(context_bytes));
        send_datagram(sender_fd, receiver, vita::as_bytes_view(signal_bytes));

        for (int i = 0; i < 2; ++i) {
            const auto received = receive_datagram(receiver_fd);
            const auto packet = vita::packet::parse(vita::as_bytes_view(received));

            if (std::holds_alternative<vita::view::context>(packet)) {
                const auto& view = std::get<vita::view::context>(packet);
                std::cout << "context: stream=0x" << std::hex << view.stream_id().value() << std::dec
                          << " rf=" << view.rf_reference_frequency_hz()
                          << " sample_rate=" << view.sample_rate_sps() << "\n";
            } else {
                const auto& view = std::get<vita::view::signal>(packet);
                std::vector<std::complex<float>> rx_samples(view.payload().size() / sizeof(std::complex<float>));
                std::memcpy(rx_samples.data(), view.payload().data(), view.payload().size());

                // Payload bytes are not swapped automatically. For a wire format
                // that stores float32 words in opposite byte order, do this after
                // copying the payload into typed storage:
                // vita::byteswap_inplace(std::span<std::complex<float>>(rx_samples));

                std::cout << "signal: stream=0x" << std::hex << view.stream_id().value() << std::dec
                          << " first_sample=(" << rx_samples.front().real() << ", "
                          << rx_samples.front().imag() << ")\n";
            }
        }

        ::close(sender_fd);
        ::close(receiver_fd);
        return 0;
    } catch (const std::exception& ex) {
        if (sender_fd >= 0) {
            ::close(sender_fd);
        }
        if (receiver_fd >= 0) {
            ::close(receiver_fd);
        }
        std::cerr << "socket example failed: " << ex.what() << '\n';
        return 1;
    }
}
