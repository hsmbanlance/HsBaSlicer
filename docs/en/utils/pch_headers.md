# PCH Headers (Precompiled Headers)

PCH Headers (Precompiled Headers) is a precompiled header file used to accelerate the compilation process, including commonly used C++ standard library and third-party library headers.

## Features

- Includes commonly used C++ standard library headers
- Includes common components from the Boost library
- Includes Eigen math library
- Includes base components from the Base module
- Improves compilation speed

## Included Headers

### Standard Library Components
- `<vector>` - Dynamic array container
- `<list>` - Doubly-linked list container
- `<unordered_map>` - Hash map container
- `<map>` - Ordered map container
- `<set>` - Ordered set container
- `<unordered_set>` - Hash set container
- `<any>` - Arbitrary type storage
- `<variant>` - Type-safe union
- `<string_view>` - String view
- `<string>` - String class
- `<algorithm>` - Algorithm library
- `<regex>` - Regular expression library

### Boost Library Components
- `<boost/any.hpp>` - Boost arbitrary type storage
- `<boost/variant.hpp>` - Boost variant type
- `<boost/variant2.hpp>` - Boost variant type (second edition)
- `<boost/date_time.hpp>` - Boost date/time library

### Math and Base Libraries
- `<Eigen/Core>` - Eigen linear algebra library core
- `"base/concepts.hpp"` - Basic concept definitions
- `"base/error.hpp"` - Error handling definitions
- `"base/template_helper.hpp"` - Template helper functions
- `"base/encoding_convert.hpp"` - Encoding conversion functions

## Usage

This file is typically used as a precompiled header file, automatically included in the project without manual import.

## Notes

- Precompiled header files should only include stable and unchanging headers
- Avoid including frequently changing headers in precompiled header files
- Precompiled headers help speed up compilation but increase memory usage