# Path Generation

<cite>
**Referenced Files in This Document**   
- [IPath.hpp](file://paths/IPath.hpp)
- [imagespath.hpp](file://paths/imagespath.hpp)
- [imagespath.cpp](file://paths/imagespath.cpp)
- [layerspath.hpp](file://paths/layerspath.hpp)
- [layerspath.cpp](file://paths/layerspath.cpp)
- [robotpath.hpp](file://paths/robotpath.hpp)
- [robotpath.cpp](file://paths/robotpath.cpp)
- [pointspath.hpp](file://paths/pointspath.hpp)
- [pointspath.cpp](file://paths/pointspath.cpp)
- [path_optimizer.hpp](file://LibHsBaSlicer/Path/path_optimizer.hpp)
- [path_optimizer.cpp](file://LibHsBaSlicer/Path/path_optimizer.cpp)
- [AreaGraph.hpp](file://utils/AreaGraph.hpp)
- [Graph.hpp](file://utils/Graph.hpp)
- [FloatPolygons.hpp](file://2D/FloatPolygons.hpp)
- [ImageToPolygons.hpp](file://2D/ImageToPolygons.hpp)
- [ImageToPolygons.cpp](file://2D/ImageToPolygons.cpp)
- [layers_path_test.cpp](file://tests/PathsOut/layers_path_test.cpp)
- [images_path_test.cpp](file://tests/PathsOut/images_path_test.cpp)
- [points_path_test.cpp](file://tests/PathsOut/points_path_test.cpp)
- [polygon_fill_test.cpp](file://tests/PolygonFill/polygon_fill_test.cpp)
</cite>

## Update Summary
**Changes Made**   
- Added comprehensive Path Optimization section covering RegionPathOptimizer class and genetic algorithm-based TSP solving
- Updated Core Path Generators Overview to include path optimization as a preprocessing step
- Added new sections for AreaGraph modeling and genetic algorithm implementation
- Enhanced existing sections with optimization workflow integration
- Updated use cases to include optimized path generation scenarios

## Table of Contents
1. [Introduction](#introduction)
2. [Core Path Generators Overview](#core-path-generators-overview)
3. [Path Optimization System](#path-optimization-system)
4. [Interface Definition: IPath](#interface-definition-ipath)
5. [ImagesPath: Raster Output Generation](#imagespath-raster-output-generation)
6. [LayersPath: Sliced Layer Data Export](#layerspath-sliced-layer-data-export)
7. [RobotPath: Robotic Toolpath Generation](#robotpath-robotic-toolpath-generation)
8. [Coordinate System and Unit Handling](#coordinate-system-and-unit-handling)
9. [Extensibility via Lua Scripting](#extensibility-via-lua-scripting)
10. [Use Cases in Manufacturing Scenarios](#use-cases-in-manufacturing-scenarios)
11. [Conclusion](#conclusion)

## Introduction

The Path Generation component in HsBaSlicer is responsible for transforming sliced polygon data into domain-specific output formats suitable for various manufacturing processes. This system provides a flexible and extensible architecture through a common interface `IPath`, with concrete implementations tailored for different output requirements: `ImagesPath` for raster image output, `LayersPath` for storing sliced layer geometry, and `RobotPath` for generating robotic toolpaths. 

**Updated** The system now includes a comprehensive path optimization layer that preprocesses polygon regions to minimize travel distance between independent areas using genetic algorithms and area graph modeling. This optimization significantly reduces empty travel moves in the final output by intelligently ordering region processing.

Each generator handles the conversion of `PolygonsD` (double-precision floating-point polygons) into specialized representations while supporting extensibility through Lua scripting for custom output formats. The optimization system operates before or after the filling stage, depending on the specific use case.

**Section sources**
- [IPath.hpp](file://paths/IPath.hpp)
- [FloatPolygons.hpp](file://2D/FloatPolygons.hpp)
- [path_optimizer.hpp](file://LibHsBaSlicer/Path/path_optimizer.hpp)

## Core Path Generators Overview

The path generation system consists of four primary implementations that inherit from the `IPath` interface, enhanced with a sophisticated optimization layer:

- **ImagesPath**: Generates raster images (PNG/SVG) from polygon data, suitable for visual inspection or bitmap-based manufacturing
- **LayersPath**: Stores sliced layer geometry in structured formats (e.g., SQLite database), preserving both configuration and polygonal data  
- **RobotPath**: Creates robot-specific toolpaths (ABB, KUKA, FANUC) with motion commands and velocity parameters
- **PointsPath**: Produces G-code-like point sequences with motion types, coordinates, and extrusion values

**Updated** All generators now benefit from the `RegionPathOptimizer` which can optimize the processing order of independent polygon regions before path generation, reducing travel distance and improving efficiency. The optimizer supports two modes: pre-fill optimization (for polygon contours) and post-fill optimization (for filled paths).

These generators share a common design pattern of collecting processed polygon data and providing multiple output methods, including direct file saving and string serialization, with support for custom Lua-based transformations.

```mermaid
classDiagram
class IPath {
<<interface>>
+~IPath()
+Save(path : filesystem : : path) const
+Save(path : filesystem : : path, script : string_view) const
+ToString() const
+ToString(script : string_view) const
}
class RegionPathOptimizer {
-config_ : ConfigFile
-images_ : unordered_map<string,string>
-callback_ : function<void(double,string_view)>
+addRegion(regionId : int, paths : PolygonsD)
+addPolygonRegion(regionId : int, polygons : PolygonsD)
+optimizeOrder() vector<int>
+buildPaths() PolygonsD
+buildPolygons() PolygonsD
}
class ImagesPath {
-config_ : ConfigFile
-images_ : unordered_map<string,string>
-callback_ : function<void(double,string_view)>
+AddImage(path : string_view, image_str : string_view)
}
class LayersPath {
-callback_ : function<void(string_view,string_view)>
-layers_ : vector<LayersData>
+push_back(layerConfig : string, layer : PolygonsD)
}
class RobotPath {
-robotType_ : RLType
-startPoint_ : OutPoints3
-points_ : vector<RLPoint>
-startProgramFunc_ : string
-endProgramFunc_ : string
+push_back(point : RLPoint)
+getRobotType() RLType
}
class PointsPath {
-points_ : vector<GPoint>
-startPoint_ : OutPoints3
-units_ : GCodeUnits
+push_back(point : GPoint)
}
IPath <|-- ImagesPath
IPath <|-- LayersPath
IPath <|-- RobotPath
IPath <|-- PointsPath
RegionPathOptimizer --> IPath : optimizes input for
```

**Diagram sources**
- [IPath.hpp](file://paths/IPath.hpp)
- [path_optimizer.hpp](file://LibHsBaSlicer/Path/path_optimizer.hpp)
- [imagespath.hpp](file://paths/imagespath.hpp)
- [layerspath.hpp](file://paths/layerspath.hpp)
- [robotpath.hpp](file://paths/robotpath.hpp)
- [pointspath.hpp](file://paths/pointspath.hpp)

## Path Optimization System

**New Section** The path optimization system represents a significant enhancement to the path generation pipeline, addressing the critical need to minimize travel distance between independent polygon regions during manufacturing processes.

### RegionPathOptimizer Architecture

The `RegionPathOptimizer` class implements a sophisticated optimization strategy that models independent polygon regions as vertices in a graph and solves for the optimal visiting sequence using genetic algorithms. It supports two distinct optimization modes:

1. **Polygon Mode (Pre-Fill)**: Optimizes the order of polygon contours before filling, treating all polygon vertices as candidate entry/exit gates
2. **Fill-Result Mode (Post-Fill)**: Optimizes the order of already-filled paths, using path endpoints as gates

```mermaid
flowchart TD
Start([Input Regions]) --> ModeCheck{"Optimization Mode?"}
ModeCheck --> |Polygon| PolygonMode["Model polygons as AreaGraph vertices<br/>All vertices = gates"]
ModeCheck --> |Fill Result| FillMode["Model filled paths as AreaGraph vertices<br/>Path endpoints = gates"]
PolygonMode --> BuildGraph["Build AreaGraph with inter-region routes"]
FillMode --> BuildGraph
BuildGraph --> SolveTSP["Genetic Algorithm TSP Solver"]
SolveTSP --> OptimizeOrder["Optimize region visiting order"]
OptimizeOrder --> ArrangeRegions["Arrange regions internally<br/>(greedy nearest neighbor)"]
ArrangeRegions --> Output["Optimized output:<br/>- buildPolygons(): ordered polygons<br/>- buildPaths(): complete fill paths"]
Output --> End([Optimized Result])
```

**Diagram sources**
- [path_optimizer.cpp:113-161](file://LibHsBaSlicer/Path/path_optimizer.cpp#L113-L161)
- [AreaGraph.hpp:231-308](file://utils/AreaGraph.hpp#L231-L308)

### AreaGraph Modeling

The optimization system uses an `AreaGraph` to model the complex relationships between polygon regions. Each region becomes a "area" in the graph, with candidate entry and exit points (gates) representing potential transition points between regions.

**Key Features:**
- **Gate System**: Each region has configurable gates that control valid entry/exit points
- **Internal Costs**: Distance-based costs for movement within a region between gates
- **Route Costs**: Inter-region travel costs calculated as minimum distances between gate pairs
- **Manual Overrides**: Support for specifying custom route costs when automatic calculation is insufficient

### Genetic Algorithm Implementation

The system employs a genetic algorithm to solve the Traveling Salesman Problem (TSP) for finding optimal region visiting sequences. The implementation includes:

- **Population-based Search**: Maintains a population of potential solutions
- **Crossover Operations**: Combines characteristics of parent solutions
- **Mutation Operations**: Introduces diversity through random changes
- **Fitness Evaluation**: Uses shortest path calculations between regions as fitness metrics

**Configuration Parameters:**
- Population size: 150 individuals (default)
- Maximum generations: 500 iterations (default)
- Mutation rate: 0.03 (default)
- Crossover rate: 0.85 (default)

### Integration with Path Generation

The optimization system integrates seamlessly with existing path generators:

```mermaid
sequenceDiagram
participant Input as "Input Polygons"
participant Optimizer as "RegionPathOptimizer"
participant Generator as "Path Generator"
participant Output as "Manufacturing Output"
Input->>Optimizer : addRegion()/addPolygonRegion()
Optimizer->>Optimizer : optimizeOrder()
Note over Optimizer : Genetic TSP solving<br/>AreaGraph construction
Optimizer->>Generator : Optimized region order
Generator->>Generator : Generate paths with reduced travel
Generator->>Output : Efficient manufacturing paths
```

**Diagram sources**
- [path_optimizer.cpp:380-408](file://LibHsBaSlicer/Path/path_optimizer.cpp#L380-L408)

### Use Cases and Benefits

**Benefits:**
- **Reduced Travel Time**: Minimizes empty travel moves between regions
- **Improved Efficiency**: Optimizes manufacturing time and material usage
- **Flexible Configuration**: Supports manual route cost overrides for special constraints
- **Dual Mode Operation**: Works with both raw polygons and filled paths

**Applications:**
- Multi-region 3D printing with reduced print head travel
- Robotic welding across multiple disconnected weld zones
- CNC machining of separate features in optimal sequence
- Laser cutting of complex parts with minimal repositioning

**Section sources**
- [path_optimizer.hpp:18-84](file://LibHsBaSlicer/Path/path_optimizer.hpp#L18-L84)
- [path_optimizer.cpp:38-161](file://LibHsBaSlicer/Path/path_optimizer.cpp#L38-L161)
- [AreaGraph.hpp:231-308](file://utils/AreaGraph.hpp#L231-L308)

## Interface Definition: IPath

The `IPath` interface serves as the foundation for all path generators, defining a consistent API for saving and serializing path data. It declares multiple overloads of `Save` and `ToString` methods that accept various combinations of output paths, Lua scripts, and function names. This design enables both direct output generation and script-driven customization. The interface uses `std::filesystem::path` for file operations and `std::string_view` for efficient string handling, ensuring compatibility with modern C++ practices.

The `IPath` interface does not define methods for adding data, as this responsibility is delegated to concrete implementations based on their specific data models. This separation allows each generator to maintain its own internal data structure while providing a uniform output interface.

**Section sources**
- [IPath.hpp](file://paths/IPath.hpp)

## ImagesPath: Raster Output Generation

The `ImagesPath` class converts polygon data into raster image formats such as PNG or SVG. It stores images as base64-encoded strings in an internal map, associating each image path with its encoded data. The generator accepts a configuration file and string during construction, which are preserved for output. Images are added via the `AddImage` method, which takes a file path and base64-encoded image data.

When saving, `ImagesPath` can either create a ZIP archive containing the configuration and all images, or execute a Lua script to generate custom output. The Lua integration allows for dynamic image processing, format conversion, or custom packaging. The `ToString` method provides a text representation that includes the configuration and decoded image data, facilitating debugging and inspection.

The implementation leverages OpenCV for rasterization, converting polygon coordinates to pixel positions based on a specified pixel size. This enables precise control over image resolution and scaling, making it suitable for applications requiring pixel-perfect correspondence between geometry and output.

**Updated** ImagesPath can now benefit from pre-optimization of input polygon regions using the RegionPathOptimizer, resulting in more efficient image generation when dealing with multiple disconnected polygon areas.

```mermaid
flowchart TD
Start([Polygon Data]) --> Optimize{"Optimize regions?"}
Optimize --> |Yes| RegionOpt["RegionPathOptimizer<br/>optimizeOrder() + buildPolygons()"]
Optimize --> |No| DirectRasterize["Direct rasterization"]
RegionOpt --> Rasterize["Rasterize to Image<br/>(ToImage function)"]
DirectRasterize --> Rasterize
Rasterize --> Encode["Encode as Base64"]
Encode --> Store["Store in images_ map"]
Store --> CheckScript{"Script provided?"}
CheckScript --> |No| CreateZIP["Create ZIP archive<br/>(config + images)"]
CheckScript --> |Yes| ExecuteLua["Execute Lua script<br/>(with images table)"]
ExecuteLua --> CustomOutput["Generate custom output"]
CreateZIP --> Output["Save to file"]
CustomOutput --> Output
Output --> End([Image Output])
```

**Diagram sources**
- [imagespath.hpp](file://paths/imagespath.hpp)
- [imagespath.cpp](file://paths/imagespath.cpp)
- [ImageToPolygons.cpp](file://2D/ImageToPolygons.cpp)

## LayersPath: Sliced Layer Data Export

The `LayersPath` class manages the export of sliced layer data, storing both configuration parameters and polygon geometry for each layer. It maintains a vector of `LayersData` structures, each containing a layer configuration string and corresponding `PolygonsD` data. Layers are added using the `push_back` method, which takes a configuration string and polygon set.

The primary output format is a SQLite database, where each layer is stored as a record with configuration metadata and serialized polygon data. The serialization format uses a custom text representation with coordinate tuples, enabling both human readability and efficient parsing. Alternatively, Lua scripts can be used to transform the layer data into other formats such as CSV, JSON, or custom binary formats.

The `LayersPath` implementation demonstrates a hybrid approach to data persistence, combining structured database storage with scriptable transformation capabilities. This flexibility allows the system to adapt to different workflow requirements, from direct database integration to custom file format generation.

**Updated** LayersPath can incorporate optimized polygon regions through the RegionPathOptimizer, ensuring that exported layer data reflects the most efficient processing order for downstream manufacturing processes.

```mermaid
flowchart TD
Start([Polygon Layers]) --> Optimize{"Optimize regions?"}
Optimize --> |Yes| ApplyOpt["Apply RegionPathOptimizer<br/>to reduce travel distance"]
Optimize --> |No| AddLayer["Add to layers_ vector<br/>(push_back method)"]
ApplyOpt --> AddLayer
AddLayer --> Serialize["Serialize to text format<br/>{config: data: {[(x,y),...]}}"]
Serialize --> CheckOutput{"Output type?"}
CheckOutput --> |Database| CreateTable["Create SQLite table<br/>(layers with BLOB data)"]
CheckOutput --> |Script| ExecuteLua["Execute Lua script<br/>(with layers table)"]
CreateTable --> InsertData["Insert serialized polygons"]
InsertData --> SaveDB["Save to SQLite file"]
ExecuteLua --> CustomFormat["Generate custom format"]
SaveDB --> End([Database Output])
CustomFormat --> End
```

**Diagram sources**
- [layerspath.hpp](file://paths/layerspath.hpp)
- [layerspath.cpp](file://paths/layerspath.cpp)

## RobotPath: Robotic Toolpath Generation

The `RobotPath` class generates toolpaths for industrial robots, supporting multiple robot types including ABB, KUKA, and FANUC. It stores a sequence of `RLPoint` structures, each representing a robot motion command with end coordinates, middle point (for arcs), velocity, and motion type (MoveJ, MoveL, MoveC). The generator maintains the robot type, start point, and program function names for proper code generation.

The `ToString` method produces robot-specific code by delegating to `GenerateAbbCode`, `GenerateKukaCode`, or `GenerateFanucCode` based on the robot type. Each generator creates syntactically correct robot programs with appropriate motion commands, speed parameters, and safety configurations. For example, ABB output uses `MOVEJ`, `MOVEL`, and `MOVEC` commands with zone and tool data specifications, while KUKA uses `LIN` and `CIRC` statements.

The implementation supports program segmentation through special point types like `ProgramLStart`, `ProgramCStart`, and their corresponding end types, allowing for modular program generation with custom start/end functions. This enables integration with existing robot programs and complex manufacturing sequences.

**Updated** RobotPath generation can now leverage pre-optimized polygon regions through the RegionPathOptimizer, resulting in robot programs with significantly reduced travel time between independent welding or machining zones.

```mermaid
sequenceDiagram
participant User as "Application"
participant Optimizer as "RegionPathOptimizer"
participant RobotPath as "RobotPath"
participant Lua as "Lua Environment"
User->>Optimizer : addRegion() for multiple zones
Optimizer->>Optimizer : optimizeOrder()
User->>RobotPath : push_back(RLPoint)
RobotPath->>RobotPath : Store point in points_ vector
User->>RobotPath : ToString()
RobotPath->>RobotPath : Check robotType_
alt ABB Robot
RobotPath->>RobotPath : GenerateAbbCode()
RobotPath->>RobotPath : Format MOVEJ/MOVEL/MOVEC commands
else KUKA Robot
RobotPath->>RobotPath : GenerateKukaCode()
RobotPath->>RobotPath : Format LIN/CIRC commands
else FANUC Robot
RobotPath->>RobotPath : GenerateFanucCode()
RobotPath->>RobotPath : Format J P/ARC commands
end
RobotPath-->>User : Return formatted robot code
```

**Diagram sources**
- [robotpath.hpp](file://paths/robotpath.hpp)
- [robotpath.cpp](file://paths/robotpath.cpp)
- [path_optimizer.cpp:380-408](file://LibHsBaSlicer/Path/path_optimizer.cpp#L380-L408)

## Coordinate System and Unit Handling

The path generation system maintains consistent coordinate handling across all generators, using double-precision floating-point values (`double`) for geometric calculations. The `OutPoints3` structure stores X, Y, and Z coordinates as `float` values, providing sufficient precision for manufacturing applications while maintaining memory efficiency.

Unit handling is implemented in the `PointsPath` class through the `GCodeUnits` enum, which supports both millimeter (`mm`) and inch units. The `ToString` method automatically includes the appropriate G-code unit command (`G21` for mm, `G20` for inches) in the output. This ensures that generated toolpaths are correctly interpreted by CNC controllers regardless of the unit system.

Coordinate transformations are handled during the slicing process, with the path generators receiving pre-transformed `PolygonsD` data. This separation of concerns allows the path generation system to focus on format conversion rather than geometric manipulation, improving both performance and reliability.

**Updated** The optimization system preserves coordinate system consistency throughout the optimization process, ensuring that optimized polygon regions maintain their original spatial relationships and units.

**Section sources**
- [IPath.hpp](file://paths/IPath.hpp)
- [pointspath.hpp](file://paths/pointspath.hpp)
- [FloatPolygons.hpp](file://2D/FloatPolygons.hpp)

## Extensibility via Lua Scripting

A key feature of the path generation system is its extensibility through Lua scripting. All generators support script-based output generation, allowing users to customize the output format without modifying the C++ code. When a Lua script is provided to `Save` or `ToString`, the system creates a Lua state, registers necessary libraries (SQLite, Zipper, Cipher), and exposes the path data as global variables.

For example, `ImagesPath` exposes a `config` table and `images` array, while `LayersPath` provides a `layers` table with configuration and polygon data. The script can then process this data and return a custom string, which is either saved to file or returned to the caller. This mechanism enables powerful customization scenarios, such as generating specialized file formats, applying post-processing filters, or integrating with external systems.

**Updated** The path optimization system also provides extensive Lua scripting capabilities through the `PathOptimize` module, enabling users to implement custom optimization strategies, integrate with external optimization libraries, or apply domain-specific heuristics for region ordering.

The Lua integration is secured through sandboxing and error handling, ensuring that script failures do not compromise the stability of the main application. Error messages are captured and reported through exceptions, providing clear feedback for script debugging.

**Section sources**
- [imagespath.cpp](file://paths/imagespath.cpp)
- [layerspath.cpp](file://paths/layerspath.cpp)
- [robotpath.cpp](file://paths/robotpath.cpp)
- [path_optimizer.cpp:619-659](file://LibHsBaSlicer/Path/path_optimizer.cpp#L619-L659)

## Use Cases in Manufacturing Scenarios

The different path generators serve distinct manufacturing scenarios, enhanced by the optimization system:

**ImagesPath** is ideal for:
- Visual inspection of sliced layers
- Bitmap-based manufacturing processes
- Quality control documentation
- Integration with image-processing pipelines
- **Enhanced**: Optimized multi-region image generation with reduced processing overhead

**LayersPath** is suited for:
- Archiving sliced geometry with metadata
- Database-backed manufacturing workflows
- Multi-material printing with layer-specific configurations
- Post-processing analysis of slice data
- **Enhanced**: Optimized layer ordering for faster downstream processing

**RobotPath** enables:
- Robotic additive manufacturing (3D printing with robotic arms)
- Automated welding and coating applications
- CNC machining with articulated robots
- Complex multi-axis manufacturing sequences
- **Enhanced**: Significantly reduced robot travel time between independent work zones

**PointsPath** supports:
- Traditional 3D printing with Cartesian printers
- CNC milling and routing
- Laser cutting and engraving
- Any application requiring G-code-like point-to-point motion
- **Enhanced**: Optimized toolpath sequencing for reduced machine idle time

**RegionPathOptimizer Applications:**
- Multi-zone welding with minimized arc-on time
- Multi-feature CNC machining with reduced air cuts
- Multi-area laser cutting with optimized cutting paths
- Multi-region 3D printing with reduced travel time
- Complex assembly operations with efficient part sequencing

Each generator can be selected based on the specific requirements of the manufacturing process, with the option to extend functionality through Lua scripting for specialized applications. The optimization system provides additional benefits across all use cases by minimizing unnecessary travel and maximizing productive work time.

**Section sources**
- [imagespath.hpp](file://paths/imagespath.hpp)
- [layerspath.hpp](file://paths/layerspath.hpp)
- [robotpath.hpp](file://paths/robotpath.hpp)
- [pointspath.hpp](file://paths/pointspath.hpp)
- [path_optimizer.hpp:18-84](file://LibHsBaSlicer/Path/path_optimizer.hpp#L18-L84)

## Conclusion

The Path Generation component in HsBaSlicer provides a robust and flexible framework for converting sliced polygon data into various manufacturing-ready formats. Through a well-defined interface and specialized implementations, it supports diverse output requirements while maintaining a consistent API. The integration of Lua scripting enables extensive customization without compromising the core system's stability.

**Updated** The addition of the comprehensive path optimization system represents a significant advancement in manufacturing efficiency. By intelligently optimizing the order of independent polygon regions using genetic algorithms and area graph modeling, the system dramatically reduces travel time and improves overall manufacturing throughput. This optimization layer seamlessly integrates with all existing path generators, providing immediate benefits across all supported manufacturing processes.

This architecture effectively bridges the gap between geometric processing and physical manufacturing, supporting a wide range of applications from traditional 3D printing to advanced robotic fabrication, with the optimization system ensuring maximum efficiency in multi-region scenarios.