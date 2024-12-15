#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <map>
#include <random>
#include <chrono>
#include <atomic>
#include <fstream>
#include <sstream>  // Added for std::stringstream

// Product class representing each product in the inventory
class Product {
public:
    int id;
    std::string name;
    double unit_price;
    std::atomic<int> quantity; // Use atomic for thread-safe operations
    std::mutex mtx; // Mutex to protect this product's quantity
    std::ofstream product_file; // File to log sales per product

    Product(int id, const std::string& name, double price, int qty)
        : id(id), name(name), unit_price(price), quantity(qty) {
        // Open the file for each product
        std::string filename = "product_" + std::to_string(id) + ".txt";
        product_file.open(filename, std::ios::out);
        if (!product_file.is_open()) {
            std::cerr << "Failed to open file for product: " << name << "\n";
            exit(1);
        }
    }

    ~Product() {
        if (product_file.is_open()) {
            product_file.close();
        }
    }

    // Log sale information to the product file
    void logSale(int qty_sold, double total_price) {
        std::lock_guard<std::mutex> file_lock(mtx); // Lock file and product quantity
        product_file << "Quantity sold: " << qty_sold << ", Remaining: " << quantity
                     << ", Total sale: $" << total_price << "\n";
    }
};

// Bill class representing a sales transaction
class Bill {
public:
    int bill_id;
    std::map<int, int> items_sold; // product_id -> quantity sold
    double total_price;

    Bill(int id) : bill_id(id), total_price(0.0) {}
};

// Inventory class managing products, total money, and bills
class Inventory {
public:
    std::map<int, Product*> products; // product_id -> Product*
    std::mutex money_mtx; // Mutex to protect total_money
    double total_money;

    std::mutex bills_mtx; // Mutex to protect bills vector
    std::vector<Bill> bills;

    // Mutex for writing to the results file
    std::mutex file_mtx;
    std::ofstream result_file;

    Inventory() : total_money(0.0) {
        // Open the results file
        result_file.open("results.txt", std::ios::out);
        if (!result_file.is_open()) {
            std::cerr << "Failed to open results file.\n";
            exit(1);
        }
    }

    ~Inventory() {
        for (auto& pair : products) {
            delete pair.second;
        }
        if (result_file.is_open()) {
            result_file.close();
        }
    }

    void addProduct(int id, const std::string& name, double price, int qty) {
        products[id] = new Product(id, name, price, qty);
    }

    // Function to perform a sale
    void performSale(int thread_id, int bill_id) {
        // Randomly select number of items to purchase
        int num_items = rand() % 5 + 1; // 1 to 5 items

        Bill bill(bill_id);

        for (int i = 0; i < num_items; ++i) {
            // Randomly select a product
            int product_id = rand() % products.size();
            Product* product = products[product_id];

            // Randomly select quantity to purchase
            int qty_to_buy = rand() % 3 + 1; // 1 to 3 units

            // Acquire lock on the product's mutex
            std::unique_lock<std::mutex> lock(product->mtx);

            if (product->quantity >= qty_to_buy) {
                product->quantity -= qty_to_buy;
                lock.unlock(); // Release the lock immediately after updating quantity

                // Update bill
                bill.items_sold[product_id] += qty_to_buy;
                double price = qty_to_buy * product->unit_price;
                bill.total_price += price;

                // Log the sale to the product's file
                product->logSale(qty_to_buy, price);
            } else {
                lock.unlock();
                // Insufficient stock; skip this item
                continue;
            }
        }

        if (bill.total_price > 0) {
            // Update total money
            std::lock_guard<std::mutex> money_lock(money_mtx);
            total_money += bill.total_price;

            // Add bill to the list
            std::lock_guard<std::mutex> bills_lock(bills_mtx);
            bills.push_back(bill);

            // Optionally, write the bill details to the main results file
            std::lock_guard<std::mutex> file_lock(file_mtx);
            result_file << "Thread " << thread_id << ", Bill ID: " << bill.bill_id << "\n";
            result_file << "Items Sold:\n";
            for (const auto& item : bill.items_sold) {
                Product* product = products[item.first];
                result_file << "  " << product->name << " (ID: " << product->id << ") - Quantity: " << item.second << ", Unit Price: " << product->unit_price << "\n";
            }
            result_file << "Total Price: " << bill.total_price << "\n\n";
        }
    }

    // Function to perform inventory check with performance logging
    void inventoryCheck() {
        // Start performance timer
        auto start = std::chrono::high_resolution_clock::now();

        // Prepare a string to hold the inventory check results
        std::stringstream ss;

        ss << "\nPerforming inventory check...\n";

        // Calculate total quantities sold from bills
        std::map<int, int> total_sold;
        double calculated_total_money = 0.0;

        // Lock bills for reading
        bills_mtx.lock();
        for (const auto& bill : bills) {
            for (const auto& item : bill.items_sold) {
                total_sold[item.first] += item.second;
                calculated_total_money += item.second * products[item.first]->unit_price;
            }
        }
        bills_mtx.unlock();

        // Verify total money
        money_mtx.lock();
        if (calculated_total_money != total_money) {
            ss << "Mismatch in total money! Calculated: " << calculated_total_money
               << ", Recorded: " << total_money << "\n";
        } else {
            ss << "Total money matches: " << total_money << "\n";
        }
        money_mtx.unlock();

        // Verify product quantities
        bool quantities_match = true;
        for (const auto& pair : products) {
            Product* product = pair.second;
            int initial_qty = 100; // Assuming initial quantity is 100 for all products
            int expected_qty = initial_qty - total_sold[product->id];
            if (product->quantity != expected_qty) {
                ss << "Mismatch in quantity for product " << product->name
                   << "! Expected: " << expected_qty
                   << ", Actual: " << product->quantity.load() << "\n";
                quantities_match = false;
            }
        }

        if (quantities_match) {
            ss << "All product quantities match.\n";
        }

        // Stop performance timer and calculate duration
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        ss << "Inventory check completed in " << duration.count() << " seconds.\n";

        // Write the inventory check results to the file and console
        std::lock_guard<std::mutex> file_lock(file_mtx);
        result_file << ss.str();
        std::cout << ss.str(); // Also print to the console
    }
};

// Worker function for each sales thread
void salesThread(Inventory& inventory, int thread_id, int num_sales) {
    for (int i = 0; i < num_sales; ++i) {
        inventory.performSale(thread_id, thread_id * 1000000 + i);
    }
}

int main() {
    // Seed the random number generator
    srand(static_cast<unsigned int>(time(0)));

    Inventory inventory;

    // Initialize products
    int num_products = 10;
    for (int i = 0; i < num_products; ++i) {
        inventory.addProduct(i, "Product_" + std::to_string(i), (i + 1) * 10.0, 100);
    }

    // Number of sales threads
    int num_threads = 5;
    int sales_per_thread = 1000;

    // Measure the total execution time for all sales
    auto overall_start = std::chrono::high_resolution_clock::now();

    // Create sales threads
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(salesThread, std::ref(inventory), i, sales_per_thread);
    }

    // Perform inventory checks periodically
    std::thread inventory_checker([&inventory]() {
        for (int i = 0; i < 5; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            inventory.inventoryCheck();
        }
    });

    // Wait for all sales threads to finish
    for (auto& t : threads) {
        t.join();
    }

    // Wait for inventory checker to finish
    inventory_checker.join();

    // Final inventory check
    inventory.inventoryCheck();

    // Measure the overall time and print
    auto overall_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> total_duration = overall_end - overall_start;
    std::cout << "Total execution time for sales: " << total_duration.count() << " seconds.\n";
    inventory.result_file << "Total execution time for sales: " << total_duration.count() << " seconds.\n";

    return 0;
}
