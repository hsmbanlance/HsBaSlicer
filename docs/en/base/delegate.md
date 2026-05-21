# Delegate

The Delegate component implements a type-safe delegate/event system that supports multicast delegates and thread-safe callback management.

## Features

- Type-safe delegate system
- Supports multicast delegates (multiple callback functions)
- Thread-safe callback management
- Supports arbitrary callable objects
- Supports both void and non-void return types

## Usage

### 1. Basic Usage

```cpp
#include "base/delegate.hpp"

// Create a delegate that returns void and accepts two int parameters
HsBa::Slicer::Utils::Delegate<void, int, int> my_delegate;

// Add callback functions
my_delegate += [](int a, int b) {
    std::cout << "Sum: " << a + b << std::endl;
};

my_delegate += [](int a, int b) {
    std::cout << "Product: " << a * b << std::endl;
};

// Call all callback functions
my_delegate(3, 4);  // Output: Sum: 7 and Product: 12
```

### 2. Return Value Handling

For delegates with non-void return types, handling depends on the return type:

```cpp
// For addable return types (such as int, double, etc.)
HsBa::Slicer::Utils::Delegate<int, int> add_delegate;

add_delegate += [](int x) { return x; };
add_delegate += [](int x) { return x * 2; };
add_delegate += [](int x) { return x * 3; };

int result = add_delegate(5);  // Result is 5 + 10 + 15 = 30

// For non-addable return types, the return value of the last callback will be returned
HsBa::Slicer::Utils::Delegate<std::string, std::string> string_delegate;

string_delegate += [](const std::string& s) { return "First: " + s; };
string_delegate += [](const std::string& s) { return "Second: " + s; };

std::string result2 = string_delegate("Hello");  // Result is "Second: Hello"
```

### 3. Event System

The Delegate component also provides Event and EventSource for implementing event-driven programming patterns:

```cpp
#include "base/delegate.hpp"

// Inherit from EventSource to create an event-supporting class
class Button : public HsBa::Slicer::Utils::EventSource<Button, void, int> {
public:
    void Click(int click_count) {
        // Trigger event
        RaiseEvent(click_count);
    }
};

// Using events
Button button;
button += [](int clicks) {
    std::cout << "Button clicked " << clicks << " times" << std::endl;
};

button.Click(3);  // Output: Button clicked 3 times
```

### 4. Complete Example

```cpp
#include "base/delegate.hpp"
#include <iostream>
#include <string>

int main() {
    // Create a delegate that returns int and accepts two double parameters
    HsBa::Slicer::Utils::Delegate<int, double, double> calc_delegate;
    
    // Add different callback functions
    calc_delegate.Add([](double a, double b) -> int {
        return static_cast<int>(a + b);
    });
    
    calc_delegate.Add([](double a, double b) -> int {
        return static_cast<int>(a * b);
    });
    
    // Call delegate (for addable return types, results will be accumulated)
    int result = calc_delegate(3.5, 2.0);  // 5 + 7 = 12
    std::cout << "Result: " << result << std::endl;
    
    // Using void delegate
    HsBa::Slicer::Utils::Delegate<void, std::string> log_delegate;
    
    log_delegate += [](const std::string& msg) {
        std::cout << "[INFO] " << msg << std::endl;
    };
    
    log_delegate += [](const std::string& msg) {
        std::cout << "[DEBUG] " << msg << std::endl;
    };
    
    log_delegate("Application started");
    
    // Check delegate status
    std::cout << "Delegate size: " << log_delegate.size() << std::endl;
    std::cout << "Is empty: " << log_delegate.empty() << std::endl;
    
    return 0;
}
```

## Notes

- The delegate system is thread-safe, but callback functions themselves need to ensure thread safety
- For addable return types, all callback results will be accumulated
- For non-addable return types, only the result of the last callback will be returned
- The callback removal feature has been deprecated because std::function does not have an operator== operator
- Delegates do not hold the lifecycle of captured objects in callback functions; ensure callback functions remain valid

### Further Information

#### Special Usage for Delegate Unregistration

In theory, if different types of non-anonymous functors are used for each registered delegate, the unregistration of delegates of the same type can be performed normally.

```cpp
class Button : public HsBa::Slicer::Utils::EventSource<Button, void, int> {
public:
    void Click(int click_count) {
        // Trigger event
        RaiseEvent(click_count);
    }
};

struct AClickCallback{
    void operator()(int click_count) {
        std::cout << "A: Click with " << click_count << "\n";
    }
};

struct BClickCallback{
    void operator()(int click_count) {
        std::cout << "B: Click with " << click_count << "\n";
    }
};

// Using events
Button button;
button += AClickCallback{};
button += BClickCallback{};

button.Click(3);  // Output: A: Click with 3\n B: Click with 3

button -= BClickCallback{};  // Unregister callbacks of type BClickCallback

button.Click(2);  // Output: A: Click with 2
```

#### Recommended Approach

In general, the above special method is not recommended, even though readability can be enhanced through schemes like NameType. The reason is that this approach is inconsistent with the usage of function pointers (or function names) and Lambda expressions. C++ implementations of delegate/event systems face difficulties in uniformly handling these three types of callable objects.

Since this library primarily targets non-UI scenarios such as slicing engines, typically using delegate schemes without unregistration functionality is sufficient to meet requirements.

#### Usage Scenario Information

The Delegate component is generally only used for simple event callbacks, such as selectively triggering outputs to terminals, UI, logging systems, etc. when triggered. In such cases, the effects of having extra or missing unregistrations are often not serious, but it is still not recommended. If you use the approach described in the further information section, please take responsibility yourself and clean up the corresponding compiler warnings.