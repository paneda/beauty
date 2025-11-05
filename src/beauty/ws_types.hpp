#pragma once

#include <functional>
#include <system_error>

namespace beauty {

// Result of a WebSocket write operation
enum class WriteResult {
    SUCCESS,            ///< Write operation started successfully
    WRITE_IN_PROGRESS,  ///< Another write is already in progress
    CONNECTION_CLOSED   ///< Connection is closed or not found
};

// Callback for write completion notification
// params:
// error_code: Error code (empty on success)
// bytes_written: Number of bytes written
using WriteCompleteCallback = std::function<void(const std::error_code&, std::size_t)>;

}  // namespace beauty