#include "order_book.h"
#include <iostream>
#include <iomanip>

void print_separator(const std::string& title) {
    std::cout << "\n" << std::string(50, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(50, '=') << "\n";
}

int main() {
    OrderBook book;

    print_separator("TEST 1: Adding Multiple Buy and Sell Orders");

    // Add buy orders
    book.add_order({1, true, 100.50, 100, 0});
    book.add_order({2, true, 100.25, 150, 0});
    book.add_order({3, true, 100.50, 50, 0});  // Same price as order 1
    book.add_order({4, true, 99.75, 200, 0});

    // Add sell orders
    book.add_order({5, false, 101.00, 100, 0});
    book.add_order({6, false, 101.25, 150, 0});
    book.add_order({7, false, 101.00, 75, 0}); // Same price as order 5
    book.add_order({8, false, 102.00, 200, 0});

    book.print_book(5);

    print_separator("TEST 2: Cancel Order");
    std::cout << "Cancelling order #5 (Sell @ 101.00, qty 100)\n";

    if (book.cancel_order(5)) {
        std::cout << "Order #5 cancelled successfully\n";
    } else {
        std::cout << "Failed to cancel order #5\n";
    }

    book.print_book(5);

    print_separator("TEST 3: Amend Order - Quantity Only");
    std::cout << "Amending order #3 (Buy @ 100.50): changing quantity from 50 to 200\n";

    if (book.amend_order(3, 100.50, 200)) {
        std::cout << "Order #3 amended successfully\n";
    } else {
        std::cout << "Failed to amend order #3\n";
    }

    book.print_book(5);

    print_separator("TEST 4: Amend Order - Price Change");
    std::cout << "Amending order #2 (Buy @ 100.25): changing price to 100.75, qty to 100\n";

    if (book.amend_order(2, 100.75, 100)) {
        std::cout << "Order #2 amended successfully\n";
    } else {
        std::cout << "Failed to amend order #2\n";
    }

    book.print_book(5);

    print_separator("TEST 5: Add Orders That Trigger Matching");
    std::cout << "Adding aggressive buy order @ 101.50 (will cross the spread)\n";

    book.add_order({9, true, 101.50, 80, 0});

    std::cout << "\nBook after matching:\n";
    book.print_book(5);

    print_separator("TEST 6: More Matching - Full Order Fill");
    std::cout << "Adding aggressive sell order @ 99.00 (will match all bids)\n";

    book.add_order({10, false, 99.00, 500, 0});

    std::cout << "\nBook after matching:\n";
    book.print_book(5);

    print_separator("TEST 7: Get Snapshot (Top 3 Levels)");

    // Add some more orders for snapshot test
    book.add_order({11, true, 98.00, 100, 0});
    book.add_order({12, true, 97.50, 150, 0});
    book.add_order({13, true, 97.00, 200, 0});
    book.add_order({14, false, 102.50, 100, 0});
    book.add_order({15, false, 103.00, 150, 0});

    std::vector<PriceLevel> bids, asks;
    book.get_snapshot(3, bids, asks);

    std::cout << "Top 3 Bid Levels:\n";
    for (const auto& level : bids) {
        std::cout << "  Price: " << std::fixed << std::setprecision(2) << level.price
                  << ", Qty: " << level.total_quantity << "\n";
    }

    std::cout << "\nTop 3 Ask Levels:\n";
    for (const auto& level : asks) {
        std::cout << "  Price: " << std::fixed << std::setprecision(2) << level.price
                  << ", Qty: " << level.total_quantity << "\n";
    }

    book.print_book(5);

    print_separator("TEST 8: Edge Cases");

    std::cout << "Test 8a: Cancel non-existent order\n";
    if (!book.cancel_order(9999)) {
        std::cout << "Correctly returned false for non-existent order\n";
    }

    std::cout << "\nTest 8b: Amend non-existent order\n";
    if (!book.amend_order(9999, 100.00, 100)) {
        std::cout << "Correctly returned false for non-existent order\n";
    }

    std::cout << "\nTest 8c: Add orders at same price (FIFO test)\n";
    book.add_order({20, true, 95.00, 100, 0});
    book.add_order({21, true, 95.00, 200, 0});
    book.add_order({22, true, 95.00, 300, 0});

    std::cout << "Added 3 buy orders @ 95.00 with quantities 100, 200, 300\n";
    std::cout << "Adding sell order @ 95.00 with qty 250 (should match first two orders in FIFO)\n";

    book.add_order({23, false, 95.00, 250, 0});

    book.print_book(5);

    print_separator("ALL TESTS COMPLETED");

    return 0;
}
