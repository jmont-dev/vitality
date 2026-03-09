#ifndef VITALITY_VITALITY_HPP
#define VITALITY_VITALITY_HPP

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <complex>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace vita {

using byte = std::byte;
using bytes_view = std::span<const byte>;
using mutable_bytes_view = std::span<byte>;

[[nodiscard]] constexpr bool host_is_big_endian() noexcept {
    return std::endian::native == std::endian::big;
}

[[nodiscard]] constexpr bool host_is_little_endian() noexcept {
    return std::endian::native == std::endian::little;
}

[[nodiscard]] constexpr std::uint16_t byteswap16(std::uint16_t value) noexcept {
    return static_cast<std::uint16_t>(((value & 0x00FFu) << 8U) |
                                      ((value & 0xFF00u) >> 8U));
}

[[nodiscard]] constexpr std::int16_t byteswap16(std::int16_t value) noexcept {
    return static_cast<std::int16_t>(byteswap16(static_cast<std::uint16_t>(value)));
}

[[nodiscard]] constexpr std::uint32_t byteswap32(std::uint32_t value) noexcept {
    return ((value & 0x000000FFu) << 24U) |
           ((value & 0x0000FF00u) << 8U) |
           ((value & 0x00FF0000u) >> 8U) |
           ((value & 0xFF000000u) >> 24U);
}

[[nodiscard]] constexpr std::int32_t byteswap32(std::int32_t value) noexcept {
    return static_cast<std::int32_t>(byteswap32(static_cast<std::uint32_t>(value)));
}

[[nodiscard]] constexpr std::uint64_t byteswap64(std::uint64_t value) noexcept {
    return ((value & 0x00000000000000FFULL) << 56U) |
           ((value & 0x000000000000FF00ULL) << 40U) |
           ((value & 0x0000000000FF0000ULL) << 24U) |
           ((value & 0x00000000FF000000ULL) << 8U) |
           ((value & 0x000000FF00000000ULL) >> 8U) |
           ((value & 0x0000FF0000000000ULL) >> 24U) |
           ((value & 0x00FF000000000000ULL) >> 40U) |
           ((value & 0xFF00000000000000ULL) >> 56U);
}

[[nodiscard]] constexpr std::int64_t byteswap64(std::int64_t value) noexcept {
    return static_cast<std::int64_t>(byteswap64(static_cast<std::uint64_t>(value)));
}

namespace detail {

template <typename T>
struct is_std_complex : std::false_type {};

template <typename T>
struct is_std_complex<std::complex<T>> : std::true_type {};

template <typename T>
inline constexpr bool is_std_complex_v = is_std_complex<T>::value;

} // namespace detail

template <typename T>
[[nodiscard]] inline T byteswap(T value) noexcept {
    using decayed = std::remove_cv_t<T>;

    if constexpr (std::is_same_v<decayed, std::uint8_t> ||
                  std::is_same_v<decayed, std::int8_t> ||
                  std::is_same_v<decayed, char> ||
                  std::is_same_v<decayed, unsigned char> ||
                  std::is_same_v<decayed, std::byte>) {
        return value;
    } else if constexpr (std::is_same_v<decayed, std::uint16_t>) {
        return byteswap16(value);
    } else if constexpr (std::is_same_v<decayed, std::int16_t>) {
        return byteswap16(value);
    } else if constexpr (std::is_same_v<decayed, std::uint32_t>) {
        return byteswap32(value);
    } else if constexpr (std::is_same_v<decayed, std::int32_t>) {
        return byteswap32(value);
    } else if constexpr (std::is_same_v<decayed, std::uint64_t>) {
        return byteswap64(value);
    } else if constexpr (std::is_same_v<decayed, std::int64_t>) {
        return byteswap64(value);
    } else if constexpr (std::is_same_v<decayed, float>) {
        return std::bit_cast<float>(byteswap32(std::bit_cast<std::uint32_t>(value)));
    } else if constexpr (std::is_same_v<decayed, double>) {
        return std::bit_cast<double>(byteswap64(std::bit_cast<std::uint64_t>(value)));
    } else if constexpr (detail::is_std_complex_v<decayed>) {
        using scalar = typename decayed::value_type;
        return decayed{byteswap(static_cast<scalar>(value.real())),
                       byteswap(static_cast<scalar>(value.imag()))};
    } else {
        static_assert(std::is_same_v<decayed, void>,
                      "vita::byteswap only supports standard 8/16/32/64-bit integers, float, double, std::complex<float>, and std::complex<double>");
    }
}

template <typename T>
inline void byteswap_inplace(T& value) noexcept {
    value = byteswap(value);
}

template <typename T>
inline void byteswap_inplace(std::span<T> values) noexcept {
    for (auto& value : values) {
        byteswap_inplace(value);
    }
}

namespace detail {

[[nodiscard]] inline const std::uint8_t* u8ptr(bytes_view bytes) noexcept {
    return reinterpret_cast<const std::uint8_t*>(bytes.data());
}

[[nodiscard]] inline std::uint8_t* u8ptr(mutable_bytes_view bytes) noexcept {
    return reinterpret_cast<std::uint8_t*>(bytes.data());
}

[[nodiscard]] inline std::uint16_t load_be16(const std::uint8_t* p) noexcept {
    return (static_cast<std::uint16_t>(p[0]) << 8) |
           (static_cast<std::uint16_t>(p[1]));
}

[[nodiscard]] inline std::uint32_t load_be32(const std::uint8_t* p) noexcept {
    return (static_cast<std::uint32_t>(p[0]) << 24) |
           (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) |
           (static_cast<std::uint32_t>(p[3]));
}

[[nodiscard]] inline std::uint64_t load_be64(const std::uint8_t* p) noexcept {
    return (static_cast<std::uint64_t>(p[0]) << 56) |
           (static_cast<std::uint64_t>(p[1]) << 48) |
           (static_cast<std::uint64_t>(p[2]) << 40) |
           (static_cast<std::uint64_t>(p[3]) << 32) |
           (static_cast<std::uint64_t>(p[4]) << 24) |
           (static_cast<std::uint64_t>(p[5]) << 16) |
           (static_cast<std::uint64_t>(p[6]) << 8) |
           (static_cast<std::uint64_t>(p[7]));
}

inline void store_be16(std::uint8_t* p, std::uint16_t value) noexcept {
    p[0] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
    p[1] = static_cast<std::uint8_t>(value & 0xFFu);
}

inline void store_be32(std::uint8_t* p, std::uint32_t value) noexcept {
    p[0] = static_cast<std::uint8_t>((value >> 24) & 0xFFu);
    p[1] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
    p[2] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
    p[3] = static_cast<std::uint8_t>(value & 0xFFu);
}

inline void store_be64(std::uint8_t* p, std::uint64_t value) noexcept {
    p[0] = static_cast<std::uint8_t>((value >> 56) & 0xFFu);
    p[1] = static_cast<std::uint8_t>((value >> 48) & 0xFFu);
    p[2] = static_cast<std::uint8_t>((value >> 40) & 0xFFu);
    p[3] = static_cast<std::uint8_t>((value >> 32) & 0xFFu);
    p[4] = static_cast<std::uint8_t>((value >> 24) & 0xFFu);
    p[5] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
    p[6] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
    p[7] = static_cast<std::uint8_t>(value & 0xFFu);
}

[[nodiscard]] inline std::int16_t sign_extend_16(std::uint16_t value) noexcept {
    return static_cast<std::int16_t>(value);
}

[[nodiscard]] inline std::int32_t sign_extend_32(std::uint32_t value) noexcept {
    return static_cast<std::int32_t>(value);
}

[[nodiscard]] inline std::int64_t sign_extend_64(std::uint64_t value) noexcept {
    return static_cast<std::int64_t>(value);
}

[[nodiscard]] inline double decode_q44_20(std::uint64_t raw) noexcept {
    return static_cast<double>(sign_extend_64(raw)) / static_cast<double>(1ULL << 20U);
}

[[nodiscard]] inline std::uint64_t encode_q44_20(double value) {
    constexpr double scale = static_cast<double>(1ULL << 20U);
    const long double scaled = static_cast<long double>(value) * static_cast<long double>(scale);
    if (scaled < static_cast<long double>(std::numeric_limits<std::int64_t>::min()) ||
        scaled > static_cast<long double>(std::numeric_limits<std::int64_t>::max())) {
        throw std::out_of_range("value does not fit signed 64-bit Q44.20");
    }
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(std::llround(scaled)));
}

[[nodiscard]] inline double decode_q8_7(std::uint16_t raw) noexcept {
    return static_cast<double>(sign_extend_16(raw)) / 128.0;
}

[[nodiscard]] inline std::uint16_t encode_q8_7(double value) {
    const long double scaled = static_cast<long double>(value) * 128.0L;
    if (scaled < static_cast<long double>(std::numeric_limits<std::int16_t>::min()) ||
        scaled > static_cast<long double>(std::numeric_limits<std::int16_t>::max())) {
        throw std::out_of_range("value does not fit signed 16-bit Q8.7");
    }
    return static_cast<std::uint16_t>(static_cast<std::int16_t>(std::llround(scaled)));
}

[[nodiscard]] inline double decode_q9_6(std::uint16_t raw) noexcept {
    return static_cast<double>(sign_extend_16(raw)) / 64.0;
}

[[nodiscard]] inline std::uint16_t encode_q9_6(double value) {
    const long double scaled = static_cast<long double>(value) * 64.0L;
    if (scaled < static_cast<long double>(std::numeric_limits<std::int16_t>::min()) ||
        scaled > static_cast<long double>(std::numeric_limits<std::int16_t>::max())) {
        throw std::out_of_range("value does not fit signed 16-bit Q9.6");
    }
    return static_cast<std::uint16_t>(static_cast<std::int16_t>(std::llround(scaled)));
}

[[nodiscard]] inline byte to_byte(std::uint8_t value) noexcept {
    return static_cast<byte>(value);
}

inline void append_be32(std::vector<byte>& out, std::uint32_t value) {
    const auto start = out.size();
    out.resize(start + 4U);
    store_be32(u8ptr(mutable_bytes_view{out.data() + start, 4U}), value);
}

inline void append_be64(std::vector<byte>& out, std::uint64_t value) {
    const auto start = out.size();
    out.resize(start + 8U);
    store_be64(u8ptr(mutable_bytes_view{out.data() + start, 8U}), value);
}

inline void append_bytes(std::vector<byte>& out, bytes_view payload) {
    out.insert(out.end(), payload.begin(), payload.end());
}

[[nodiscard]] constexpr std::uint32_t packet_header_word(std::uint8_t packet_type,
                                                      bool class_id_included,
                                                      bool trailer_included,
                                                      bool not_v49d0,
                                                      bool packet_specific_flag,
                                                      std::uint8_t tsi,
                                                      std::uint8_t tsf,
                                                      std::uint8_t sequence,
                                                      std::uint16_t packet_size_words) noexcept {
    return (static_cast<std::uint32_t>(packet_type & 0x0Fu) << 28U) |
           (static_cast<std::uint32_t>(class_id_included ? 1U : 0U) << 27U) |
           (static_cast<std::uint32_t>(trailer_included ? 1U : 0U) << 26U) |
           (static_cast<std::uint32_t>(not_v49d0 ? 1U : 0U) << 25U) |
           (static_cast<std::uint32_t>(packet_specific_flag ? 1U : 0U) << 24U) |
           (static_cast<std::uint32_t>(tsi & 0x03U) << 22U) |
           (static_cast<std::uint32_t>(tsf & 0x03U) << 20U) |
           (static_cast<std::uint32_t>(sequence & 0x0FU) << 16U) |
           packet_size_words;
}

} // namespace detail

enum class PacketType : std::uint8_t {
    SignalDataNoStreamId = 0,
    SignalData = 1,
    ExtensionDataNoStreamId = 2,
    ExtensionData = 3,
    Context = 4,
    ExtensionContext = 5,
    Command = 6,
    ExtensionCommand = 7,
};

enum class IntegerTimestampType : std::uint8_t {
    None = 0,
    UTC = 1,
    GPS = 2,
    Other = 3,
};

enum class FractionalTimestampType : std::uint8_t {
    None = 0,
    SampleCount = 1,
    Picoseconds = 2,
    FreeRunningCount = 3,
};

enum class RealComplexType : std::uint8_t {
    Real = 0,
    ComplexCartesian = 1,
    ComplexPolar = 2,
    Reserved = 3,
};

enum class DataItemFormat : std::uint8_t {
    SignedFixedPoint = 0,
    SignedVrt1BitExponent = 1,
    SignedVrt2BitExponent = 2,
    SignedVrt3BitExponent = 3,
    SignedVrt4BitExponent = 4,
    SignedVrt5BitExponent = 5,
    SignedVrt6BitExponent = 6,
    SignedFixedPointNonNormalized = 7,
    IEEE754Half = 13,
    IEEE754Single = 14,
    IEEE754Double = 15,
    UnsignedFixedPoint = 16,
    UnsignedVrt1BitExponent = 17,
    UnsignedVrt2BitExponent = 18,
    UnsignedVrt3BitExponent = 19,
    UnsignedVrt4BitExponent = 20,
    UnsignedVrt5BitExponent = 21,
    UnsignedVrt6BitExponent = 22,
};

enum class PackingMethod : std::uint8_t {
    ProcessingEfficient = 0,
    LinkEfficient = 1,
};

enum class ParseErrorCode {
    BufferTooSmall,
    InvalidPacketSize,
    UnsupportedPacketType,
    UnsupportedContextIndicators,
    MissingRequiredField,
    MalformedPacket,
};

class ParseError : public std::runtime_error {
public:
    ParseError(ParseErrorCode code, std::string message)
        : std::runtime_error(std::move(message)), code_(code) {}

    [[nodiscard]] ParseErrorCode code() const noexcept { return code_; }

private:
    ParseErrorCode code_;
};

class ClassId {
public:
    constexpr ClassId() = default;
    constexpr ClassId(std::uint8_t reserved, std::uint32_t oui, std::uint16_t icc, std::uint16_t pcc) noexcept
        : reserved_(reserved), oui_(oui & 0x00FF'FFFFu), icc_(icc), pcc_(pcc) {}

    [[nodiscard]] static constexpr ClassId from_raw(std::uint64_t raw) noexcept {
        return ClassId(static_cast<std::uint8_t>((raw >> 56U) & 0xFFU),
                       static_cast<std::uint32_t>((raw >> 32U) & 0x00FF'FFFFULL),
                       static_cast<std::uint16_t>((raw >> 16U) & 0xFFFFU),
                       static_cast<std::uint16_t>(raw & 0xFFFFU));
    }

    [[nodiscard]] constexpr std::uint64_t raw() const noexcept {
        return (static_cast<std::uint64_t>(reserved_) << 56U) |
               (static_cast<std::uint64_t>(oui_ & 0x00FF'FFFFu) << 32U) |
               (static_cast<std::uint64_t>(icc_) << 16U) |
               static_cast<std::uint64_t>(pcc_);
    }

    [[nodiscard]] constexpr std::uint8_t reserved() const noexcept { return reserved_; }
    [[nodiscard]] constexpr std::uint32_t oui() const noexcept { return oui_; }
    [[nodiscard]] constexpr std::uint16_t information_class_code() const noexcept { return icc_; }
    [[nodiscard]] constexpr std::uint16_t packet_class_code() const noexcept { return pcc_; }

    constexpr void set_reserved(std::uint8_t value) noexcept { reserved_ = value; }
    constexpr void set_oui(std::uint32_t value) noexcept { oui_ = value & 0x00FF'FFFFu; }
    constexpr void set_information_class_code(std::uint16_t value) noexcept { icc_ = value; }
    constexpr void set_packet_class_code(std::uint16_t value) noexcept { pcc_ = value; }

private:
    std::uint8_t reserved_ = 0;
    std::uint32_t oui_ = 0;
    std::uint16_t icc_ = 0;
    std::uint16_t pcc_ = 0;
};

class Timestamp {
public:
    [[nodiscard]] constexpr IntegerTimestampType integer_type() const noexcept { return integer_type_; }
    [[nodiscard]] constexpr FractionalTimestampType fractional_type() const noexcept { return fractional_type_; }
    [[nodiscard]] constexpr std::uint32_t integer_seconds() const noexcept { return integer_seconds_; }
    [[nodiscard]] constexpr std::uint64_t fractional() const noexcept { return fractional_; }
    [[nodiscard]] constexpr bool has_integer() const noexcept { return integer_type_ != IntegerTimestampType::None; }
    [[nodiscard]] constexpr bool has_fractional() const noexcept { return fractional_type_ != FractionalTimestampType::None; }

    constexpr void set_integer_type(IntegerTimestampType value) noexcept { integer_type_ = value; }
    constexpr void set_fractional_type(FractionalTimestampType value) noexcept { fractional_type_ = value; }
    constexpr void set_integer_seconds(std::uint32_t value) noexcept { integer_seconds_ = value; }
    constexpr void set_fractional(std::uint64_t value) noexcept { fractional_ = value; }

private:
    IntegerTimestampType integer_type_ = IntegerTimestampType::None;
    FractionalTimestampType fractional_type_ = FractionalTimestampType::None;
    std::uint32_t integer_seconds_ = 0;
    std::uint64_t fractional_ = 0;
};

class SignalDataFormat {
public:
    constexpr SignalDataFormat() = default;

    [[nodiscard]] static constexpr SignalDataFormat from_raw_words(std::uint32_t word0, std::uint32_t word1) noexcept {
        SignalDataFormat format;
        format.word0_ = word0;
        format.word1_ = word1;
        return format;
    }

    [[nodiscard]] constexpr std::uint32_t raw_word0() const noexcept { return word0_; }
    [[nodiscard]] constexpr std::uint32_t raw_word1() const noexcept { return word1_; }
    [[nodiscard]] constexpr std::uint64_t raw() const noexcept {
        return (static_cast<std::uint64_t>(word0_) << 32U) | word1_;
    }

    [[nodiscard]] constexpr PackingMethod packing_method() const noexcept {
        return static_cast<PackingMethod>((word0_ >> 31U) & 0x01U);
    }
    [[nodiscard]] constexpr RealComplexType real_complex_type() const noexcept {
        return static_cast<RealComplexType>((word0_ >> 29U) & 0x03U);
    }
    [[nodiscard]] constexpr DataItemFormat data_item_format() const noexcept {
        return static_cast<DataItemFormat>((word0_ >> 24U) & 0x1FU);
    }
    [[nodiscard]] constexpr bool sample_component_repeat() const noexcept {
        return ((word0_ >> 23U) & 0x01U) != 0U;
    }
    [[nodiscard]] constexpr std::uint8_t event_tag_size() const noexcept {
        return static_cast<std::uint8_t>((word0_ >> 20U) & 0x07U);
    }
    [[nodiscard]] constexpr std::uint8_t channel_tag_size() const noexcept {
        return static_cast<std::uint8_t>((word0_ >> 16U) & 0x0FU);
    }
    [[nodiscard]] constexpr std::uint8_t data_item_fraction_size() const noexcept {
        return static_cast<std::uint8_t>((word0_ >> 12U) & 0x0FU);
    }
    [[nodiscard]] constexpr std::uint8_t item_packing_field_size() const noexcept {
        return static_cast<std::uint8_t>((word0_ >> 6U) & 0x3FU);
    }
    [[nodiscard]] constexpr std::uint8_t data_item_size() const noexcept {
        return static_cast<std::uint8_t>(word0_ & 0x3FU);
    }
    [[nodiscard]] constexpr std::uint16_t repeat_count() const noexcept {
        return static_cast<std::uint16_t>((word1_ >> 16U) & 0xFFFFU);
    }
    [[nodiscard]] constexpr std::uint16_t vector_size() const noexcept {
        return static_cast<std::uint16_t>(word1_ & 0xFFFFU);
    }

    constexpr void set_packing_method(PackingMethod value) noexcept {
        word0_ = (word0_ & ~(0x01U << 31U)) | (static_cast<std::uint32_t>(value) << 31U);
    }
    constexpr void set_real_complex_type(RealComplexType value) noexcept {
        word0_ = (word0_ & ~(0x03U << 29U)) | (static_cast<std::uint32_t>(value) << 29U);
    }
    constexpr void set_data_item_format(DataItemFormat value) noexcept {
        word0_ = (word0_ & ~(0x1FU << 24U)) | (static_cast<std::uint32_t>(value) << 24U);
    }
    constexpr void set_sample_component_repeat(bool value) noexcept {
        word0_ = (word0_ & ~(0x01U << 23U)) | (static_cast<std::uint32_t>(value ? 1U : 0U) << 23U);
    }
    constexpr void set_event_tag_size(std::uint8_t value) noexcept {
        word0_ = (word0_ & ~(0x07U << 20U)) | (static_cast<std::uint32_t>(value & 0x07U) << 20U);
    }
    constexpr void set_channel_tag_size(std::uint8_t value) noexcept {
        word0_ = (word0_ & ~(0x0FU << 16U)) | (static_cast<std::uint32_t>(value & 0x0FU) << 16U);
    }
    constexpr void set_data_item_fraction_size(std::uint8_t value) noexcept {
        word0_ = (word0_ & ~(0x0FU << 12U)) | (static_cast<std::uint32_t>(value & 0x0FU) << 12U);
    }
    constexpr void set_item_packing_field_size(std::uint8_t value) noexcept {
        word0_ = (word0_ & ~(0x3FU << 6U)) | (static_cast<std::uint32_t>(value & 0x3FU) << 6U);
    }
    constexpr void set_data_item_size(std::uint8_t value) noexcept {
        word0_ = (word0_ & ~0x3FU) | static_cast<std::uint32_t>(value & 0x3FU);
    }
    constexpr void set_repeat_count(std::uint16_t value) noexcept {
        word1_ = (word1_ & 0x0000FFFFU) | (static_cast<std::uint32_t>(value) << 16U);
    }
    constexpr void set_vector_size(std::uint16_t value) noexcept {
        word1_ = (word1_ & 0xFFFF0000U) | static_cast<std::uint32_t>(value);
    }

private:
    std::uint32_t word0_ = 0;
    std::uint32_t word1_ = 0;
};

class StateEventIndicators {
public:
    constexpr StateEventIndicators() = default;
    explicit constexpr StateEventIndicators(std::uint32_t raw) noexcept : raw_(raw) {}

    [[nodiscard]] constexpr std::uint32_t raw() const noexcept { return raw_; }
    constexpr void set_raw(std::uint32_t value) noexcept { raw_ = value; }

    [[nodiscard]] constexpr bool calibrated_time_enabled() const noexcept { return bit(31U); }
    [[nodiscard]] constexpr bool valid_data_enabled() const noexcept { return bit(30U); }
    [[nodiscard]] constexpr bool reference_lock_enabled() const noexcept { return bit(29U); }
    [[nodiscard]] constexpr bool agc_mgc_enabled() const noexcept { return bit(28U); }
    [[nodiscard]] constexpr bool detected_signal_enabled() const noexcept { return bit(27U); }
    [[nodiscard]] constexpr bool spectral_inversion_enabled() const noexcept { return bit(26U); }
    [[nodiscard]] constexpr bool over_range_enabled() const noexcept { return bit(25U); }
    [[nodiscard]] constexpr bool sample_loss_enabled() const noexcept { return bit(24U); }

    [[nodiscard]] constexpr bool calibrated_time() const noexcept { return bit(19U); }
    [[nodiscard]] constexpr bool valid_data() const noexcept { return bit(18U); }
    [[nodiscard]] constexpr bool reference_lock() const noexcept { return bit(17U); }
    [[nodiscard]] constexpr bool agc_mgc() const noexcept { return bit(16U); }
    [[nodiscard]] constexpr bool detected_signal() const noexcept { return bit(15U); }
    [[nodiscard]] constexpr bool spectral_inversion() const noexcept { return bit(14U); }
    [[nodiscard]] constexpr bool over_range() const noexcept { return bit(13U); }
    [[nodiscard]] constexpr bool sample_loss() const noexcept { return bit(12U); }
    [[nodiscard]] constexpr std::uint8_t user_bits() const noexcept { return static_cast<std::uint8_t>(raw_ & 0xFFU); }

    constexpr void set_calibrated_time_enabled(bool value) noexcept { set_bit(31U, value); }
    constexpr void set_valid_data_enabled(bool value) noexcept { set_bit(30U, value); }
    constexpr void set_reference_lock_enabled(bool value) noexcept { set_bit(29U, value); }
    constexpr void set_agc_mgc_enabled(bool value) noexcept { set_bit(28U, value); }
    constexpr void set_detected_signal_enabled(bool value) noexcept { set_bit(27U, value); }
    constexpr void set_spectral_inversion_enabled(bool value) noexcept { set_bit(26U, value); }
    constexpr void set_over_range_enabled(bool value) noexcept { set_bit(25U, value); }
    constexpr void set_sample_loss_enabled(bool value) noexcept { set_bit(24U, value); }

    constexpr void set_calibrated_time(bool value) noexcept { set_bit(19U, value); }
    constexpr void set_valid_data(bool value) noexcept { set_bit(18U, value); }
    constexpr void set_reference_lock(bool value) noexcept { set_bit(17U, value); }
    constexpr void set_agc_mgc(bool value) noexcept { set_bit(16U, value); }
    constexpr void set_detected_signal(bool value) noexcept { set_bit(15U, value); }
    constexpr void set_spectral_inversion(bool value) noexcept { set_bit(14U, value); }
    constexpr void set_over_range(bool value) noexcept { set_bit(13U, value); }
    constexpr void set_sample_loss(bool value) noexcept { set_bit(12U, value); }
    constexpr void set_user_bits(std::uint8_t value) noexcept {
        raw_ = (raw_ & ~0xFFU) | static_cast<std::uint32_t>(value);
    }

private:
    [[nodiscard]] constexpr bool bit(unsigned position) const noexcept {
        return ((raw_ >> position) & 0x1U) != 0U;
    }
    constexpr void set_bit(unsigned position, bool value) noexcept {
        raw_ = (raw_ & ~(1U << position)) | (static_cast<std::uint32_t>(value ? 1U : 0U) << position);
    }

    std::uint32_t raw_ = 0;
};

class Trailer {
public:
    constexpr Trailer() = default;
    explicit constexpr Trailer(std::uint32_t raw) noexcept : raw_(raw) {}

    [[nodiscard]] constexpr std::uint32_t raw() const noexcept { return raw_; }
    constexpr void set_raw(std::uint32_t value) noexcept { raw_ = value; }

private:
    std::uint32_t raw_ = 0;
};

class PacketHeader {
public:
    PacketHeader() = default;

    [[nodiscard]] constexpr PacketType packet_type() const noexcept { return packet_type_; }
    [[nodiscard]] constexpr bool class_id_included() const noexcept { return class_id_included_; }
    [[nodiscard]] constexpr bool trailer_included() const noexcept { return trailer_included_; }
    [[nodiscard]] constexpr bool v49d1_or_later() const noexcept { return v49d1_or_later_; }
    [[nodiscard]] constexpr bool packet_specific_flag() const noexcept { return packet_specific_flag_; }
    [[nodiscard]] constexpr std::uint8_t sequence() const noexcept { return sequence_; }
    [[nodiscard]] constexpr std::uint16_t packet_size_words() const noexcept { return packet_size_words_; }
    [[nodiscard]] constexpr IntegerTimestampType integer_timestamp_type() const noexcept { return integer_timestamp_type_; }
    [[nodiscard]] constexpr FractionalTimestampType fractional_timestamp_type() const noexcept { return fractional_timestamp_type_; }

    constexpr void set_packet_type(PacketType value) noexcept { packet_type_ = value; }
    constexpr void set_class_id_included(bool value) noexcept { class_id_included_ = value; }
    constexpr void set_trailer_included(bool value) noexcept { trailer_included_ = value; }
    constexpr void set_v49d1_or_later(bool value) noexcept { v49d1_or_later_ = value; }
    constexpr void set_packet_specific_flag(bool value) noexcept { packet_specific_flag_ = value; }
    constexpr void set_sequence(std::uint8_t value) noexcept { sequence_ = value & 0x0FU; }
    constexpr void set_packet_size_words(std::uint16_t value) noexcept { packet_size_words_ = value; }
    constexpr void set_integer_timestamp_type(IntegerTimestampType value) noexcept { integer_timestamp_type_ = value; }
    constexpr void set_fractional_timestamp_type(FractionalTimestampType value) noexcept { fractional_timestamp_type_ = value; }

    [[nodiscard]] constexpr bool is_signal_data() const noexcept {
        return packet_type_ == PacketType::SignalData || packet_type_ == PacketType::SignalDataNoStreamId;
    }
    [[nodiscard]] constexpr bool is_context() const noexcept {
        return packet_type_ == PacketType::Context;
    }
    [[nodiscard]] constexpr bool has_stream_id() const noexcept {
        return packet_type_ == PacketType::SignalData || packet_type_ == PacketType::Context ||
               packet_type_ == PacketType::ExtensionData || packet_type_ == PacketType::ExtensionContext ||
               packet_type_ == PacketType::Command || packet_type_ == PacketType::ExtensionCommand;
    }
    [[nodiscard]] constexpr bool spectrum_mode() const noexcept {
        return is_signal_data() ? packet_specific_flag_ : false;
    }
    [[nodiscard]] constexpr bool timestamp_mode_general() const noexcept {
        return is_context() ? packet_specific_flag_ : false;
    }

    [[nodiscard]] constexpr std::uint32_t raw() const noexcept {
        return detail::packet_header_word(static_cast<std::uint8_t>(packet_type_),
                                          class_id_included_,
                                          trailer_included_,
                                          v49d1_or_later_,
                                          packet_specific_flag_,
                                          static_cast<std::uint8_t>(integer_timestamp_type_),
                                          static_cast<std::uint8_t>(fractional_timestamp_type_),
                                          sequence_,
                                          packet_size_words_);
    }

    [[nodiscard]] static PacketHeader from_raw(std::uint32_t raw) {
        PacketHeader header;
        header.packet_type_ = static_cast<PacketType>((raw >> 28U) & 0x0FU);
        header.class_id_included_ = ((raw >> 27U) & 0x01U) != 0U;
        header.trailer_included_ = ((raw >> 26U) & 0x01U) != 0U;
        header.v49d1_or_later_ = ((raw >> 25U) & 0x01U) != 0U;
        header.packet_specific_flag_ = ((raw >> 24U) & 0x01U) != 0U;
        header.integer_timestamp_type_ = static_cast<IntegerTimestampType>((raw >> 22U) & 0x03U);
        header.fractional_timestamp_type_ = static_cast<FractionalTimestampType>((raw >> 20U) & 0x03U);
        header.sequence_ = static_cast<std::uint8_t>((raw >> 16U) & 0x0FU);
        header.packet_size_words_ = static_cast<std::uint16_t>(raw & 0xFFFFU);
        return header;
    }

private:
    PacketType packet_type_ = PacketType::SignalData;
    bool class_id_included_ = false;
    bool trailer_included_ = false;
    bool v49d1_or_later_ = true;
    bool packet_specific_flag_ = false;
    std::uint8_t sequence_ = 0;
    std::uint16_t packet_size_words_ = 0;
    IntegerTimestampType integer_timestamp_type_ = IntegerTimestampType::None;
    FractionalTimestampType fractional_timestamp_type_ = FractionalTimestampType::None;
};

class SignalDataPacketView {
public:
    SignalDataPacketView() = default;

    [[nodiscard]] const PacketHeader& header() const noexcept { return header_; }
    [[nodiscard]] bytes_view bytes() const noexcept { return packet_; }
    [[nodiscard]] std::optional<std::uint32_t> stream_id() const noexcept { return stream_id_; }
    [[nodiscard]] std::optional<ClassId> class_id() const noexcept { return class_id_; }
    [[nodiscard]] const Timestamp& timestamp() const noexcept { return timestamp_; }
    [[nodiscard]] bytes_view payload() const noexcept { return payload_; }
    [[nodiscard]] std::optional<Trailer> trailer() const noexcept { return trailer_; }

    [[nodiscard]] static SignalDataPacketView parse(bytes_view packet) {
        if (packet.size() < 4U) {
            throw ParseError(ParseErrorCode::BufferTooSmall, "packet smaller than 4-byte VITA header");
        }

        const auto* p = detail::u8ptr(packet);
        const auto header_word = detail::load_be32(p);
        SignalDataPacketView out;
        out.packet_ = packet;
        out.header_ = PacketHeader::from_raw(header_word);

        if (!out.header_.is_signal_data()) {
            throw ParseError(ParseErrorCode::UnsupportedPacketType, "packet is not a supported signal data packet");
        }

        const auto packet_size_words = out.header_.packet_size_words();
        if (packet_size_words == 0U) {
            throw ParseError(ParseErrorCode::InvalidPacketSize, "packet size field is zero");
        }
        const std::size_t packet_size_bytes = static_cast<std::size_t>(packet_size_words) * 4U;
        if (packet_size_bytes != packet.size()) {
            throw ParseError(ParseErrorCode::InvalidPacketSize, "packet size field does not match buffer length");
        }

        std::size_t offset = 4U;
        if (out.header_.has_stream_id()) {
            if (offset + 4U > packet.size()) {
                throw ParseError(ParseErrorCode::BufferTooSmall, "stream ID exceeds packet size");
            }
            out.stream_id_ = detail::load_be32(p + offset);
            offset += 4U;
        }
        if (out.header_.class_id_included()) {
            if (offset + 8U > packet.size()) {
                throw ParseError(ParseErrorCode::BufferTooSmall, "class ID exceeds packet size");
            }
            out.class_id_ = ClassId::from_raw(detail::load_be64(p + offset));
            offset += 8U;
        }
        out.timestamp_.set_integer_type(out.header_.integer_timestamp_type());
        out.timestamp_.set_fractional_type(out.header_.fractional_timestamp_type());
        if (out.timestamp_.has_integer()) {
            if (offset + 4U > packet.size()) {
                throw ParseError(ParseErrorCode::BufferTooSmall, "integer timestamp exceeds packet size");
            }
            out.timestamp_.set_integer_seconds(detail::load_be32(p + offset));
            offset += 4U;
        }
        if (out.timestamp_.has_fractional()) {
            if (offset + 8U > packet.size()) {
                throw ParseError(ParseErrorCode::BufferTooSmall, "fractional timestamp exceeds packet size");
            }
            out.timestamp_.set_fractional(detail::load_be64(p + offset));
            offset += 8U;
        }

        std::size_t trailer_size = out.header_.trailer_included() ? 4U : 0U;
        if (offset + trailer_size > packet.size()) {
            throw ParseError(ParseErrorCode::MalformedPacket, "header fields consume more than packet size");
        }
        const std::size_t payload_size = packet.size() - offset - trailer_size;
        out.payload_ = packet.subspan(offset, payload_size);

        if (out.header_.trailer_included()) {
            out.trailer_ = Trailer(detail::load_be32(p + packet.size() - 4U));
        }

        return out;
    }

private:
    bytes_view packet_{};
    PacketHeader header_{};
    std::optional<std::uint32_t> stream_id_{};
    std::optional<ClassId> class_id_{};
    Timestamp timestamp_{};
    bytes_view payload_{};
    std::optional<Trailer> trailer_{};
};

class ContextPacketView {
public:
    ContextPacketView() = default;

    struct SupportedCif0 {
        static constexpr std::uint32_t change_indicator = 1U << 31U;
        static constexpr std::uint32_t reference_point_id = 1U << 30U;
        static constexpr std::uint32_t bandwidth = 1U << 29U;
        static constexpr std::uint32_t if_reference_frequency = 1U << 28U;
        static constexpr std::uint32_t rf_reference_frequency = 1U << 27U;
        static constexpr std::uint32_t rf_reference_frequency_offset = 1U << 26U;
        static constexpr std::uint32_t if_band_offset = 1U << 25U;
        static constexpr std::uint32_t reference_level = 1U << 24U;
        static constexpr std::uint32_t gain = 1U << 23U;
        static constexpr std::uint32_t over_range_count = 1U << 22U;
        static constexpr std::uint32_t sample_rate = 1U << 21U;
        static constexpr std::uint32_t timestamp_adjustment = 1U << 20U;
        static constexpr std::uint32_t timestamp_calibration_time = 1U << 19U;
        static constexpr std::uint32_t temperature = 1U << 18U;
        static constexpr std::uint32_t device_identifier = 1U << 17U;
        static constexpr std::uint32_t state_event_indicators = 1U << 16U;
        static constexpr std::uint32_t signal_data_format = 1U << 15U;
        static constexpr std::uint32_t all = change_indicator |
                                             reference_point_id |
                                             bandwidth |
                                             if_reference_frequency |
                                             rf_reference_frequency |
                                             rf_reference_frequency_offset |
                                             if_band_offset |
                                             reference_level |
                                             gain |
                                             over_range_count |
                                             sample_rate |
                                             timestamp_adjustment |
                                             timestamp_calibration_time |
                                             temperature |
                                             device_identifier |
                                             state_event_indicators |
                                             signal_data_format;
    };

    [[nodiscard]] const PacketHeader& header() const noexcept { return header_; }
    [[nodiscard]] bytes_view bytes() const noexcept { return packet_; }
    [[nodiscard]] std::optional<std::uint32_t> stream_id() const noexcept { return stream_id_; }
    [[nodiscard]] std::optional<ClassId> class_id() const noexcept { return class_id_; }
    [[nodiscard]] const Timestamp& timestamp() const noexcept { return timestamp_; }
    [[nodiscard]] std::uint32_t cif0_raw() const noexcept { return cif0_; }
    [[nodiscard]] bool change_indicator() const noexcept { return (cif0_ & SupportedCif0::change_indicator) != 0U; }

    [[nodiscard]] bool has_reference_point_id() const noexcept { return ref_point_id_.has_value(); }
    [[nodiscard]] bool has_bandwidth_hz() const noexcept { return bandwidth_.has_value(); }
    [[nodiscard]] bool has_if_reference_frequency_hz() const noexcept { return if_ref_freq_.has_value(); }
    [[nodiscard]] bool has_rf_reference_frequency_hz() const noexcept { return rf_ref_freq_.has_value(); }
    [[nodiscard]] bool has_rf_reference_frequency_offset_hz() const noexcept { return rf_ref_freq_offset_.has_value(); }
    [[nodiscard]] bool has_if_band_offset_hz() const noexcept { return if_band_offset_.has_value(); }
    [[nodiscard]] bool has_reference_level_dbm() const noexcept { return reference_level_.has_value(); }
    [[nodiscard]] bool has_gain_stage1_db() const noexcept { return gain_stage1_.has_value(); }
    [[nodiscard]] bool has_gain_stage2_db() const noexcept { return gain_stage2_.has_value(); }
    [[nodiscard]] bool has_over_range_count() const noexcept { return over_range_count_.has_value(); }
    [[nodiscard]] bool has_sample_rate_sps() const noexcept { return sample_rate_.has_value(); }
    [[nodiscard]] bool has_timestamp_adjustment_femtoseconds() const noexcept { return timestamp_adjustment_.has_value(); }
    [[nodiscard]] bool has_timestamp_calibration_time_seconds() const noexcept { return timestamp_calibration_time_.has_value(); }
    [[nodiscard]] bool has_temperature_celsius() const noexcept { return temperature_.has_value(); }
    [[nodiscard]] bool has_device_identifier() const noexcept { return device_identifier_.has_value(); }
    [[nodiscard]] bool has_state_event_indicators() const noexcept { return state_event_.has_value(); }
    [[nodiscard]] bool has_signal_data_format() const noexcept { return signal_data_format_.has_value(); }

    [[nodiscard]] std::uint32_t reference_point_id() const { return require(ref_point_id_, "reference point ID"); }
    [[nodiscard]] double bandwidth_hz() const { return require(bandwidth_, "bandwidth"); }
    [[nodiscard]] double if_reference_frequency_hz() const { return require(if_ref_freq_, "IF reference frequency"); }
    [[nodiscard]] double rf_reference_frequency_hz() const { return require(rf_ref_freq_, "RF reference frequency"); }
    [[nodiscard]] double rf_reference_frequency_offset_hz() const { return require(rf_ref_freq_offset_, "RF reference frequency offset"); }
    [[nodiscard]] double if_band_offset_hz() const { return require(if_band_offset_, "IF band offset"); }
    [[nodiscard]] double reference_level_dbm() const { return require(reference_level_, "reference level"); }
    [[nodiscard]] double gain_stage1_db() const { return require(gain_stage1_, "gain stage 1"); }
    [[nodiscard]] double gain_stage2_db() const { return require(gain_stage2_, "gain stage 2"); }
    [[nodiscard]] std::uint32_t over_range_count() const { return require(over_range_count_, "over-range count"); }
    [[nodiscard]] double sample_rate_sps() const { return require(sample_rate_, "sample rate"); }
    [[nodiscard]] std::int64_t timestamp_adjustment_femtoseconds() const { return require(timestamp_adjustment_, "timestamp adjustment"); }
    [[nodiscard]] std::uint32_t timestamp_calibration_time_seconds() const { return require(timestamp_calibration_time_, "timestamp calibration time"); }
    [[nodiscard]] double temperature_celsius() const { return require(temperature_, "temperature"); }
    [[nodiscard]] std::uint32_t device_identifier() const { return require(device_identifier_, "device identifier"); }
    [[nodiscard]] StateEventIndicators state_event_indicators() const { return require(state_event_, "state/event indicators"); }
    [[nodiscard]] SignalDataFormat signal_data_format() const { return require(signal_data_format_, "signal data format"); }

    [[nodiscard]] static ContextPacketView parse(bytes_view packet) {
        if (packet.size() < 4U) {
            throw ParseError(ParseErrorCode::BufferTooSmall, "packet smaller than 4-byte VITA header");
        }

        const auto* p = detail::u8ptr(packet);
        const auto header_word = detail::load_be32(p);
        ContextPacketView out;
        out.packet_ = packet;
        out.header_ = PacketHeader::from_raw(header_word);

        if (!out.header_.is_context()) {
            throw ParseError(ParseErrorCode::UnsupportedPacketType, "packet is not a supported context packet");
        }
        if (out.header_.trailer_included()) {
            throw ParseError(ParseErrorCode::MalformedPacket, "context packets do not carry trailers in this implementation");
        }

        const auto packet_size_words = out.header_.packet_size_words();
        if (packet_size_words == 0U) {
            throw ParseError(ParseErrorCode::InvalidPacketSize, "packet size field is zero");
        }
        const std::size_t packet_size_bytes = static_cast<std::size_t>(packet_size_words) * 4U;
        if (packet_size_bytes != packet.size()) {
            throw ParseError(ParseErrorCode::InvalidPacketSize, "packet size field does not match buffer length");
        }

        std::size_t offset = 4U;
        if (out.header_.has_stream_id()) {
            if (offset + 4U > packet.size()) {
                throw ParseError(ParseErrorCode::BufferTooSmall, "stream ID exceeds packet size");
            }
            out.stream_id_ = detail::load_be32(p + offset);
            offset += 4U;
        }
        if (out.header_.class_id_included()) {
            if (offset + 8U > packet.size()) {
                throw ParseError(ParseErrorCode::BufferTooSmall, "class ID exceeds packet size");
            }
            out.class_id_ = ClassId::from_raw(detail::load_be64(p + offset));
            offset += 8U;
        }
        out.timestamp_.set_integer_type(out.header_.integer_timestamp_type());
        out.timestamp_.set_fractional_type(out.header_.fractional_timestamp_type());
        if (out.timestamp_.has_integer()) {
            if (offset + 4U > packet.size()) {
                throw ParseError(ParseErrorCode::BufferTooSmall, "integer timestamp exceeds packet size");
            }
            out.timestamp_.set_integer_seconds(detail::load_be32(p + offset));
            offset += 4U;
        }
        if (out.timestamp_.has_fractional()) {
            if (offset + 8U > packet.size()) {
                throw ParseError(ParseErrorCode::BufferTooSmall, "fractional timestamp exceeds packet size");
            }
            out.timestamp_.set_fractional(detail::load_be64(p + offset));
            offset += 8U;
        }
        if (offset + 4U > packet.size()) {
            throw ParseError(ParseErrorCode::BufferTooSmall, "missing CIF0 in context packet");
        }
        out.cif0_ = detail::load_be32(p + offset);
        offset += 4U;

        const std::uint32_t unsupported = out.cif0_ & ~SupportedCif0::all;
        if (unsupported != 0U) {
            throw ParseError(ParseErrorCode::UnsupportedContextIndicators,
                             "context packet uses CIF0 fields outside Vitality's common supported subset");
        }

        auto consume32 = [&](std::uint32_t bit, auto&& fn) {
            if ((out.cif0_ & bit) != 0U) {
                if (offset + 4U > packet.size()) {
                    throw ParseError(ParseErrorCode::BufferTooSmall, "context field exceeds packet size");
                }
                fn(detail::load_be32(p + offset));
                offset += 4U;
            }
        };
        auto consume64 = [&](std::uint32_t bit, auto&& fn) {
            if ((out.cif0_ & bit) != 0U) {
                if (offset + 8U > packet.size()) {
                    throw ParseError(ParseErrorCode::BufferTooSmall, "context field exceeds packet size");
                }
                fn(detail::load_be64(p + offset));
                offset += 8U;
            }
        };

        consume32(SupportedCif0::reference_point_id, [&](std::uint32_t raw) { out.ref_point_id_ = raw; });
        consume64(SupportedCif0::bandwidth, [&](std::uint64_t raw) { out.bandwidth_ = detail::decode_q44_20(raw); });
        consume64(SupportedCif0::if_reference_frequency, [&](std::uint64_t raw) { out.if_ref_freq_ = detail::decode_q44_20(raw); });
        consume64(SupportedCif0::rf_reference_frequency, [&](std::uint64_t raw) { out.rf_ref_freq_ = detail::decode_q44_20(raw); });
        consume64(SupportedCif0::rf_reference_frequency_offset, [&](std::uint64_t raw) { out.rf_ref_freq_offset_ = detail::decode_q44_20(raw); });
        consume64(SupportedCif0::if_band_offset, [&](std::uint64_t raw) { out.if_band_offset_ = detail::decode_q44_20(raw); });
        consume32(SupportedCif0::reference_level, [&](std::uint32_t raw) {
            out.reference_level_ = detail::decode_q8_7(static_cast<std::uint16_t>(raw & 0xFFFFU));
        });
        consume32(SupportedCif0::gain, [&](std::uint32_t raw) {
            out.gain_stage1_ = detail::decode_q8_7(static_cast<std::uint16_t>((raw >> 16U) & 0xFFFFU));
            out.gain_stage2_ = detail::decode_q8_7(static_cast<std::uint16_t>(raw & 0xFFFFU));
        });
        consume32(SupportedCif0::over_range_count, [&](std::uint32_t raw) { out.over_range_count_ = raw; });
        consume64(SupportedCif0::sample_rate, [&](std::uint64_t raw) { out.sample_rate_ = detail::decode_q44_20(raw); });
        consume64(SupportedCif0::timestamp_adjustment, [&](std::uint64_t raw) {
            out.timestamp_adjustment_ = detail::sign_extend_64(raw);
        });
        consume32(SupportedCif0::timestamp_calibration_time, [&](std::uint32_t raw) { out.timestamp_calibration_time_ = raw; });
        consume32(SupportedCif0::temperature, [&](std::uint32_t raw) {
            out.temperature_ = detail::decode_q9_6(static_cast<std::uint16_t>(raw & 0xFFFFU));
        });
        consume32(SupportedCif0::device_identifier, [&](std::uint32_t raw) { out.device_identifier_ = raw; });
        consume32(SupportedCif0::state_event_indicators, [&](std::uint32_t raw) { out.state_event_ = StateEventIndicators(raw); });
        consume64(SupportedCif0::signal_data_format, [&](std::uint64_t raw) {
            out.signal_data_format_ = SignalDataFormat::from_raw_words(static_cast<std::uint32_t>(raw >> 32U),
                                                                      static_cast<std::uint32_t>(raw & 0xFFFF'FFFFULL));
        });

        if (offset != packet.size()) {
            throw ParseError(ParseErrorCode::MalformedPacket, "context packet has trailing bytes after supported fields");
        }

        return out;
    }

private:
    template <typename T>
    [[nodiscard]] static T require(const std::optional<T>& value, const char* field_name) {
        if (!value.has_value()) {
            throw ParseError(ParseErrorCode::MissingRequiredField, std::string("missing field: ") + field_name);
        }
        return *value;
    }

    bytes_view packet_{};
    PacketHeader header_{};
    std::optional<std::uint32_t> stream_id_{};
    std::optional<ClassId> class_id_{};
    Timestamp timestamp_{};
    std::uint32_t cif0_ = 0;

    std::optional<std::uint32_t> ref_point_id_{};
    std::optional<double> bandwidth_{};
    std::optional<double> if_ref_freq_{};
    std::optional<double> rf_ref_freq_{};
    std::optional<double> rf_ref_freq_offset_{};
    std::optional<double> if_band_offset_{};
    std::optional<double> reference_level_{};
    std::optional<double> gain_stage1_{};
    std::optional<double> gain_stage2_{};
    std::optional<std::uint32_t> over_range_count_{};
    std::optional<double> sample_rate_{};
    std::optional<std::int64_t> timestamp_adjustment_{};
    std::optional<std::uint32_t> timestamp_calibration_time_{};
    std::optional<double> temperature_{};
    std::optional<std::uint32_t> device_identifier_{};
    std::optional<StateEventIndicators> state_event_{};
    std::optional<SignalDataFormat> signal_data_format_{};
};

class SignalDataPacket {
public:
    SignalDataPacket() {
        header_.set_packet_type(PacketType::SignalData);
        header_.set_v49d1_or_later(true);
    }

    [[nodiscard]] PacketHeader& header() noexcept { return header_; }
    [[nodiscard]] const PacketHeader& header() const noexcept { return header_; }
    [[nodiscard]] std::optional<std::uint32_t> stream_id() const noexcept { return stream_id_; }
    [[nodiscard]] std::optional<ClassId> class_id() const noexcept { return class_id_; }
    [[nodiscard]] Timestamp& timestamp() noexcept { return timestamp_; }
    [[nodiscard]] const Timestamp& timestamp() const noexcept { return timestamp_; }
    [[nodiscard]] bytes_view payload() const noexcept {
        if (payload_view_.has_value()) {
            return *payload_view_;
        }
        return bytes_view{payload_storage_.data(), payload_storage_.size()};
    }
    [[nodiscard]] std::optional<Trailer> trailer() const noexcept { return trailer_; }

    void set_include_stream_id(bool value) noexcept {
        header_.set_packet_type(value ? PacketType::SignalData : PacketType::SignalDataNoStreamId);
        if (!value) {
            stream_id_.reset();
        }
    }
    void set_spectrum_mode(bool value) noexcept { header_.set_packet_specific_flag(value); }
    void set_stream_id(std::optional<std::uint32_t> value) noexcept {
        stream_id_ = value;
        set_include_stream_id(value.has_value());
    }
    void set_class_id(std::optional<ClassId> value) noexcept {
        class_id_ = value;
        header_.set_class_id_included(value.has_value());
    }
    void set_timestamp(const Timestamp& value) noexcept {
        timestamp_ = value;
        header_.set_integer_timestamp_type(value.integer_type());
        header_.set_fractional_timestamp_type(value.fractional_type());
    }
    void set_payload(bytes_view data) {
        payload_storage_.assign(data.begin(), data.end());
        payload_view_.reset();
    }
    void set_payload_view(bytes_view data) noexcept {
        payload_view_ = data;
        payload_storage_.clear();
    }
    void set_trailer(std::optional<Trailer> value) noexcept {
        trailer_ = value;
        header_.set_trailer_included(value.has_value());
    }

    [[nodiscard]] std::size_t serialized_size_bytes() const noexcept {
        std::size_t size = 4U;
        if (header_.has_stream_id()) size += 4U;
        if (class_id_.has_value()) size += 8U;
        if (timestamp_.has_integer()) size += 4U;
        if (timestamp_.has_fractional()) size += 8U;
        size += payload().size();
        if (trailer_.has_value()) size += 4U;
        return size;
    }

    [[nodiscard]] std::vector<byte> to_bytes() const {
        const auto payload_bytes = payload();
        if ((payload_bytes.size() % 4U) != 0U) {
            throw std::invalid_argument("signal data payload must be padded to a 32-bit word boundary");
        }

        std::vector<byte> out;
        out.reserve(serialized_size_bytes());

        PacketHeader wire_header = header_;
        wire_header.set_class_id_included(class_id_.has_value());
        wire_header.set_trailer_included(trailer_.has_value());
        wire_header.set_integer_timestamp_type(timestamp_.integer_type());
        wire_header.set_fractional_timestamp_type(timestamp_.fractional_type());
        wire_header.set_packet_size_words(static_cast<std::uint16_t>(serialized_size_bytes() / 4U));

        detail::append_be32(out, wire_header.raw());
        if (wire_header.has_stream_id()) {
            if (!stream_id_.has_value()) {
                throw std::invalid_argument("stream ID is required when packet type includes stream ID");
            }
            detail::append_be32(out, *stream_id_);
        }
        if (class_id_.has_value()) {
            detail::append_be64(out, class_id_->raw());
        }
        if (timestamp_.has_integer()) {
            detail::append_be32(out, timestamp_.integer_seconds());
        }
        if (timestamp_.has_fractional()) {
            detail::append_be64(out, timestamp_.fractional());
        }
        detail::append_bytes(out, payload_bytes);
        if (trailer_.has_value()) {
            detail::append_be32(out, trailer_->raw());
        }
        return out;
    }

private:
    PacketHeader header_{};
    std::optional<std::uint32_t> stream_id_{};
    std::optional<ClassId> class_id_{};
    Timestamp timestamp_{};
    std::vector<byte> payload_storage_{};
    std::optional<bytes_view> payload_view_{};
    std::optional<Trailer> trailer_{};
};

class ContextPacket {
public:
    ContextPacket() {
        header_.set_packet_type(PacketType::Context);
        header_.set_v49d1_or_later(true);
        header_.set_packet_specific_flag(false);
    }

    [[nodiscard]] PacketHeader& header() noexcept { return header_; }
    [[nodiscard]] const PacketHeader& header() const noexcept { return header_; }
    [[nodiscard]] std::optional<std::uint32_t> stream_id() const noexcept { return stream_id_; }
    [[nodiscard]] std::optional<ClassId> class_id() const noexcept { return class_id_; }
    [[nodiscard]] Timestamp& timestamp() noexcept { return timestamp_; }
    [[nodiscard]] const Timestamp& timestamp() const noexcept { return timestamp_; }

    void set_stream_id(std::uint32_t value) noexcept { stream_id_ = value; }
    void clear_stream_id() noexcept { stream_id_.reset(); }
    void set_class_id(std::optional<ClassId> value) noexcept {
        class_id_ = value;
        header_.set_class_id_included(value.has_value());
    }
    void set_timestamp(const Timestamp& value) noexcept {
        timestamp_ = value;
        header_.set_integer_timestamp_type(value.integer_type());
        header_.set_fractional_timestamp_type(value.fractional_type());
    }
    void set_timestamp_mode_general(bool value) noexcept { header_.set_packet_specific_flag(value); }
    void set_change_indicator(bool value) noexcept { change_indicator_ = value; }

    [[nodiscard]] bool change_indicator() const noexcept { return change_indicator_; }
    [[nodiscard]] std::optional<std::uint32_t> reference_point_id() const noexcept { return reference_point_id_; }
    [[nodiscard]] std::optional<double> bandwidth_hz() const noexcept { return bandwidth_hz_; }
    [[nodiscard]] std::optional<double> if_reference_frequency_hz() const noexcept { return if_reference_frequency_hz_; }
    [[nodiscard]] std::optional<double> rf_reference_frequency_hz() const noexcept { return rf_reference_frequency_hz_; }
    [[nodiscard]] std::optional<double> rf_reference_frequency_offset_hz() const noexcept { return rf_reference_frequency_offset_hz_; }
    [[nodiscard]] std::optional<double> if_band_offset_hz() const noexcept { return if_band_offset_hz_; }
    [[nodiscard]] std::optional<double> reference_level_dbm() const noexcept { return reference_level_dbm_; }
    [[nodiscard]] std::optional<std::pair<double, double>> gain_db() const noexcept { return gain_db_; }
    [[nodiscard]] std::optional<std::uint32_t> over_range_count() const noexcept { return over_range_count_; }
    [[nodiscard]] std::optional<double> sample_rate_sps() const noexcept { return sample_rate_sps_; }
    [[nodiscard]] std::optional<std::int64_t> timestamp_adjustment_femtoseconds() const noexcept { return timestamp_adjustment_femtoseconds_; }
    [[nodiscard]] std::optional<std::uint32_t> timestamp_calibration_time_seconds() const noexcept { return timestamp_calibration_time_seconds_; }
    [[nodiscard]] std::optional<double> temperature_celsius() const noexcept { return temperature_celsius_; }
    [[nodiscard]] std::optional<std::uint32_t> device_identifier() const noexcept { return device_identifier_; }
    [[nodiscard]] std::optional<StateEventIndicators> state_event_indicators() const noexcept { return state_event_indicators_; }
    [[nodiscard]] std::optional<SignalDataFormat> signal_data_format() const noexcept { return signal_data_format_; }

    void set_reference_point_id(std::optional<std::uint32_t> value) noexcept { reference_point_id_ = value; }
    void set_bandwidth_hz(std::optional<double> value) noexcept { bandwidth_hz_ = value; }
    void set_if_reference_frequency_hz(std::optional<double> value) noexcept { if_reference_frequency_hz_ = value; }
    void set_rf_reference_frequency_hz(std::optional<double> value) noexcept { rf_reference_frequency_hz_ = value; }
    void set_rf_reference_frequency_offset_hz(std::optional<double> value) noexcept { rf_reference_frequency_offset_hz_ = value; }
    void set_if_band_offset_hz(std::optional<double> value) noexcept { if_band_offset_hz_ = value; }
    void set_reference_level_dbm(std::optional<double> value) noexcept { reference_level_dbm_ = value; }
    void set_gain_db(std::optional<std::pair<double, double>> value) noexcept { gain_db_ = value; }
    void set_over_range_count(std::optional<std::uint32_t> value) noexcept { over_range_count_ = value; }
    void set_sample_rate_sps(std::optional<double> value) noexcept { sample_rate_sps_ = value; }
    void set_timestamp_adjustment_femtoseconds(std::optional<std::int64_t> value) noexcept { timestamp_adjustment_femtoseconds_ = value; }
    void set_timestamp_calibration_time_seconds(std::optional<std::uint32_t> value) noexcept { timestamp_calibration_time_seconds_ = value; }
    void set_temperature_celsius(std::optional<double> value) noexcept { temperature_celsius_ = value; }
    void set_device_identifier(std::optional<std::uint32_t> value) noexcept { device_identifier_ = value; }
    void set_state_event_indicators(std::optional<StateEventIndicators> value) noexcept { state_event_indicators_ = value; }
    void set_signal_data_format(std::optional<SignalDataFormat> value) noexcept { signal_data_format_ = value; }

    [[nodiscard]] std::uint32_t cif0_raw() const noexcept {
        std::uint32_t cif0 = change_indicator_ ? ContextPacketView::SupportedCif0::change_indicator : 0U;
        if (reference_point_id_.has_value()) cif0 |= ContextPacketView::SupportedCif0::reference_point_id;
        if (bandwidth_hz_.has_value()) cif0 |= ContextPacketView::SupportedCif0::bandwidth;
        if (if_reference_frequency_hz_.has_value()) cif0 |= ContextPacketView::SupportedCif0::if_reference_frequency;
        if (rf_reference_frequency_hz_.has_value()) cif0 |= ContextPacketView::SupportedCif0::rf_reference_frequency;
        if (rf_reference_frequency_offset_hz_.has_value()) cif0 |= ContextPacketView::SupportedCif0::rf_reference_frequency_offset;
        if (if_band_offset_hz_.has_value()) cif0 |= ContextPacketView::SupportedCif0::if_band_offset;
        if (reference_level_dbm_.has_value()) cif0 |= ContextPacketView::SupportedCif0::reference_level;
        if (gain_db_.has_value()) cif0 |= ContextPacketView::SupportedCif0::gain;
        if (over_range_count_.has_value()) cif0 |= ContextPacketView::SupportedCif0::over_range_count;
        if (sample_rate_sps_.has_value()) cif0 |= ContextPacketView::SupportedCif0::sample_rate;
        if (timestamp_adjustment_femtoseconds_.has_value()) cif0 |= ContextPacketView::SupportedCif0::timestamp_adjustment;
        if (timestamp_calibration_time_seconds_.has_value()) cif0 |= ContextPacketView::SupportedCif0::timestamp_calibration_time;
        if (temperature_celsius_.has_value()) cif0 |= ContextPacketView::SupportedCif0::temperature;
        if (device_identifier_.has_value()) cif0 |= ContextPacketView::SupportedCif0::device_identifier;
        if (state_event_indicators_.has_value()) cif0 |= ContextPacketView::SupportedCif0::state_event_indicators;
        if (signal_data_format_.has_value()) cif0 |= ContextPacketView::SupportedCif0::signal_data_format;
        return cif0;
    }

    [[nodiscard]] std::size_t serialized_size_bytes() const noexcept {
        std::size_t size = 4U;  // header
        size += 4U;             // stream id, always present for supported context packets
        if (class_id_.has_value()) size += 8U;
        if (timestamp_.has_integer()) size += 4U;
        if (timestamp_.has_fractional()) size += 8U;
        size += 4U; // CIF0
        if (reference_point_id_.has_value()) size += 4U;
        if (bandwidth_hz_.has_value()) size += 8U;
        if (if_reference_frequency_hz_.has_value()) size += 8U;
        if (rf_reference_frequency_hz_.has_value()) size += 8U;
        if (rf_reference_frequency_offset_hz_.has_value()) size += 8U;
        if (if_band_offset_hz_.has_value()) size += 8U;
        if (reference_level_dbm_.has_value()) size += 4U;
        if (gain_db_.has_value()) size += 4U;
        if (over_range_count_.has_value()) size += 4U;
        if (sample_rate_sps_.has_value()) size += 8U;
        if (timestamp_adjustment_femtoseconds_.has_value()) size += 8U;
        if (timestamp_calibration_time_seconds_.has_value()) size += 4U;
        if (temperature_celsius_.has_value()) size += 4U;
        if (device_identifier_.has_value()) size += 4U;
        if (state_event_indicators_.has_value()) size += 4U;
        if (signal_data_format_.has_value()) size += 8U;
        return size;
    }

    [[nodiscard]] std::vector<byte> to_bytes() const {
        if (!stream_id_.has_value()) {
            throw std::invalid_argument("context packets require a stream ID");
        }

        std::vector<byte> out;
        out.reserve(serialized_size_bytes());

        PacketHeader wire_header = header_;
        wire_header.set_packet_type(PacketType::Context);
        wire_header.set_class_id_included(class_id_.has_value());
        wire_header.set_trailer_included(false);
        wire_header.set_integer_timestamp_type(timestamp_.integer_type());
        wire_header.set_fractional_timestamp_type(timestamp_.fractional_type());
        wire_header.set_packet_size_words(static_cast<std::uint16_t>(serialized_size_bytes() / 4U));

        detail::append_be32(out, wire_header.raw());
        detail::append_be32(out, *stream_id_);
        if (class_id_.has_value()) {
            detail::append_be64(out, class_id_->raw());
        }
        if (timestamp_.has_integer()) {
            detail::append_be32(out, timestamp_.integer_seconds());
        }
        if (timestamp_.has_fractional()) {
            detail::append_be64(out, timestamp_.fractional());
        }
        detail::append_be32(out, cif0_raw());
        if (reference_point_id_.has_value()) {
            detail::append_be32(out, *reference_point_id_);
        }
        if (bandwidth_hz_.has_value()) {
            detail::append_be64(out, detail::encode_q44_20(*bandwidth_hz_));
        }
        if (if_reference_frequency_hz_.has_value()) {
            detail::append_be64(out, detail::encode_q44_20(*if_reference_frequency_hz_));
        }
        if (rf_reference_frequency_hz_.has_value()) {
            detail::append_be64(out, detail::encode_q44_20(*rf_reference_frequency_hz_));
        }
        if (rf_reference_frequency_offset_hz_.has_value()) {
            detail::append_be64(out, detail::encode_q44_20(*rf_reference_frequency_offset_hz_));
        }
        if (if_band_offset_hz_.has_value()) {
            detail::append_be64(out, detail::encode_q44_20(*if_band_offset_hz_));
        }
        if (reference_level_dbm_.has_value()) {
            detail::append_be32(out, static_cast<std::uint32_t>(detail::encode_q8_7(*reference_level_dbm_)));
        }
        if (gain_db_.has_value()) {
            const std::uint32_t stage1 = static_cast<std::uint32_t>(detail::encode_q8_7(gain_db_->first));
            const std::uint32_t stage2 = static_cast<std::uint32_t>(detail::encode_q8_7(gain_db_->second));
            detail::append_be32(out, (stage1 << 16U) | stage2);
        }
        if (over_range_count_.has_value()) {
            detail::append_be32(out, *over_range_count_);
        }
        if (sample_rate_sps_.has_value()) {
            detail::append_be64(out, detail::encode_q44_20(*sample_rate_sps_));
        }
        if (timestamp_adjustment_femtoseconds_.has_value()) {
            detail::append_be64(out, static_cast<std::uint64_t>(*timestamp_adjustment_femtoseconds_));
        }
        if (timestamp_calibration_time_seconds_.has_value()) {
            detail::append_be32(out, *timestamp_calibration_time_seconds_);
        }
        if (temperature_celsius_.has_value()) {
            detail::append_be32(out, static_cast<std::uint32_t>(detail::encode_q9_6(*temperature_celsius_)));
        }
        if (device_identifier_.has_value()) {
            detail::append_be32(out, *device_identifier_);
        }
        if (state_event_indicators_.has_value()) {
            detail::append_be32(out, state_event_indicators_->raw());
        }
        if (signal_data_format_.has_value()) {
            detail::append_be32(out, signal_data_format_->raw_word0());
            detail::append_be32(out, signal_data_format_->raw_word1());
        }

        return out;
    }

private:
    PacketHeader header_{};
    std::optional<std::uint32_t> stream_id_{};
    std::optional<ClassId> class_id_{};
    Timestamp timestamp_{};
    bool change_indicator_ = false;

    std::optional<std::uint32_t> reference_point_id_{};
    std::optional<double> bandwidth_hz_{};
    std::optional<double> if_reference_frequency_hz_{};
    std::optional<double> rf_reference_frequency_hz_{};
    std::optional<double> rf_reference_frequency_offset_hz_{};
    std::optional<double> if_band_offset_hz_{};
    std::optional<double> reference_level_dbm_{};
    std::optional<std::pair<double, double>> gain_db_{};
    std::optional<std::uint32_t> over_range_count_{};
    std::optional<double> sample_rate_sps_{};
    std::optional<std::int64_t> timestamp_adjustment_femtoseconds_{};
    std::optional<std::uint32_t> timestamp_calibration_time_seconds_{};
    std::optional<double> temperature_celsius_{};
    std::optional<std::uint32_t> device_identifier_{};
    std::optional<StateEventIndicators> state_event_indicators_{};
    std::optional<SignalDataFormat> signal_data_format_{};
};

using ParsedPacket = std::variant<SignalDataPacketView, ContextPacketView>;

[[nodiscard]] inline ParsedPacket parse_packet(bytes_view packet) {
    if (packet.size() < 1U) {
        throw ParseError(ParseErrorCode::BufferTooSmall, "buffer is empty");
    }
    const auto* p = detail::u8ptr(packet);
    const auto packet_type = static_cast<PacketType>((p[0] >> 4U) & 0x0FU);
    switch (packet_type) {
        case PacketType::SignalDataNoStreamId:
        case PacketType::SignalData:
            return SignalDataPacketView::parse(packet);
        case PacketType::Context:
            return ContextPacketView::parse(packet);
        default:
            throw ParseError(ParseErrorCode::UnsupportedPacketType, "Vitality only supports common signal-data and context packet types");
    }
}

[[nodiscard]] inline bytes_view as_bytes_view(const std::vector<byte>& bytes) noexcept {
    return bytes_view{bytes.data(), bytes.size()};
}

template <typename T>
[[nodiscard]] inline bytes_view as_bytes_view(std::span<const T> values) noexcept {
    return std::as_bytes(values);
}

template <typename T>
[[nodiscard]] inline bytes_view as_bytes_view(const std::vector<T>& values) noexcept {
    return as_bytes_view(std::span<const T>{values.data(), values.size()});
}

template <typename T>
[[nodiscard]] inline mutable_bytes_view as_writable_bytes_view(std::span<T> values) noexcept {
    return std::as_writable_bytes(values);
}

template <typename T>
[[nodiscard]] inline mutable_bytes_view as_writable_bytes_view(std::vector<T>& values) noexcept {
    return as_writable_bytes_view(std::span<T>{values.data(), values.size()});
}

[[nodiscard]] inline std::vector<byte> from_u8(const std::vector<std::uint8_t>& bytes) {
    std::vector<byte> out(bytes.size());
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        out[i] = static_cast<byte>(bytes[i]);
    }
    return out;
}

[[nodiscard]] inline std::vector<std::uint8_t> to_u8(bytes_view bytes) {
    std::vector<std::uint8_t> out(bytes.size());
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        out[i] = std::to_integer<std::uint8_t>(bytes[i]);
    }
    return out;
}



using class_id = ClassId;
using timestamp = Timestamp;
using trailer = Trailer;
using packet_header = PacketHeader;
using state_event_indicators = StateEventIndicators;
using parse_error = ParseError;
using parse_error_code = ParseErrorCode;
using packet_type = PacketType;
using integer_timestamp_type = IntegerTimestampType;
using fractional_timestamp_type = FractionalTimestampType;
using real_complex_type = RealComplexType;
using data_item_format = DataItemFormat;
using packing_method = PackingMethod;

namespace signal {
using format = SignalDataFormat;
}

namespace view {
using signal = SignalDataPacketView;
using context = ContextPacketView;
}

namespace packet {
using signal = SignalDataPacket;
using context = ContextPacket;
using parsed = ParsedPacket;

[[nodiscard]] inline parsed parse(bytes_view packet) {
    return parse_packet(packet);
}
} // namespace packet

} // namespace vita

#endif // VITALITY_VITALITY_HPP
