# Any Object

The AnyObject component provides runtime type reflection and dynamic invocation capabilities, allowing inspection of object fields, method calls at runtime, and support for custom type information registration.

## Features

- Runtime type reflection
- Dynamic field access (optional)
- Dynamic method invocation
- Custom type information registration
- Support for virtual functions and bitfields
- Type-safe arbitrary type storage
- Support for move and copy semantics
- Support for non-member function registration
- Mockit-style test mocking support (enabled only when common test macros are detected; runtime toggle available)

## Usage

### 1. Basic Usage

```cpp
#include "base/any_object.hpp"
#include <iostream>

// Create an AnyObject storing an int value
HsBa::Slicer::Utils::AnyObject obj = 42;

// Get the stored value
int value = obj.cast<int>();
std::cout << "Value: " << value << std::endl;  // Output: Value: 42
```

### 2. Custom Type Information Registration

For custom types, you need to specialize the `GetTypeInfo` function to provide type information:

```cpp
#include "base/any_object.hpp"

struct Player
{
    int health;
    float speed;
    
    int AddHealth(int amount) {
        health += amount;
        return health;
    }
};

namespace HsBa::Slicer::Utils
{
    template<>
    TypeInfo* GetTypeInfo<Player>()
    {
        static TypeInfo info;
        info.Name = "Player";
        info.destroy = [](void* data) { delete static_cast<Player*>(data); };
        info.copy = [](const void* data) -> void* { 
            return new Player(*static_cast<const Player*>(data)); 
        };
        info.move = [](void* data) -> void* { 
            return new Player(std::move(*static_cast<Player*>(data))); 
        };
        
        // Register fields (optional)
        info.fields.clear();
        info.fields.emplace("health", std::make_pair(GetTypeInfo<int>(), offsetof(Player, health)));
        info.fields.emplace("speed", std::make_pair(GetTypeInfo<float>(), offsetof(Player, speed)));
        
        // Register methods
        info.methods.clear();
        info.methods.emplace("AddHealth", type_ensure<&Player::AddHealth>());
        
        return &info;
    }
}
```

**Notes**:
- **Field registration is optional**: You can choose not to register any fields (keep `fields` empty), or only register some fields that need reflection access
- **Minimal registration**: If you only need method invocation functionality, you can skip field registration and only register methods
- **Register on demand**: Selectively register fields that need dynamic access based on actual runtime requirements

### 3. Field Access

Use `ForeachField` to iterate over all fields of an object:

```cpp
#include "base/any_object.hpp"
#include <iostream>

Player player{100, 5.5f};
HsBa::Slicer::Utils::AnyObject obj(player);

// Iterate through all fields
obj.ForeachField([&](std::string_view name, AnyObject value) {
    std::cout << "Field: " << name;
    if (name == "health") {
        std::cout << " = " << value.cast<int>() << std::endl;
    } else if (name == "speed") {
        std::cout << " = " << value.cast<float>() << std::endl;
    }
});
```

### 3.1 Offset Calculation for Simple Types (Without offsetof)

For simple types without virtual functions and bitfields, you can use `alignof` and `sizeof` to directly calculate field offsets, avoiding non-standard extensions:

```cpp
#include "base/any_object.hpp"
#include <iostream>
#include <cstdint>

// Simple POD structure without virtual functions and bitfields
struct Point2D
{
    int32_t x;      // First field, offset is 0
    int32_t y;      // Second field, offset is sizeof(int32_t)
    double z;       // Third field, alignment needs to be considered
};

namespace HsBa::Slicer::Utils
{
    template<>
    TypeInfo* GetTypeInfo<Point2D>()
    {
        static TypeInfo info;
        info.Name = "Point2D";
        info.destroy = [](void* data) { delete static_cast<Point2D*>(data); };
        info.copy = [](const void* data) -> void* { 
            return new Point2D(*static_cast<const Point2D*>(data)); 
        };
        info.move = [](void* data) -> void* { 
            return new Point2D(std::move(*static_cast<Point2D*>(data))); 
        };
        
        // Use alignof and sizeof to calculate offsets, without using offsetof
        info.fields.clear();
        
        // x field: first field, offset is 0
        info.fields.emplace("x", std::make_pair(GetTypeInfo<int32_t>(), 0));
        
        // y field: after x, offset is sizeof(int32_t)
        info.fields.emplace("y", std::make_pair(GetTypeInfo<int32_t>(), sizeof(int32_t)));
        
        // z field: need to calculate total size of previous fields and consider alignment
        constexpr size_t first_two_fields_size = sizeof(int32_t) + sizeof(int32_t);
        constexpr size_t alignment_of_double = alignof(double);
        // Calculate aligned offset
        constexpr size_t offset_z = (first_two_fields_size + alignment_of_double - 1) / alignment_of_double * alignment_of_double;
        
        info.fields.emplace("z", std::make_pair(GetTypeInfo<double>(), offset_z));
        
        info.methods.clear();
        
        return &info;
    }
}

// Usage example
Point2D point{10, 20, 30.5};
HsBa::Slicer::Utils::AnyObject obj(point);

// Access fields
obj.ForeachField([&](std::string_view name, AnyObject value) {
    if (name == "x") {
        std::cout << "x = " << value.cast<int32_t>() << std::endl;  // Output: x = 10
    } else if (name == "y") {
        std::cout << "y = " << value.cast<int32_t>() << std::endl;  // Output: y = 20
    } else if (name == "z") {
        std::cout << "z = " << value.cast<double>() << std::endl;  // Output: z = 30.5
    }
});
```

**Note**: This method only applies to simple POD types (no virtual functions, no bitfields, no inheritance). For complex types, the `offsetof` macro is still required.

### 4. Method Invocation

Use the `Invoke` method to dynamically call object member functions:

```cpp
#include "base/any_object.hpp"
#include <iostream>

Player player{100, 5.5f};
HsBa::Slicer::Utils::AnyObject obj(player);

// Prepare arguments
AnyObject args[] = { AnyObject(20) };

// Call AddHealth method
AnyObject result = obj.Invoke("AddHealth", args);
std::cout << "New health: " << result.cast<int>() << std::endl;  // Output: New health: 120
```

### 4.1 Registering Non-Member Functions

In addition to member functions, you can also register non-member functions such as free functions and static member functions. This requires manually writing lambda wrappers:

```cpp
#include "base/any_object.hpp"
#include <iostream>

struct Math
{
    int value;
    Math(int v = 0) : value(v) {}
    
    // Member function
    int Add(int x) const { return value + x; }
};

// Free function
int Multiply(Math& self, int factor) {
    return self.value * factor;
}

// Static member function
struct Calculator
{
    static int Power(int base, int exp) {
        int result = 1;
        for (int i = 0; i < exp; ++i) result *= base;
        return result;
    }
};

namespace HsBa::Slicer::Utils
{
    template<>
    TypeInfo* GetTypeInfo<Math>()
    {
        static TypeInfo info;
        info.Name = "Math";
        info.destroy = [](void* data) { delete static_cast<Math*>(data); };
        info.copy = [](const void* data) -> void* { 
            return new Math(*static_cast<const Math*>(data)); 
        };
        info.move = [](void* data) -> void* { 
            return new Math(std::move(*static_cast<Math*>(data))); 
        };
        
        info.fields.clear();
        // It's okay not to register fields
        
        info.methods.clear();
        // Register member function (using type_ensure)
        info.methods.emplace("Add", type_ensure<&Math::Add>());
        
        // Register free function (manual lambda wrapper)
        info.methods.emplace("Multiply", +[](void* obj, std::span<AnyObject> args) -> AnyObject {
            auto self = static_cast<Math*>(obj);
            if (args.size() != 1 || !args[0].get_type_info() || args[0].get_type_info()->Name != "int")
            {
                throw RuntimeError("Math::Multiply expects one int argument");
            }
            int factor = args[0].cast<int>();
            return AnyObject{ Multiply(*self, factor) };
        });
        
        return &info;
    }
    
    template<>
    TypeInfo* GetTypeInfo<Calculator>()
    {
        static TypeInfo info;
        info.Name = "Calculator";
        info.destroy = [](void* data) { delete static_cast<Calculator*>(data); };
        info.copy = [](const void* data) -> void* { 
            return new Calculator(*static_cast<const Calculator*>(data)); 
        };
        info.move = [](void* data) -> void* { 
            return new Calculator(std::move(*static_cast<Calculator*>(data))); 
        };
        
        info.fields.clear();
        info.methods.clear();
        
        // Register static member function (no object instance needed, but need to pass an arbitrary object as placeholder)
        info.methods.emplace("Power", +[](void* obj, std::span<AnyObject> args) -> AnyObject {
            if (args.size() != 2 || 
                !args[0].get_type_info() || args[0].get_type_info()->Name != "int" ||
                !args[1].get_type_info() || args[1].get_type_info()->Name != "int")
            {
                throw RuntimeError("Calculator::Power expects two int arguments");
            }
            int base = args[0].cast<int>();
            int exp = args[1].cast<int>();
            return AnyObject{ Calculator::Power(base, exp) };
        });
        
        return &info;
    }
}

// Usage examples
Math math{10};
HsBa::Slicer::Utils::AnyObject obj(math);

// Call member function
AnyObject args1[] = { AnyObject(5) };
AnyObject result1 = obj.Invoke("Add", args1);
std::cout << "Add: " << result1.cast<int>() << std::endl;  // Output: Add: 15

// Call free function
AnyObject args2[] = { AnyObject(3) };
AnyObject result2 = obj.Invoke("Multiply", args2);
std::cout << "Multiply: " << result2.cast<int>() << std::endl;  // Output: Multiply: 30

// Call static member function
Calculator calc;
HsBa::Slicer::Utils::AnyObject obj_calc(calc);
AnyObject args3[] = { AnyObject(2), AnyObject(8) };
AnyObject result3 = obj_calc.Invoke("Power", args3);
std::cout << "Power: " << result3.cast<int>() << std::endl;  // Output: Power: 256
```

**Notes**:
- Member functions can use the `type_ensure` helper template for automatic wrapping
- Free functions and static member functions require manual lambda wrappers
- The lambda wrapper signature must be `AnyObject(void*, std::span<AnyObject>)`
- The first parameter is the object pointer (can be ignored for static functions), the second parameter is the argument list

### 5. Virtual Function Support

AnyObject supports classes with virtual functions:

```cpp
#include "base/any_object.hpp"
#include <iostream>

struct Base
{
    int value;
    Base(int v = 0) : value(v) {}
    virtual int GetValue() const { return value; }
    virtual ~BaseVirtual() = default;
};

struct Derived : Base
{
    Derived(int v = 0) : Base(v) {}
    int GetValue() const override { return value + 1; }
};

namespace HsBa::Slicer::Utils
{
    template<>
    TypeInfo* GetTypeInfo<Base>()
    {
        static TypeInfo info;
        info.Name = "Base";
        info.destroy = [](void* data) { delete static_cast<Base*>(data); };
        info.copy = [](const void* data) -> void* { 
            return new Base(*static_cast<const Base*>(data)); 
        };
        info.move = [](void* data) -> void* { 
            return new Base(std::move(*static_cast<Base*>(data))); 
        };
        info.fields.clear();
        info.fields.emplace("value", std::make_pair(GetTypeInfo<int>(), offsetof(Base, value)));
        info.methods.clear();
        info.methods.emplace("GetValue", type_ensure<&Base::GetValue>());
        return &info;
    }
    
    template<>
    TypeInfo* GetTypeInfo<Derived>()
    {
        static TypeInfo info;
        info.Name = "Derived";
        info.destroy = [](void* data) { delete static_cast<Derived*>(data); };
        info.copy = [](const void* data) -> void* { 
            return new Derived(*static_cast<const Derived*>(data)); 
        };
        info.move = [](void* data) -> void* { 
            return new Derived(std::move(*static_cast<Derived*>(data))); 
        };
        info.fields.clear();
        info.fields.emplace("value", std::make_pair(GetTypeInfo<int>(), offsetof(Derived, value)));
        info.methods.clear();
        info.methods.emplace("GetValue", type_ensure<&Derived::GetValue>());
        return &info;
    }
}

// Usage example
Derived derived{10};
HsBa::Slicer::Utils::AnyObject obj(derived);

AnyObject args[] = {};
AnyObject result = obj.Invoke("GetValue", args);
std::cout << "Value: " << result.cast<int>() << std::endl;  // Output: Value: 11 (calls derived class virtual function)
```

### 6. Bitfield Support

AnyObject supports classes containing bitfields:

```cpp
#include "base/any_object.hpp"
#include <iostream>

struct BitfieldClass
{
    int value;
    unsigned flags : 3;  // 3-bit bitfield
    
    BitfieldClass(int v = 0, unsigned f = 0) : value(v), flags(f) {}
    int Add(int x) const { return value + x; }
};

namespace HsBa::Slicer::Utils
{
    template<>
    TypeInfo* GetTypeInfo<BitfieldClass>()
    {
        static TypeInfo info;
        info.Name = "BitfieldClass";
        info.destroy = [](void* data) { delete static_cast<BitfieldClass*>(data); };
        info.copy = [](const void* data) -> void* { 
            return new BitfieldClass(*static_cast<const BitfieldClass*>(data)); 
        };
        info.move = [](void* data) -> void* { 
            return new BitfieldClass(std::move(*static_cast<BitfieldClass*>(data))); 
        };
        info.fields.clear();
        info.fields.emplace("value", std::make_pair(GetTypeInfo<int>(), offsetof(BitfieldClass, value)));
        info.methods.clear();
        info.methods.emplace("Add", type_ensure<&BitfieldClass::Add>());
        return &info;
    }
}

// Usage example
BitfieldClass obj{10, 5};
HsBa::Slicer::Utils::AnyObject any_obj(obj);

// Access regular fields (bitfield fields are not accessed)
any_obj.ForeachField([&](std::string_view name, AnyObject value) {
    if (name == "value") {
        std::cout << "Value: " << value.cast<int>() << std::endl;  // Output: Value: 10
    }
});

// Call method
AnyObject args[] = { AnyObject(2) };
AnyObject result = any_obj.Invoke("Add", args);
std::cout << "Result: " << result.cast<int>() << std::endl;  // Output: Result: 12
```

### 7. Complete Example

```cpp
#include "base/any_object.hpp"
#include <iostream>
#include <string>

// Define a simple class
struct Person
{
    std::string name;
    int age;
    
    std::string Greet(const std::string& greeting) const {
        return greeting + ", I'm " + name + " and I'm " + std::to_string(age) + " years old.";
    }
};

// Register type information
namespace HsBa::Slicer::Utils
{
    template<>
    TypeInfo* GetTypeInfo<Person>()
    {
        static TypeInfo info;
        info.Name = "Person";
        info.destroy = [](void* data) { delete static_cast<Person*>(data); };
        info.copy = [](const void* data) -> void* { 
            return new Person(*static_cast<const Person*>(data)); 
        };
        info.move = [](void* data) -> void* { 
            return new Person(std::move(*static_cast<Person*>(data))); 
        };
        
        info.fields.clear();
        info.fields.emplace("name", std::make_pair(GetTypeInfo<std::string>(), offsetof(Person, name)));
        info.fields.emplace("age", std::make_pair(GetTypeInfo<int>(), offsetof(Person, age)));
        
        info.methods.clear();
        info.methods.emplace("Greet", type_ensure<&Person::Greet>());
        
        return &info;
    }
}

int main()
{
    // Create object
    Person person{"Alice", 30};
    HsBa::Slicer::Utils::AnyObject obj(person);
    
    // Iterate through fields
    std::cout << "Person fields:" << std::endl;
    obj.ForeachField([&](std::string_view name, AnyObject value) {
        if (name == "name") {
            std::cout << "  Name: " << value.cast<std::string>() << std::endl;
        } else if (name == "age") {
            std::cout << "  Age: " << value.cast<int>() << std::endl;
        }
    });
    
    // Call method
    AnyObject args[] = { AnyObject(std::string("Hello")) };
    AnyObject result = obj.Invoke("Greet", args);
    std::cout << "\nGreeting: " << result.cast<std::string>() << std::endl;
    
    // Test copy and move
    HsBa::Slicer::Utils::AnyObject copied = obj;
    std::cout << "\nCopied person name: " 
              << copied.cast<Person>().name << std::endl;
    
    return 0;
}
```

### 8. Mock Support (Tests Only)

`AnyObject::Invoke` has built-in Mockit-style method stubbing so unit tests can isolate the code under test from its dependencies. To keep production builds clean, all Mock code is gated by the `HSBA_ANY_OBJECT_ENABLE_MOCK` macro.

**Activation conditions (any one is enough):**

- Define `HSBA_ANY_OBJECT_ENABLE_MOCK=1` explicitly.
- Any of the following common test-framework macros is visible in the current translation unit:
  - Boost.Test: `BOOST_TEST_MODULE`, `BOOST_TEST_INCLUDED`, `BOOST_TEST_DYN_LINK`, `BOOST_TEST_ALTERNATIVE_INIT_API`
  - GoogleTest: `GTEST_INCLUDE_GTEST_GTEST_H_`, `GTEST_API_`, `GTEST_HAS_MOCK`
  - Catch2: `CATCH_VERSION_MAJOR`, `CATCH_CONFIG_MAIN`, `CATCH_CONFIG_RUNNER`
  - doctest: `DOCTEST_LIBRARY_INCLUDED`, `DOCTEST_CONFIG_IMPLEMENT`, `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`
- Project level: when `HSBA_SLICER_USE_TESTS=ON`, CMake adds `PUBLIC HSBA_ANY_OBJECT_ENABLE_MOCK=1` to `HsBaSlicerBase`, so any test target that links it inherits the macro automatically.

**Runtime toggle:** even when compiled in, mocking is off by default and `AnyObject::Invoke` runs the real method with zero behavioural change. Use the APIs below to turn it on/off manually:

| API | Description |
| --- | --- |
| `Mock::EnableMock()` / `MockRegistry::instance().enable()` | Turn mocking on. |
| `Mock::DisableMock()` / `MockRegistry::instance().disable()` | Turn mocking off (registered stubs are preserved). |
| `Mock::SetMockEnabled(bool)` | Set the toggle explicitly. |
| `Mock::IsMockEnabled()` | Query the current state. |
| `Mock::ClearMocks()` / `MockRegistry::clear()` | Drop every stub and recorded call. |
| `Mock::MockRegistry::ScopedEnable` | RAII: enable on construction, restore the previous state on destruction. |
| `Mock::MockRegistry::ScopedStubs` | RAII: clear + enable on construction, clear + restore on destruction; ideal for per-case isolation. |

**Stubbing API:** stubs are keyed by `(TypeInfo*, method_name)`.

- `stub<T>(method_name, fn)`: install a custom stub with signature `AnyObject(void* self, std::span<AnyObject> args)`.
- `stub_return<T>(method_name, value)`: install a stub that always returns a fixed value.
- `stub_throw<T>(method_name, eptr)`: install a stub that always rethrows a `std::exception_ptr`.
- `stub_throw<T, E, Args...>(method_name, args...)`: install a stub that constructs `E(args...)` and throws it every call.
- `add_rule<T>(method_name, predicate, action)`: register an **argument-dependent** rule (fires only when the predicate matches).
- `when_called<T>(method_name, action)`: shorthand for `add_rule<T>(name, AnyArgs(), action)`.
- `unstub<T>(method_name)` / `clear<T>()`: remove a single stub, or every stub and rule of a type.
- `clear_rules<T>(method_name)`: drop rules only, keep the plain stub.
- `rule_count<T>(method_name)`: number of currently registered rules.
- `call_count<T>(method_name)`: number of recorded calls.
- `calls<T>(method_name)`: recorded calls, each carrying `method_name` and an argument snapshot.
- `reset_calls()`: drop recorded calls while keeping stubs and rules.

**Precedence:** `AnyObject::Invoke` first evaluates every rule registered for the current (type, method) pair in insertion order; the **first rule whose predicate matches** handles the call. If no rule matches, it falls back to the plain stub (the `stub*` family); if there is no stub either, the real method runs. Only calls **actually served by the mock** are recorded in `calls<T>()`.

All stubs, rules and recorded calls are guarded by an internal `std::mutex`, so parallel tests can install and inspect them safely. Predicates and actions are invoked **outside** the lock so user callbacks may re-enter the registry without deadlocking.

**Action factories (`Mock::`):**

| Factory | Purpose |
| --- | --- |
| `Return(value)` | Return a fixed value wrapped into an `AnyObject`. |
| `Throw(std::exception_ptr)` | Rethrow the given exception pointer. |
| `ThrowOf<E, Args...>(args...)` | Construct `E(args...)` and throw it on every call. |
| `Do(callable)` | Wrap any `AnyObject(void*, std::span<AnyObject>)` callable (free to read args, touch `self`, throw). |

**Predicate factories (`Mock::`):**

| Factory | Purpose |
| --- | --- |
| `AnyArgs()` | Always matches. |
| `ArgCountIs(n)` | Matches when the argument count equals `n`. |
| `ArgAtIs<V>(i, expected)` | `args[i]` can be `cast<V>()` and compares equal to `expected`. Returns false instead of throwing on type mismatch. |
| `ArgAtMatches<V>(i, unary_pred)` | `args[i].cast<V>()` satisfies `unary_pred`. |
| `AllOf(ps...)` / `AnyOf(ps...)` / `Not(p)` | Boolean combinations of predicates. |

**Example:**

```cpp
#include "base/any_object.hpp"

using namespace HsBa::Slicer::Utils;

BOOST_AUTO_TEST_CASE(player_add_health_mocked)
{
    Player p{100, 1.0f};
    AnyObject obj(p);
    AnyObject args[] = {AnyObject(50)};

    // Mocking is off by default: the real method runs.
    BOOST_CHECK_EQUAL(obj.Invoke("AddHealth", args).cast<int>(), 150);

    {
        // Enter the scope: clear + enable mocking and register a fixed-return stub.
        Mock::MockRegistry::ScopedStubs scope;
        scope.registry().stub_return<Player>("AddHealth", 999);

        BOOST_CHECK_EQUAL(obj.Invoke("AddHealth", args).cast<int>(), 999);
        BOOST_CHECK_EQUAL(scope.registry().call_count<Player>("AddHealth"), 1u);

        auto records = scope.registry().calls<Player>("AddHealth");
        BOOST_REQUIRE_EQUAL(records.size(), 1u);
        BOOST_CHECK_EQUAL(records[0].args[0].cast<int>(), 50);
    }
    // On scope exit: stubs cleared, mocking disabled, real behaviour restored.
    BOOST_CHECK(!Mock::IsMockEnabled());
    BOOST_CHECK_EQUAL(obj.Invoke("AddHealth", args).cast<int>(), 200);
}
```

**Throwing stubs:**

```cpp
using namespace HsBa::Slicer::Utils;
Mock::MockRegistry::ScopedStubs scope;

// Option A: pass a std::exception_ptr directly.
scope.registry().stub_throw<Player>("AddHealth",
    std::make_exception_ptr(std::runtime_error("db down")));

// Option B: construct and throw a specific exception type on every call.
scope.registry().stub_throw<Player, std::logic_error>("AddHealth", "invalid state");

// Option C: free-function shortcut.
Mock::StubThrow<Player, std::out_of_range>("AddHealth", "oor");

BOOST_CHECK_THROW(obj.Invoke("AddHealth", args), std::out_of_range);
// Throwing calls are still recorded in calls<Player>("AddHealth").
```

**Argument-dependent rules:**

```cpp
using namespace HsBa::Slicer::Utils;
Mock::MockRegistry::ScopedStubs scope;
auto& reg = scope.registry();

// Rule 1: throw when the argument is 0.
reg.add_rule<Player>("AddHealth",
    Mock::ArgAtIs<int>(0, 0),
    Mock::ThrowOf<std::runtime_error>("zero not allowed"));

// Rule 2: return a fixed value when the argument is > 100.
reg.add_rule<Player>("AddHealth",
    Mock::ArgAtMatches<int>(0, [](const int& v) { return v > 100; }),
    Mock::Return<int>(-1));

// Plain stub as fallback: custom behaviour driven by self and args.
reg.stub<Player>("AddHealth", [](void* self, std::span<AnyObject> a) -> AnyObject {
    auto* p = static_cast<Player*>(self);
    p->health += a[0].cast<int>();
    return AnyObject{p->health};
});

AnyObject zero[] = {AnyObject(0)};
AnyObject big[]  = {AnyObject(500)};
AnyObject mid[]  = {AnyObject(20)};

BOOST_CHECK_THROW(obj.Invoke("AddHealth", zero), std::runtime_error); // rule 1 fires
BOOST_CHECK_EQUAL(obj.Invoke("AddHealth", big).cast<int>(), -1);      // rule 2 fires
BOOST_CHECK_EQUAL(obj.Invoke("AddHealth", mid).cast<int>(), 120);     // falls back to the plain stub
```

**Combining predicates:** `AllOf` / `AnyOf` / `Not` compose arbitrarily complex argument conditions:

```cpp
reg.add_rule<Player>("AddHealth",
    Mock::AllOf(Mock::ArgCountIs(1u),
                Mock::Not(Mock::ArgAtIs<int>(0, 0))),
    Mock::Return<int>(42));
```

**Notes:**

- `MockRegistry` is a global singleton. Always use `ScopedStubs`, or call `ClearMocks()` explicitly in your fixture's `setup/teardown`, to avoid leaking state between test cases.
- The first parameter of a stub function is the original object's `void* self`; `static_cast` it back to the concrete type when you need to read or mutate its state.
- `CallRecord::args` returned by `calls<T>(name)` is captured via `AnyObject`'s copy constructor, so argument types must be copyable.
- **Throwing actions still record the call**: the `CallRecord` is written before dispatch, so you can assert on the exception and then verify `call_count` / `calls`.
- **Rules take precedence over the plain stub**: when both exist for the same (type, method), rules are evaluated in insertion order first; the plain stub is used only when no rule matches.
- `ArgAtIs` / `ArgAtMatches` **return false instead of throwing** when the argument count is too small or the stored type does not match, so loose predicates stay safe.
- When `HSBA_ANY_OBJECT_ENABLE_MOCK` is not defined, the `Mock` namespace does not exist and `Invoke` has no additional overhead.

## Notes

- You need to specialize the `GetTypeInfo` function for each custom type
- Type information must be static to ensure lifetime
- Field access order is not guaranteed to match definition order
- Bitfield fields are not included in the field list
- Virtual table pointers are not treated as fields (this is a C++ implementation detail; different compilers may have different implementations, such as Itanium C++ ABI, Microsoft C++ ABI, or other standard-licensed implementations)
- For classes containing virtual functions or bitfields, it's often necessary to use the non-C++-standard `offsetof` macro to obtain field offsets; for other simple cases, it's recommended to directly calculate offsets using field sizes and alignment requirements to avoid using non-standard extensions
- All method calls require correct argument type matching
- Private or protected members are not accessible (need friend or public members)
- Names in type information must exactly match names used during invocation
- Ensure corresponding type information is registered before using AnyObject
- Types containing references require special handling to ensure safety

## any_object.hpp Specialization Notes

The `any_object.hpp` header file already provides specialized `GetTypeInfo` for the following standard types:

### Specialized Standard Types

1. **`std::string`**
   - Registered methods: `size()`, `c_str()`, `at(size_t)`
   - Can be used directly without manual registration

2. **`std::string_view`**
   - Registered methods: `size()`, `data()`, `at(size_t)`
   - Can be used directly without manual registration

### Built-in Types

The following built-in types use the default `GetTypeInfo` template and automatically support basic operations:
- All arithmetic types (`int`, `float`, `double`, `size_t`, etc.)
- All POD types
- All copyable/movable user-defined types

### Specialization Recommendations

- **Standard containers**: Manual specialization required for `std::vector`, `std::map`, etc.
- **Smart pointers**: Manual specialization required for `std::unique_ptr`, `std::shared_ptr`, pay attention to ownership management
- **Enumeration types**: Recommended to specialize for better type safety
- **Complex business types**: Selectively register fields and methods based on runtime requirements

### Minimal Specialization Example

If you only need basic storage and type conversion functionality, you can skip registering any fields and methods:

```cpp
namespace HsBa::Slicer::Utils
{
    template<>
    TypeInfo* GetTypeInfo<MyType>()
    {
        static TypeInfo info;
        info.Name = "MyType";
        info.destroy = [](void* data) { delete static_cast<MyType*>(data); };
        info.copy = [](const void* data) -> void* { 
            return new MyType(*static_cast<const MyType*>(data)); 
        };
        info.move = [](void* data) -> void* { 
            return new MyType(std::move(*static_cast<MyType*>(data))); 
        };
        
        // Don't register any fields and methods
        info.fields.clear();
        info.methods.clear();
        
        return &info;
    }
}
```

This minimal specialization is suitable for:
- Type erasure and runtime type identification only
- Dynamic dispatch through other means (such as virtual functions)
- Performance-sensitive scenarios to reduce reflection overhead
