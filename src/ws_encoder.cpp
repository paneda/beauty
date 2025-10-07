#include "beauty/ws_encoder.hpp"
#include <chrono>
#include <random>

namespace beauty {

// Server constructor - defaults to SERVER role, no masking
WsEncoder::WsEncoder(std::vector<char>& buffer) : buffer_(buffer), role_(SERVER), random_(nullptr) {
    buffer_.clear();  // Start with empty buffer
}

// Client constructor - CLIENT role with required random generator
WsEncoder::WsEncoder(std::vector<char>& buffer, IRandom& random)
    : buffer_(buffer), role_(CLIENT), random_(&random) {
    buffer_.clear();  // Start with empty buffer
}

void WsEncoder::encodeTextFrame(const std::string& text, bool final) {
    std::vector<char> payload(text.begin(), text.end());
    encodeFrame(TextData, payload, final, role_ == CLIENT);
}

void WsEncoder::encodeBinaryFrame(const std::vector<char>& data, bool final) {
    encodeFrame(BinData, data, final, role_ == CLIENT);
}

void WsEncoder::encodePingFrame(const std::string& payload) {
    std::vector<char> data(payload.begin(), payload.end());
    encodeFrame(Ping, data, true, role_ == CLIENT);
}

void WsEncoder::encodePongFrame(const std::vector<char>& payload) {
    encodeFrame(Pong, payload, true, role_ == CLIENT);
}

void WsEncoder::encodePongFrame(const std::string& payload) {
    std::vector<char> data(payload.begin(), payload.end());
    encodeFrame(Pong, data, true, role_ == CLIENT);
}

void WsEncoder::encodeCloseFrame(uint16_t statusCode, const std::string& reason) {
    std::vector<char> payload;

    // Add 2-byte status code (network byte order)
    payload.push_back(static_cast<char>((statusCode >> 8) & 0xFF));
    payload.push_back(static_cast<char>(statusCode & 0xFF));

    // Add reason string
    payload.insert(payload.end(), reason.begin(), reason.end());

    encodeFrame(Close, payload, true, role_ == CLIENT);
}

void WsEncoder::encodeFrame(OpCode opcode,
                            const std::vector<char>& payload,
                            bool final,
                            bool mask) {
    buffer_.clear();  // Clear any previous frame data

    // First byte: FIN bit + RSV bits (000) + opcode
    uint8_t firstByte = static_cast<uint8_t>(opcode);
    if (final) {
        firstByte |= 0x80;  // Set FIN bit
    }
    buffer_.push_back(static_cast<char>(firstByte));

    // Second byte: MASK bit + payload length
    uint8_t secondByte = 0;
    if (mask) {
        secondByte |= 0x80;  // Set MASK bit
    }

    uint64_t payloadLength = payload.size();

    if (payloadLength < 126) {
        // 7-bit length
        secondByte |= static_cast<uint8_t>(payloadLength);
        buffer_.push_back(static_cast<char>(secondByte));
    } else if (payloadLength < 65536) {
        // 16-bit length
        secondByte |= 126;
        buffer_.push_back(static_cast<char>(secondByte));
        encodePayloadLength(payloadLength);
    } else {
        // 64-bit length
        secondByte |= 127;
        buffer_.push_back(static_cast<char>(secondByte));
        encodePayloadLength(payloadLength);
    }

    // Add masking key if needed (for client-side)
    uint8_t maskKey[4] = {0};
    if (mask) {
        // Generate 32-bit mask
        IRandom* rng = random_ ? random_ : &defaultRandom_;
        uint32_t mask = rng->generateRandom();

        // Extract bytes from 32-bit mask
        maskKey[0] = static_cast<uint8_t>(mask & 0xFF);
        maskKey[1] = static_cast<uint8_t>((mask >> 8) & 0xFF);
        maskKey[2] = static_cast<uint8_t>((mask >> 16) & 0xFF);
        maskKey[3] = static_cast<uint8_t>((mask >> 24) & 0xFF);

        for (int i = 0; i < 4; ++i) {
            buffer_.push_back(static_cast<char>(maskKey[i]));
        }
    }

    // Add payload (apply mask if needed)
    std::vector<char> maskedPayload = payload;
    if (mask && !payload.empty()) {
        applyMask(maskedPayload, maskKey);
    }

    buffer_.insert(buffer_.end(), maskedPayload.begin(), maskedPayload.end());
}

void WsEncoder::encodePayloadLength(uint64_t length) {
    if (length < 65536) {
        // 16-bit length
        buffer_.push_back(static_cast<char>((length >> 8) & 0xFF));
        buffer_.push_back(static_cast<char>(length & 0xFF));
    } else {
        // 64-bit length
        for (int i = 7; i >= 0; --i) {
            buffer_.push_back(static_cast<char>((length >> (i * 8)) & 0xFF));
        }
    }
}

void WsEncoder::applyMask(std::vector<char>& payload, const uint8_t mask[4]) {
    for (size_t i = 0; i < payload.size(); ++i) {
        payload[i] ^= mask[i % 4];
    }
}

}  // namespace beauty