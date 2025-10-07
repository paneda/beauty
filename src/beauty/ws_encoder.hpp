#pragma once

#include <vector>
#include <string>
#include <cstdint>

#include "beauty/i_random_interface.hpp"
#include "beauty/default_random.hpp"

namespace beauty {

class WsEncoder {
   public:
    enum OpCode { Continuation = 0, TextData = 1, BinData = 2, Close = 8, Ping = 9, Pong = 10 };

    enum Role {
        SERVER,  // Server->Client frames (no masking)
        CLIENT   // Client->Server frames (masking required)
    };

   public:
    // Server constructor - no masking, no random generator needed
    explicit WsEncoder(std::vector<char>& buffer);

    // Client constructor - masking required, random generator must be provided
    WsEncoder(std::vector<char>& buffer, IRandom& random);

    // Encode text message frame - result stored in buffer_
    void encodeTextFrame(const std::string& text, bool final = true);

    // Encode binary message frame - result stored in buffer_
    void encodeBinaryFrame(const std::vector<char>& data, bool final = true);

    // Encode ping frame (optionally with payload for latency measurement)
    void encodePingFrame(const std::string& payload = "");

    // Encode pong frame (echo the ping payload)
    void encodePongFrame(const std::vector<char>& payload);
    void encodePongFrame(const std::string& payload = "");

    // Encode close frame with status code and optional reason
    void encodeCloseFrame(uint16_t statusCode = 1000, const std::string& reason = "");

    // Get the encoded frame data (similar to HttpResult::content_)
    const std::vector<char>& getFrame() const {
        return buffer_;
    }

    // Get frame size for asio::buffer() calls
    size_t size() const {
        return buffer_.size();
    }

   private:
    std::vector<char>& buffer_;    // Reference to pre-allocated buffer
    Role role_;                    // Determines masking behavior
    IRandom* random_;              // Platform-specific random generator
    DefaultRandom defaultRandom_;  // Fallback random generator

    // Core encoding function
    void encodeFrame(OpCode opcode,
                     const std::vector<char>& payload,
                     bool final = true,
                     bool mask = false);

    // Helper to encode payload length (handles 7-bit, 16-bit, and 64-bit lengths)
    void encodePayloadLength(uint64_t length);

    // Helper to apply masking (for client-side encoding if needed)
    void applyMask(std::vector<char>& payload, const uint8_t mask[4]);
};

}  // namespace beauty