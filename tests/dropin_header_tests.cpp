#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "../Vitality.hpp"

#include <vector>

TEST_CASE("single-file Vitality.hpp exposes packet::dispatch") {
    std::vector<vita::byte> payload = {vita::byte{0}, vita::byte{1}, vita::byte{2}, vita::byte{3}};

    vita::packet::signal sig;
    sig.set_stream_id(0x01020304u);
    sig.set_payload_view(vita::bytes_view{payload.data(), payload.size()});

    const auto bytes = sig.to_bytes();
    bool signal_called = false;
    bool context_called = false;

    vita::packet::dispatch(
        vita::as_bytes_view(bytes),
        [&](const vita::view::signal&) { signal_called = true; },
        [&](const vita::view::context&) { context_called = true; });

    CHECK(signal_called);
    CHECK_FALSE(context_called);
}
