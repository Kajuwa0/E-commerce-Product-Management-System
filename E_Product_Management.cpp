// ecommerce_system.cpp
// Compile: g++ -std=c++17 ecommerce_system.cpp -o ecommerce_system

#include <bits/stdc++.h>
using namespace std;
using uid_t = unsigned long long;

// -------------------------
// Interface: IDiscount
// -------------------------
struct IDiscount {
    virtual double applyDiscount(double price) const = 0;
    virtual ~IDiscount() = default;
};

// -------------------------
// Product base class
// -------------------------
class Product {
protected:
    uid_t id;
    string name;
    double price;
    string sku;
public:
    Product(uid_t id, string name, double price, string sku) : id(id), name(move(name)), price(price), sku(move(sku)) {}

    virtual ~Product() = default;

    uid_t getId() const { return id; }
    const string& getName() const { return name; }
    double getBasePrice() const { return price; }
    const string& getSku() const { return sku; }

    // virtual hook for final price (after product-level rules)
    virtual double finalPrice() const { return price; }

    virtual string getType() const { return "Product"; }

    virtual string toString() const {
        ostringstream oss;
        oss << "[" << getType() << "] " << name << " (SKU:" << sku << ") : " << fixed << setprecision(2) << finalPrice();
        return oss.str();
    }

    // allow printing with <<
    friend ostream& operator<<(ostream& os, const Product& p) {
        os << p.toString();
        return os;
    }
};

// -------------------------
// Specialized products
// -------------------------
class Electronics : public Product, public IDiscount {
    int warranty_months;
public:
    Electronics(uid_t id, string name, double price, string sku, int warranty_months) : Product(id, move(name), price, move(sku)), warranty_months(warranty_months) {}

    string getType() const override { return "Electronics"; }

    // Electronics get a flat promotional 10% discount
    double applyDiscount(double price) const override {
        return price * 0.90;
    }

    double finalPrice() const override {
        return applyDiscount(price);
    }
};

class Clothing : public Product, public IDiscount {
    string size;
    bool on_clearance;
public:
    Clothing(uid_t id, string name, double price, string sku, string size, bool clearance=false) : Product(id, move(name), price, move(sku)), size(move(size)), on_clearance(clearance) {}

    string getType() const override { return "Clothing"; }

    // Clothing clearance: 30% off; otherwise 5% off
    double applyDiscount(double price) const override {
        if (on_clearance) return price * 0.70;
        return price * 0.95;
    }

    double finalPrice() const override {
        return applyDiscount(price);
    }

    string toString() const override {
        ostringstream oss;
        oss << "[" << getType() << "] " << name << " (Size:" << size
            << ", SKU:" << sku << ") : " << fixed << setprecision(2) << finalPrice();
        return oss.str();
    }
};

class Grocery : public Product {
    string expiry_date;
public:
    Grocery(uid_t id, string name, double price, string sku, string expiry) : Product(id, move(name), price, move(sku)), expiry_date(move(expiry)) {}

    string getType() const override { return "Grocery"; }

    string toString() const override {
        ostringstream oss;
        oss << "[" << getType() << "] " << name << " (exp:" << expiry_date << ", SKU:" << sku << ") : " << fixed << setprecision(2) << finalPrice();
        return oss.str();
    }
};

// -------------------------
// Template: GenericCatalog<T>
// -------------------------
template<typename T>
class GenericCatalog {
    vector<shared_ptr<T>> items;
public:
    void add(shared_ptr<T> item) { items.push_back(move(item)); }
    const vector<shared_ptr<T>>& getItems() const { return items; }
    size_t size() const { return items.size(); }
};

// -------------------------
// ShoppingCart
// -------------------------
class ShoppingCart {
    // map product id -> pair(product_ptr, qty)
    unordered_map<uid_t, pair<shared_ptr<Product>, size_t>> items;
public:
    ShoppingCart() = default;

    void addProduct(shared_ptr<Product> p, size_t qty = 1) {
        if (!p || qty == 0) return;
        auto it = items.find(p->getId());
        if (it == items.end()) items.emplace(p->getId(), make_pair(p, qty));
        else it->second.second += qty;
    }

    void removeProduct(uid_t id, size_t qty = 1) {
        auto it = items.find(id);
        if (it == items.end()) return;
        if (qty >= it->second.second) items.erase(it);
        else it->second.second -= qty;
    }

    ShoppingCart& operator+=(shared_ptr<Product> p) {
        addProduct(move(p), 1);
        return *this;
    }

    friend ShoppingCart operator+(ShoppingCart cart, shared_ptr<Product> p) {
        cart.addProduct(move(p), 1);
        return cart;
    }

    double total() const {
        double sum = 0.0;
        for (const auto &kv : items) {
            const auto &p = kv.second.first;
            size_t qty = kv.second.second;
            if (auto disc = dynamic_cast<const IDiscount*>(p.get())) {
                sum += disc->applyDiscount(p->getBasePrice()) * qty;
            } else {
                sum += p->getBasePrice() * qty;
            }
        }
        return sum;
    }

    bool empty() const { return items.empty(); }

    string toString() const {
        ostringstream oss;
        oss << "ShoppingCart:\n";
        for (const auto &kv : items) {
            const auto &p = kv.second.first;
            size_t qty = kv.second.second;
            oss << "  x" << qty << " " << p->toString() << "\n";
        }
        oss << "Total: " << fixed << setprecision(2) << total();
        return oss.str();
    }

    friend ostream& operator<<(ostream& os, const ShoppingCart& c) {
        os << c.toString();
        return os;
    }

    unordered_map<uid_t, pair<shared_ptr<Product>, size_t>> itemsSnapshot() const {
        return items;
    }

    void clear() { items.clear(); }
};

// -------------------------
// Order
// -------------------------
enum class OrderStatus { Created, Paid, Shipped, Cancelled };

class Order {
    static atomic<uid_t> nextOrderId;
    uid_t order_id;
    unordered_map<uid_t, pair<shared_ptr<Product>, size_t>> items;
    OrderStatus status;
    time_t created_at;
public:
    explicit Order(const ShoppingCart& cart) : order_id(++nextOrderId), items(cart.itemsSnapshot()), status(OrderStatus::Created), created_at(time(nullptr)){}

    uid_t getId() const { return order_id; }

    double total() const {
        double sum = 0.0;
        for (const auto &kv : items) {
            auto p = kv.second.first;
            size_t qty = kv.second.second;
            if (auto disc = dynamic_cast<const IDiscount*>(p.get())) {
                sum += disc->applyDiscount(p->getBasePrice()) * qty;
            } else {
                sum += p->getBasePrice() * qty;
            }
        }
        return sum;
    }

    void pay() { status = OrderStatus::Paid; }
    void ship() { status = OrderStatus::Shipped; }
    void cancel() { status = OrderStatus::Cancelled; }

    string statusString() const {
        switch (status) {
            case OrderStatus::Created: return "Created";
            case OrderStatus::Paid: return "Paid";
            case OrderStatus::Shipped: return "Shipped";
            case OrderStatus::Cancelled: return "Cancelled";
        }
        return "Unknown";
    }

    string toString() const {
        ostringstream oss;
        oss << "Order#" << order_id << " (" << statusString() << ")\n";
        for (const auto &kv : items) {
            auto p = kv.second.first;
            size_t qty = kv.second.second;
            oss << "  x" << qty << " " << p->toString() << "\n";
        }
        oss << "Order Total: " << fixed << setprecision(2) << total();
        return oss.str();
    }

    friend ostream& operator<<(ostream& os, const Order& o) {
        os << o.toString();
        return os;
    }
};

atomic<uid_t> Order::nextOrderId{0};

// -------------------------
// Demo / Tests (main)
// -------------------------
int main() {

    // --- 1. Creating objects ---
    auto e1 = make_shared<Electronics>(1, "Smartphone", 699.99, "ELEC-100", 12);
    auto c1 = make_shared<Clothing>(2, "Leather Jacket", 250.00, "CLOTH-200", "L", false);
    auto g1 = make_shared<Grocery>(3, "Organic Milk", 3.49, "GROC-300", "2025-12-01");

    cout << *e1 << "\n";
    cout << *c1 << "\n";
    cout << *g1 << "\n\n";

    // --- 2. Inheritance / Overridden methods ---
    cout << "Base price of Smartphone: " << e1->getBasePrice()
         << " | Final price (after discount): " << e1->finalPrice() << "\n";
    cout << "Base price of Jacket: " << c1->getBasePrice()
         << " | Final price (after discount): " << c1->finalPrice() << "\n\n";

    // --- 3. Operator overloading (+= and +) ---
    ShoppingCart cart;
    cart += e1;   // using operator+=
    cart = cart + c1; // using operator+
    cart += g1;
    cout << cart << "\n\n";

    // --- 4. Interface & polymorphism ---
    vector<shared_ptr<Product>> products = {e1, c1, g1};
    for (auto &p : products) {
        cout << p->getName() << " -> Final Price: ";
        if (auto d = dynamic_cast<IDiscount*>(p.get())) {
            cout << d->applyDiscount(p->getBasePrice());
        } else {
            cout << p->getBasePrice();
        }
        cout << "\n";
    }
    cout << "\n";

    // --- 5. Template Class (GenericCatalog) ---
    GenericCatalog<Product> catalog;
    catalog.add(e1);
    catalog.add(c1);
    catalog.add(g1);
    cout << "Catalog contains " << catalog.size() << " items:\n";
    for (auto &it : catalog.getItems()) {
        cout << "  " << *it << "\n";
    }
    cout << "\n";

    // --- 6. Operations (cart, order, errors) ---
    cout << "Cart total: " << cart.total() << "\n";

    // Create an order from cart
    Order order(cart);
    cout << "Order created:\n" << order << "\n";

    order.pay();
    cout << "After payment:\n" << order << "\n";

    // Remove items from cart
    cout << "Removing product ID=2 (Jacket) from cart...\n";
    cart.removeProduct(2, 1);
    cout << cart << "\n";

    // Try invalid operation
    cout << "Attempting to remove invalid product ID=999...\n";
    cart.removeProduct(999, 1); // should handle gracefully
    cout << "Cart still contains:\n" << cart << "\n";

    // Clear cart
    cart.clear();
    cout << "Cart cleared. Empty? " << boolalpha << cart.empty() << "\n";

    return 0;
}
