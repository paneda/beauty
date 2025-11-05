#pragma once

#include <chrono>
#include <unordered_map>
#include <random>
#include <queue>
#include "beauty/ws_endpoint.hpp"

// Example data streaming endpoint with advanced flow control
//
// This endpoint demonstrates multiple flow control strategies:
// 1. Drop-on-busy: Messages are dropped when connection is busy (default)
// 2. Queue-based: Messages are queued and processed when connection becomes available
//
// Users can switch between strategies in real-time to compare behavior.
class MyDataStreamingEndpoint : public beauty::WsEndpoint {
   private:
    // Flow control strategy enumeration
    enum class FlowControlMode {
        DROP_ON_BUSY,  // Drop messages when connection is busy
        QUEUE_BASED    // Queue messages and process via callbacks
    };

    // Message queue for callback-driven processing
    struct MessageQueue {
        std::queue<std::string> messages;
        size_t maxQueueSize = 100;  // Prevent unbounded growth
        size_t queueOverflows = 0;  // Track when queue gets full
    };

    // Enhanced connection statistics for monitoring
    struct ConnectionStats {
        size_t messagesSent = 0;
        size_t messagesDropped = 0;
        size_t queueOverflows = 0;
        std::chrono::steady_clock::time_point lastSendTime;
        FlowControlMode mode = FlowControlMode::DROP_ON_BUSY;

        double getDropRate() const {
            size_t total = messagesSent + messagesDropped;
            return total > 0 ? (double)messagesDropped / total : 0.0;
        }

        size_t getCurrentQueueDepth() const {
            // Will be set separately for each connection
            return 0;  // Placeholder, updated in logic
        }
    };

    std::unordered_map<std::string, ConnectionStats> connectionStats_;
    std::unordered_map<std::string, MessageQueue> messageQueues_;
    std::mt19937 rng_{std::random_device{}()};
    std::uniform_real_distribution<> dist_{0.0, 100.0};

   public:
    MyDataStreamingEndpoint() : WsEndpoint("/ws/data") {}

    void onWsOpen(const std::string& connectionId) override {
        connectionStats_[connectionId] = ConnectionStats{};
        messageQueues_[connectionId] = MessageQueue{};
        sendText(connectionId,
                 "Welcome to advanced data streaming! Try commands: 'help', 'stats', 'drop_mode', "
                 "'queue_mode'");

        // Send initial stats
        sendStats(connectionId);
    }

    void onWsMessage(const std::string& connectionId, const beauty::WsMessage& wsMessage) override {
        // Convert vector<char> to string for comparison
        std::string message(wsMessage.content_.begin(), wsMessage.content_.end());

        if (message == "stats") {
            sendStats(connectionId);
        } else if (message == "reset") {
            resetStats(connectionId);
        } else if (message == "burst") {
            sendBurstData(connectionId);
        } else if (message == "drop_mode") {
            setFlowControlMode(connectionId, FlowControlMode::DROP_ON_BUSY);
        } else if (message == "queue_mode") {
            setFlowControlMode(connectionId, FlowControlMode::QUEUE_BASED);
        } else if (message == "help") {
            sendHelp(connectionId);
        } else {
            sendHelp(connectionId);
        }
    }

    void onWsClose(const std::string& connectionId) override {
        connectionStats_.erase(connectionId);
        messageQueues_.erase(connectionId);
    }

    void onWsError(const std::string& connectionId, const std::string& /*error*/) override {
        // Clean up on error
        connectionStats_.erase(connectionId);
        messageQueues_.erase(connectionId);
    }

    // Send data to all connections using their configured flow control mode
    //
    // This method demonstrates two strategies:
    // 1. Drop-on-busy: Messages are dropped when connection is busy
    // 2. Queue-based: Messages are queued and processed via callbacks
    // Process queues only (called frequently for responsive queue draining)
    void processQueues() {
        auto connections = getActiveConnections();
        for (const auto& connId : connections) {
            auto& stats = connectionStats_[connId];
            if (stats.mode == FlowControlMode::QUEUE_BASED) {
                processQueuedMessages(connId);
            }
        }
    }

    void broadcastData() {
        auto connections = getActiveConnections();

        // First, always try to process queued messages for queue-based connections
        processQueues();

        std::string data = generateRandomData();

        for (const auto& connId : connections) {
            auto& stats = connectionStats_[connId];

            if (stats.mode == FlowControlMode::DROP_ON_BUSY) {
                sendWithDropOnBusy(connId, data);
            } else {
                sendWithQueueing(connId, data);
            }
        }
    }

   private:
    // Drop-on-busy flow control strategy
    void sendWithDropOnBusy(const std::string& connId, const std::string& data) {
        auto& stats = connectionStats_[connId];

        if (canSendTo(connId)) {
            auto result = sendText(connId, data);

            if (result == beauty::WriteResult::SUCCESS) {
                stats.messagesSent++;
                stats.lastSendTime = std::chrono::steady_clock::now();
            } else if (result == beauty::WriteResult::WRITE_IN_PROGRESS) {
                stats.messagesDropped++;
            }
            // CONNECTION_CLOSED will be handled by onWsClose
        } else {
            stats.messagesDropped++;
        }
    }

    // Simple queue-based flow control strategy
    void sendWithQueueing(const std::string& connId, const std::string& data) {
        auto& stats = connectionStats_[connId];
        auto& queue = messageQueues_[connId];

        // First try to process any existing queued messages
        processQueuedMessages(connId);

        // Try to send directly if no queue backlog
        if (queue.messages.empty() && canSendTo(connId)) {
            auto result = sendText(connId, data);
            if (result == beauty::WriteResult::SUCCESS) {
                stats.messagesSent++;
                stats.lastSendTime = std::chrono::steady_clock::now();
                return;
            }
        }

        // Connection is busy or has backlog, add to queue
        if (queue.messages.size() < queue.maxQueueSize) {
            queue.messages.push(data);
        } else {
            // Queue overflow - drop the message
            stats.messagesDropped++;
            queue.queueOverflows++;
            stats.queueOverflows++;
        }
    }

    // Simple queue processing - try to send queued messages for a connection
    void processQueuedMessages(const std::string& connId) {
        if (connectionStats_.find(connId) == connectionStats_.end()) {
            return;  // Connection was closed
        }

        auto& stats = connectionStats_[connId];
        auto& queue = messageQueues_[connId];

        // Try to process messages, but don't get stuck on busy connections
        int processedCount = 0;
        const int maxProcessPerCall = 20;  // Reasonable limit to avoid blocking

        while (!queue.messages.empty() && processedCount < maxProcessPerCall) {
            std::string message = queue.messages.front();

            auto result = sendText(connId, message);

            if (result == beauty::WriteResult::SUCCESS) {
                // Only remove message from queue if send was successful
                queue.messages.pop();
                stats.messagesSent++;
                stats.lastSendTime = std::chrono::steady_clock::now();
                processedCount++;
                // Continue immediately to next message
            } else if (result == beauty::WriteResult::WRITE_IN_PROGRESS) {
                // Connection is busy, leave message in queue and stop for now
                // The next call to processQueuedMessages will try again
                break;
            } else {
                // CONNECTION_CLOSED or other error - remove and drop message
                queue.messages.pop();
                stats.messagesDropped++;
                processedCount++;
                break;
            }
        }
    }

    // Set flow control mode for a connection
    void setFlowControlMode(const std::string& connId, FlowControlMode mode) {
        if (connectionStats_.find(connId) != connectionStats_.end()) {
            auto& stats = connectionStats_[connId];
            auto& queue = messageQueues_[connId];

            // Auto-reset stats when switching mode
            stats = ConnectionStats{};
            stats.mode = mode;

            // Clear queue for clean slate
            while (!queue.messages.empty()) {
                queue.messages.pop();
            }
            queue.queueOverflows = 0;

            const char* modeStr =
                (mode == FlowControlMode::DROP_ON_BUSY) ? "drop-on-busy" : "queue-based";
            sendText(connId,
                     std::string("Flow control mode set to: ") + modeStr + " (stats reset)");
        }
    }

    // Reset connection statistics and queue
    void resetStats(const std::string& connId) {
        if (connectionStats_.find(connId) != connectionStats_.end()) {
            auto mode = connectionStats_[connId].mode;  // Preserve mode
            connectionStats_[connId] = ConnectionStats{};
            connectionStats_[connId].mode = mode;

            auto& queue = messageQueues_[connId];
            while (!queue.messages.empty()) {
                queue.messages.pop();
            }
            queue.queueOverflows = 0;

            sendText(connId, "Statistics and queue reset");
        }
    }

    // Send help message with available commands
    void sendHelp(const std::string& connId) {
        std::string help =
            "=== WebSocket Flow Control Demo ===\n"
            "This demo shows how to handle BURSTY DATA PRODUCTION scenarios.\n"
            "Normal rate: 1 message/second. Burst rate: 50 messages instantly.\n\n"
            "Commands:\n"
            "stats - Show connection statistics\n"
            "reset - Reset statistics and queue\n"
            "burst - Send 50 messages instantly (tests current mode)\n"
            "drop_mode - Handle bursts by DROPPING excess messages\n"
            "queue_mode - Handle bursts by QUEUING messages for later\n"
            "help - Show this help\n\n"
            "Try: 1) Set mode, 2) Send burst, 3) Check stats to compare!";
        sendText(connId, help);
    }

    std::string generateRandomData() {
        double value = dist_(rng_);
        auto now = std::chrono::steady_clock::now();
        auto timestamp =
            std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        return "DATA:" + std::to_string(timestamp) + ":" + std::to_string(value);
    }

    void sendBurstData(const std::string& connectionId) {
        // Send 50 messages as fast as possible to trigger flow control
        // Behavior depends on current flow control mode
        auto& stats = connectionStats_[connectionId];

        sendText(connectionId, "Starting burst mode (50 messages)...");

        for (int i = 0; i < 50; i++) {
            std::string data = "BURST:" + std::to_string(i) + ":" + generateRandomData();

            if (stats.mode == FlowControlMode::DROP_ON_BUSY) {
                sendWithDropOnBusy(connectionId, data);
            } else {
                sendWithQueueing(connectionId, data);
            }
        }

        // Final queue processing attempt for queue mode
        if (stats.mode == FlowControlMode::QUEUE_BASED) {
            processQueuedMessages(connectionId);
        }

        sendText(connectionId, "Burst complete. Check stats to see results.");
    }

    void sendStats(const std::string& connectionId) {
        if (connectionStats_.find(connectionId) == connectionStats_.end()) {
            return;
        }

        const auto& stats = connectionStats_[connectionId];
        auto& queue = messageQueues_.at(connectionId);

        // Process any queued messages when getting stats
        if (stats.mode == FlowControlMode::QUEUE_BASED) {
            processQueuedMessages(connectionId);
        }

        const char* modeStr =
            (stats.mode == FlowControlMode::DROP_ON_BUSY) ? "drop-on-busy" : "queue-based";

        std::string statsMsg = "STATS:mode=" + std::string(modeStr) +
                               ",sent=" + std::to_string(stats.messagesSent) +
                               ",dropped=" + std::to_string(stats.messagesDropped) +
                               ",drop_rate=" + std::to_string(stats.getDropRate() * 100) + "%" +
                               ",queue_depth=" + std::to_string(queue.messages.size()) +
                               ",queue_overflows=" + std::to_string(stats.queueOverflows);

        // For queue mode, add stats message directly to front of queue to ensure delivery
        if (stats.mode == FlowControlMode::QUEUE_BASED) {
            std::queue<std::string> tempQueue;
            tempQueue.push(statsMsg);
            while (!queue.messages.empty()) {
                tempQueue.push(queue.messages.front());
                queue.messages.pop();
            }
            queue.messages = tempQueue;
        } else {
            sendText(connectionId, statsMsg);
        }
    }
};