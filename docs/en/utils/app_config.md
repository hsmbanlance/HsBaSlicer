# AppConfig

The AppConfig component provides a thread-safe singleton pattern implementation for managing application configuration information.

## Features

- Thread-safe singleton pattern implementation
- Uses mutex to ensure thread safety
- Provides methods to get SevenZip path

## Usage

### 1. Get Instance

```cpp
#include "utils/app_config.hpp"

// Get AppConfig singleton instance
auto& config = HsBa::Slicer::AppConfigSingletone::GetInstance();
```

### 2. Using Configuration Functions

```cpp
// Get SevenZip path
std::string sevenZPath = config.GetSevenZPath();
```

### 3. Clean Up Instance

```cpp
// Clean up instance when application exits
HsBa::Slicer::AppConfigSingletone::DeleteInstance();
```

### 4. Complete Example

```cpp
#include "utils/app_config.hpp"
#include <iostream>

int main() {
    // Get configuration instance
    auto& config = HsBa::Slicer::AppConfigSingletone::GetInstance();
    
    // Use configuration functions
    std::string sevenZPath = config.GetSevenZPath();
    std::cout << "SevenZ Path: " << sevenZPath << std::endl;
    
    // Clean up when application ends
    HsBa::Slicer::AppConfigSingletone::DeleteInstance();
    
    return 0;
}
```

## Notes

- AppConfigSingletone is a singleton class, always get instance through GetInstance()
- Call DeleteInstance() to clean up resources after use
- All methods are thread-safe, internally using shared_mutex for synchronization
- Currently only provides function to get SevenZip path, can be extended as needed