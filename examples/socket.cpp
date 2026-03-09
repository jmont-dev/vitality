#include <array>
#include <cerrno>
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

int main() {
    using namespace vitality;

    try {
        const int receiver_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (receiver_fd < 0) {
            throw std::system_error(errno, std::generic_category(), "socket(receiver)");
        }

        sockaddr_in receiver_addr{};
        receiver_addr.sin_family = AF_INET;
        receiver_addr.sin_port = htons(0);
        receiver_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        if (::bind(receiver_fd, reinterpret_cast<const sockaddr*>(&receiver_addr), sizeof(receiver_addr)) != 0) {
            throw std::system_error(errno, std::generic_category(), "bind");
        }

        socklen_t receiver_addr_size = sizeof(receiver_addr);
        if (::getsockname(receiver_fd, reinterpret_cast<sockaddr*>(&receiver_addr), &receiver_addr_size) != 0) {
            throw std::system_error(errno, std::generic_category(), "getsockname");
        }

        const int sender_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (sender_fd < 0) {
            throw std::system_error(errno, std::generic_category(), "socket(sender)");
        }

        std::vector<std::complex<float>> iq_samples = {
            {1.0f, -1.0f},
            {0.5f, 0.25f},
            {-0.75f, 0.125f},
        };

        // If your wire convention expects float32 payload words in the opposite byte order
        // from your host, swap the payload before send and again after receive.
        // Example:
        // if (host_is_little_endian()) {
        //     byteswap_inplace(std::span<std::complex<float>>{iq_samples});
        // }

        SignalDataPacket packet;
        packet.set_stream_id(0x12345678u);
        packet.set_payload_view(as_bytes_view(iq_samples));

        const auto wire_bytes = packet.to_bytes();
        const auto sent = ::sendto(sender_fd,
                                   wire_bytes.data(),
                                   wire_bytes.size(),
                                   0,
                                   reinterpret_cast<const sockaddr*>(&receiver_addr),
                                   sizeof(receiver_addr));
        if (sent < 0) {
            throw std::system_error(errno, std::generic_category(), "sendto");
        }

        std::array<byte, 2048> recv_buffer{};
        sockaddr_in source_addr{};
        socklen_t source_addr_size = sizeof(source_addr);
        const auto received = ::recvfrom(receiver_fd,
                                         recv_buffer.data(),
                                         recv_buffer.size(),
                                         0,
                                         reinterpret_cast<sockaddr*>(&source_addr),
                                         &source_addr_size);
        if (received < 0) {
            throw std::system_error(errno, std::generic_category(), "recvfrom");
        }

        const bytes_view received_bytes{recv_buffer.data(), static_cast<std::size_t>(received)};
        const auto signal = SignalDataPacketView::parse(received_bytes);

        std::vector<std::complex<float>> received_samples(signal.payload().size() / sizeof(std::complex<float>));
        std::memcpy(received_samples.data(), signal.payload().data(), signal.payload().size());

        // If you swapped before send, swap again here after copying out of the payload view.
        // byteswap_inplace(std::span<std::complex<float>>{received_samples});

        std::cout << "stream id = 0x" << std::hex << signal.stream_id().value() << std::dec << "\n";
        std::cout << "samples received = " << received_samples.size() << "\n";
        std::cout << "first sample = (" << received_samples.front().real()
                  << ", " << received_samples.front().imag() << ")\n";

        ::close(sender_fd);
        ::close(receiver_fd);
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "socket example failed: " << ex.what() << '\n';
        return 1;
    }
}
