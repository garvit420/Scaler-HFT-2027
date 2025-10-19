#include "order_book.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <algorithm>

// ========== Memory Pool Implementation ==========

OrderBook::MemoryPool::MemoryPool(size_t size) : capacity(size) {
    memory = new char[sizeof(Order) * capacity];
    free_list.reserve(capacity);

    // Initialize free list with all available slots
    for (size_t i = 0; i < capacity; ++i) {
        Order* order_ptr = reinterpret_cast<Order*>(memory + i * sizeof(Order));
        free_list.push_back(order_ptr);
    }
}

OrderBook::MemoryPool::~MemoryPool() {
    delete[] memory;
}

Order* OrderBook::MemoryPool::allocate() {
    if (free_list.empty()) {
        throw std::runtime_error("Memory pool exhausted");
    }
    Order* order = free_list.back();
    free_list.pop_back();
    return order;
}

void OrderBook::MemoryPool::deallocate(Order* order) {
    free_list.push_back(order);
}

// ========== OrderBook Implementation ==========

OrderBook::OrderBook(size_t pool_size) : pool(pool_size) {}

OrderBook::~OrderBook() {
    // Clean up all orders
    for (auto& [order_id, order_ptr] : order_lookup) {
        pool.deallocate(order_ptr);
    }
}

uint64_t OrderBook::get_timestamp_ns() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

void OrderBook::add_order(const Order& order) {
    // Allocate order from memory pool
    Order* new_order = pool.allocate();
    *new_order = order;

    // Auto-generate timestamp if not provided
    if (new_order->timestamp_ns == 0) {
        new_order->timestamp_ns = get_timestamp_ns();
    }

    // Add to lookup
    order_lookup[new_order->order_id] = new_order;

    // Add to appropriate side
    if (new_order->is_buy) {
        bids[new_order->price].push_back(new_order);
    } else {
        asks[new_order->price].push_back(new_order);
    }

    // Try to match orders
    try_match();
}

bool OrderBook::cancel_order(uint64_t order_id) {
    auto it = order_lookup.find(order_id);
    if (it == order_lookup.end()) {
        return false;
    }

    Order* order = it->second;
    remove_order_from_book(order);

    // Return to memory pool
    pool.deallocate(order);
    order_lookup.erase(it);

    return true;
}

bool OrderBook::amend_order(uint64_t order_id, double new_price, uint64_t new_quantity) {
    auto it = order_lookup.find(order_id);
    if (it == order_lookup.end()) {
        return false;
    }

    Order* order = it->second;

    // If price changes, treat as cancel + add
    if (order->price != new_price) {
        Order amended_order = *order;
        amended_order.price = new_price;
        amended_order.quantity = new_quantity;
        amended_order.timestamp_ns = get_timestamp_ns(); // New timestamp for new price

        cancel_order(order_id);
        add_order(amended_order);
    } else {
        // Only quantity change - update in place
        order->quantity = new_quantity;
    }

    return true;
}

void OrderBook::remove_order_from_book(Order* order) {
    if (order->is_buy) {
        auto price_it = bids.find(order->price);
        if (price_it != bids.end()) {
            auto& order_list = price_it->second;
            order_list.remove(order);

            // Remove price level if empty
            if (order_list.empty()) {
                bids.erase(price_it);
            }
        }
    } else {
        auto price_it = asks.find(order->price);
        if (price_it != asks.end()) {
            auto& order_list = price_it->second;
            order_list.remove(order);

            // Remove price level if empty
            if (order_list.empty()) {
                asks.erase(price_it);
            }
        }
    }
}

void OrderBook::try_match() {
    while (!bids.empty() && !asks.empty()) {
        auto best_bid_level = bids.begin();
        auto best_ask_level = asks.begin();

        double best_bid_price = best_bid_level->first;
        double best_ask_price = best_ask_level->first;

        // Check if prices cross
        if (best_bid_price < best_ask_price) {
            break; // No match possible
        }

        auto& bid_orders = best_bid_level->second;
        auto& ask_orders = best_ask_level->second;

        if (bid_orders.empty() || ask_orders.empty()) {
            break;
        }

        Order* bid_order = bid_orders.front();
        Order* ask_order = ask_orders.front();

        // Match at the price of the order that arrived first (price-time priority)
        double match_price = (bid_order->timestamp_ns < ask_order->timestamp_ns)
                             ? bid_order->price : ask_order->price;

        uint64_t match_qty = std::min(bid_order->quantity, ask_order->quantity);

        std::cout << "[MATCH] " << match_qty << " @ " << std::fixed << std::setprecision(2)
                  << match_price << " (Buy Order #" << bid_order->order_id
                  << " <-> Sell Order #" << ask_order->order_id << ")\n";

        // Reduce quantities
        bid_order->quantity -= match_qty;
        ask_order->quantity -= match_qty;

        // Remove filled orders
        if (bid_order->quantity == 0) {
            bid_orders.pop_front();
            order_lookup.erase(bid_order->order_id);
            pool.deallocate(bid_order);
        }

        if (ask_order->quantity == 0) {
            ask_orders.pop_front();
            order_lookup.erase(ask_order->order_id);
            pool.deallocate(ask_order);
        }

        // Remove empty price levels
        if (bid_orders.empty()) {
            bids.erase(best_bid_level);
        }
        if (ask_orders.empty()) {
            asks.erase(best_ask_level);
        }
    }
}

void OrderBook::get_snapshot(size_t depth, std::vector<PriceLevel>& bids_out, std::vector<PriceLevel>& asks_out) const {
    bids_out.clear();
    asks_out.clear();

    // Get top N bid levels
    size_t count = 0;
    for (const auto& [price, orders] : bids) {
        if (count >= depth) break;

        uint64_t total_qty = 0;
        for (const Order* order : orders) {
            total_qty += order->quantity;
        }

        bids_out.push_back({price, total_qty});
        count++;
    }

    // Get top N ask levels
    count = 0;
    for (const auto& [price, orders] : asks) {
        if (count >= depth) break;

        uint64_t total_qty = 0;
        for (const Order* order : orders) {
            total_qty += order->quantity;
        }

        asks_out.push_back({price, total_qty});
        count++;
    }
}

void OrderBook::print_book(size_t depth) const {
    std::vector<PriceLevel> bids_snapshot, asks_snapshot;
    get_snapshot(depth, bids_snapshot, asks_snapshot);

    std::cout << "\n========== ORDER BOOK ==========\n";
    std::cout << std::setw(15) << "ASKS (Sell)" << "\n";
    std::cout << std::setw(10) << "Price" << std::setw(15) << "Quantity" << "\n";
    std::cout << "--------------------------------\n";

    // Print asks in reverse (highest to lowest for display)
    for (auto it = asks_snapshot.rbegin(); it != asks_snapshot.rend(); ++it) {
        std::cout << std::setw(10) << std::fixed << std::setprecision(2) << it->price
                  << std::setw(15) << it->total_quantity << "\n";
    }

    std::cout << "================================\n";

    // Print bids (highest to lowest)
    for (const auto& level : bids_snapshot) {
        std::cout << std::setw(10) << std::fixed << std::setprecision(2) << level.price
                  << std::setw(15) << level.total_quantity << "\n";
    }

    std::cout << "--------------------------------\n";
    std::cout << std::setw(15) << "BIDS (Buy)" << "\n";
    std::cout << "================================\n\n";
}
