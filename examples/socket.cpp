#include <vitality/vitality.hpp>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <complex>
#include <cstring>
#include <iostream>
#include <span>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace {

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

    socket_handle& operator=(socket_handle&& other) noexcept {
        if (this != &other) {
            if (fd_ >= 0) {
                ::close(fd_);
            }
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    [[nodiscard]] int get() const noexcept { return fd_; }

private:
    int fd_ = -1;
};

socket_handle make_udp_socket() {
    const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        throw std::system_error(errno, std::generic_category(), "socket");
    }
    return socket_handle{fd};
}

sockaddr_in bind_loopback(int fd) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(0);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (::bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
        throw std::system_error(errno, std::generic_category(), "bind");
    }

    socklen_t size = sizeof(addr);
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &size) != 0) {
        throw std::system_error(errno, std::generic_category(), "getsockname");
    }

    return addr;
}

} // namespace

int main() {
    try {
        auto sender = make_udp_socket();
        auto receiver = make_udp_socket();
        const sockaddr_in receiver_addr = bind_loopback(receiver.get());

        std::vector<std::complex<float>> tx_samples = {
            {1.0f, -1.0f},
            {2.5f, 0.25f},
            {-3.0f, 4.0f},
        };

        vita::signal::packet packet;
        packet.set_stream_id(0x12345678u);
        packet.set_payload_view(vita::as_bytes_view(tx_samples));

        const std::vector<vita::byte> wire_bytes = packet.to_bytes();

        const auto sent = ::sendto(sender.get(),
                                   wire_bytes.data(),
                                   wire_bytes.size(),
                                   0,
                                   reinterpret_cast<const sockaddr*>(&receiver_addr),
                                   sizeof(receiver_addr));
        if (sent < 0) {
            throw std::system_error(errno, std::generic_category(), "sendto");
        }

        std::vector<vita::byte> recv_buffer(2048);
        sockaddr_in source{};
        socklen_t source_size = sizeof(source);
        const auto received = ::recvfrom(receiver.get(),
                                         recv_buffer.data(),
                                         recv_buffer.size(),
                                         0,
                                         reinterpret_cast<sockaddr*>(&source),
                                         &source_size);
        if (received < 0) {
            throw std::system_error(errno, std::generic_category(), "recvfrom");
        }
        recv_buffer.resize(static_cast<std::size_t>(received));

        const auto view = vita::signal::view::parse(vita::as_bytes_view(recv_buffer));

        std::vector<std::complex<float>> rx_samples(view.payload().size() / sizeof(std::complex<float>));
        std::memcpy(rx_samples.data(), view.payload().data(), view.payload().size());

        // Vitality already decodes VITA metadata fields as big-endian.
        // Payload bytes are left untouched so you can choose the right interpretation.
        // If the sender's float word order differs from your host's order, byteswap after copying.
        // Example:
        // if (vita::host_is_little_endian()) {
        //     vita::byteswap_inplace(std::span<std::complex<float>>(rx_samples));
        // }

        std::cout << "stream id: 0x" << std::hex << view.stream_id().value_or(0) << std::dec << "\n";
        for (const auto& sample : rx_samples) {
            std::cout << sample.real() << ", " << sample.imag() << "\n";
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
