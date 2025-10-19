#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <list>
#include <unordered_map>
#include <functional>

struct Order {
    uint64_t order_id;     // Unique order identifier
    bool is_buy;           // true = buy, false = sell
    double price;          // Limit price
    uint64_t quantity;     // Remaining quantity
    uint64_t timestamp_ns; // Order entry timestamp in nanoseconds
};

struct PriceLevel {
    double price;
    uint64_t total_quantity;
};

class OrderBook {
public:
    OrderBook(size_t pool_size = 10000);
    ~OrderBook();

    // Insert a new order into the book
    void add_order(const Order& order);

    // Cancel an existing order by its ID
    bool cancel_order(uint64_t order_id);

    // Amend an existing order's price or quantity
    bool amend_order(uint64_t order_id, double new_price, uint64_t new_quantity);

    // Get a snapshot of top N bid and ask levels (aggregated quantities)
    void get_snapshot(size_t depth, std::vector<PriceLevel>& bids, std::vector<PriceLevel>& asks) const;

    // Print current state of the order book
    void print_book(size_t depth = 10) const;

private:
    // Memory pool for Order objects
    struct MemoryPool {
        char* memory;
        size_t capacity;
        std::vector<Order*> free_list;

        MemoryPool(size_t size);
        ~MemoryPool();
        Order* allocate();
        void deallocate(Order* order);
    };

    MemoryPool pool;

    // Bids: higher prices first (use greater comparator)
    std::map<double, std::list<Order*>, std::greater<double>> bids;

    // Asks: lower prices first (use default less comparator)
    std::map<double, std::list<Order*>> asks;

    // O(1) order lookup
    std::unordered_map<uint64_t, Order*> order_lookup;

    // Helper methods
    void try_match();
    void remove_order_from_book(Order* order);
    uint64_t get_timestamp_ns() const;
};
