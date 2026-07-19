# Pipeline Conversion API

<cite>
**Referenced Files in This Document**
- [pipeline_convert.h](file://DllHsBaSlicer/pipeline_convert.h)
- [pipeline_convert.cpp](file://DllHsBaSlicer/pipeline_convert.cpp)
- [Msg2PipelineConfig.hpp](file://convert/Msg2PipelineConfig.hpp)
- [Msg2PipelineConfig.cpp](file://convert/Msg2PipelineConfig.cpp)
- [PipelineConfig2Msg.hpp](file://convert/PipelineConfig2Msg.hpp)
- [PipelineConfig2Msg.cpp](file://convert/PipelineConfig2Msg.cpp)
- [fdm_pipeline.proto](file://proto/fdm_pipeline.proto)
- [sla_pipeline.proto](file://proto/sla_pipeline.proto)
- [fdm_pipeline.h](file://DllHsBaSlicer/fdm_pipeline.h)
- [fdm_pipeline.cpp](file://DllHsBaSlicer/fdm_pipeline.cpp)
- [sla_pipeline.h](file://DllHsBaSlicer/sla_pipeline.h)
- [sla_pipeline.cpp](file://DllHsBaSlicer/sla_pipeline.cpp)
</cite>

## Table of Contents
1. [Introduction](#introduction)
2. [Project Structure](#project-structure)
3. [Core Components](#core-components)
4. [Architecture Overview](#architecture-overview)
5. [Detailed Component Analysis](#detailed-component-analysis)
6. [Dependency Analysis](#dependency-analysis)
7. [Performance Considerations](#performance-considerations)
8. [Troubleshooting Guide](#troubleshooting-guide)
9. [Conclusion](#conclusion)

## Introduction
This document describes the Pipeline Conversion API that bridges C-compatible pipeline configuration and result structures with protobuf messages for both FDM and SLA printing pipelines. It explains how to serialize and deserialize configurations and results, memory ownership rules, and the end-to-end data flow from C structs to protobuf wire format and back.

## Project Structure
The conversion layer is implemented across three main areas:
- Public C API for serialization/deserialization (DLL interface)
- Internal converters between C structs and protobuf messages
- Protobuf schema definitions for FDM and SLA pipelines

```mermaid
graph TB
subgraph "Public C API"
PC_H["pipeline_convert.h"]
PC_CPP["pipeline_convert.cpp"]
end
subgraph "Converters"
M2C_HPP["Msg2PipelineConfig.hpp"]
M2C_CPP["Msg2PipelineConfig.cpp"]
C2M_HPP["PipelineConfig2Msg.hpp"]
C2M_CPP["PipelineConfig2Msg.cpp"]
end
subgraph "Protobuf Schemas"
FDM_PROTO["fdm_pipeline.proto"]
SLA_PROTO["sla_pipeline.proto"]
end
subgraph "Pipeline Types"
FDM_H["fdm_pipeline.h"]
SLA_H["sla_pipeline.h"]
end
PC_H --> PC_CPP
PC_CPP --> M2C_HPP
PC_CPP --> C2M_HPP
M2C_CPP --> FDM_H
M2C_CPP --> SLA_H
C2M_CPP --> FDM_H
C2M_CPP --> SLA_H
M2C_CPP --> FDM_PROTO
M2C_CPP --> SLA_PROTO
C2M_CPP --> FDM_PROTO
C2M_CPP --> SLA_PROTO
```

**Diagram sources**
- [pipeline_convert.h:1-124](file://DllHsBaSlicer/pipeline_convert.h#L1-L124)
- [pipeline_convert.cpp:1-210](file://DllHsBaSlicer/pipeline_convert.cpp#L1-L210)
- [Msg2PipelineConfig.hpp:1-37](file://convert/Msg2PipelineConfig.hpp#L1-L37)
- [Msg2PipelineConfig.cpp:1-129](file://convert/Msg2PipelineConfig.cpp#L1-L129)
- [PipelineConfig2Msg.hpp:1-29](file://convert/PipelineConfig2Msg.hpp#L1-L29)
- [PipelineConfig2Msg.cpp:1-124](file://convert/PipelineConfig2Msg.cpp#L1-L124)
- [fdm_pipeline.proto:1-63](file://proto/fdm_pipeline.proto#L1-L63)
- [sla_pipeline.proto:1-67](file://proto/sla_pipeline.proto#L1-L67)
- [fdm_pipeline.h:1-156](file://DllHsBaSlicer/fdm_pipeline.h#L1-L156)
- [sla_pipeline.h:1-160](file://DllHsBaSlicer/sla_pipeline.h#L1-L160)

**Section sources**
- [pipeline_convert.h:1-124](file://DllHsBaSlicer/pipeline_convert.h#L1-L124)
- [pipeline_convert.cpp:1-210](file://DllHsBaSlicer/pipeline_convert.cpp#L1-L210)
- [Msg2PipelineConfig.hpp:1-37](file://convert/Msg2PipelineConfig.hpp#L1-L37)
- [Msg2PipelineConfig.cpp:1-129](file://convert/Msg2PipelineConfig.cpp#L1-L129)
- [PipelineConfig2Msg.hpp:1-29](file://convert/PipelineConfig2Msg.hpp#L1-L29)
- [PipelineConfig2Msg.cpp:1-124](file://convert/PipelineConfig2Msg.cpp#L1-L124)
- [fdm_pipeline.proto:1-63](file://proto/fdm_pipeline.proto#L1-L63)
- [sla_pipeline.proto:1-67](file://proto/sla_pipeline.proto#L1-L67)
- [fdm_pipeline.h:1-156](file://DllHsBaSlicer/fdm_pipeline.h#L1-L156)
- [sla_pipeline.h:1-160](file://DllHsBaSlicer/sla_pipeline.h#L1-L160)

## Core Components
- Public C API functions for FDM and SLA:
  - Config and result serialization to/from protobuf bytes
  - Memory cleanup helpers for converted config strings
- Converters:
  - From protobuf message to C struct (allocates string fields via malloc)
  - From C struct to protobuf message
- Protobuf schemas:
  - FDM pipeline config/result messages and enums
  - SLA pipeline config/result messages and enums

Key responsibilities:
- Validate inputs and return clear success/failure codes
- Manage memory ownership explicitly (caller frees allocated buffers)
- Provide symmetric conversions for both directions

**Section sources**
- [pipeline_convert.h:14-117](file://DllHsBaSlicer/pipeline_convert.h#L14-L117)
- [pipeline_convert.cpp:23-179](file://DllHsBaSlicer/pipeline_convert.cpp#L23-L179)
- [Msg2PipelineConfig.hpp:14-32](file://convert/Msg2PipelineConfig.hpp#L14-L32)
- [Msg2PipelineConfig.cpp:27-126](file://convert/Msg2PipelineConfig.cpp#L27-L126)
- [PipelineConfig2Msg.hpp:14-24](file://convert/PipelineConfig2Msg.hpp#L14-L24)
- [PipelineConfig2Msg.cpp:6-121](file://convert/PipelineConfig2Msg.cpp#L6-L121)
- [fdm_pipeline.proto:19-63](file://proto/fdm_pipeline.proto#L19-L63)
- [sla_pipeline.proto:18-67](file://proto/sla_pipeline.proto#L18-L67)

## Architecture Overview
End-to-end flow for converting a C config to protobuf bytes and back:

```mermaid
sequenceDiagram
participant Caller as "Caller"
participant API as "pipeline_convert.cpp"
participant ConvIn as "Msg2PipelineConfig.cpp"
participant ConvOut as "PipelineConfig2Msg.cpp"
participant Proto as "protobuf messages"
Note over Caller,API : Serialize C -> Proto bytes
Caller->>API : HsBaFdmConfigToProtoBytes(config, &out_data, &out_size)
API->>ConvOut : FdmConfigToMsg(config, msg)
ConvOut-->>API : msg populated
API->>Proto : msg.SerializeToArray(buf, size)
API-->>Caller : out_data, out_size
Note over Caller,API : Deserialize Proto bytes -> C
Caller->>API : HsBaFdmConfigFromProtoBytes(proto_data, proto_size, &config)
API->>Proto : msg.ParseFromArray(proto_data, proto_size)
API->>ConvIn : MsgToFdmConfig(msg, &config)
ConvIn-->>API : config with malloc'd strings
API-->>Caller : config
```

**Diagram sources**
- [pipeline_convert.cpp:37-99](file://DllHsBaSlicer/pipeline_convert.cpp#L37-L99)
- [Msg2PipelineConfig.cpp:27-64](file://convert/Msg2PipelineConfig.cpp#L27-L64)
- [PipelineConfig2Msg.cpp:6-48](file://convert/PipelineConfig2Msg.cpp#L6-L48)
- [fdm_pipeline.proto:19-54](file://proto/fdm_pipeline.proto#L19-L54)

## Detailed Component Analysis

### FDM Serialization/Deserialization API
- Functions:
  - HsBaFdmConfigToProtoBytes / HsBaFdmConfigFromProtoBytes
  - HsBaFdmResultToProtoBytes / HsBaFdmResultFromProtoBytes
  - HsBaFreeFdmConfigStrings
- Behavior:
  - Validates pointers and sizes
  - Parses or serializes using protobuf
  - Allocates output buffer with malloc; caller must free
  - Converts between C structs and protobuf messages

```mermaid
flowchart TD
Start([Entry]) --> CheckArgs["Validate input pointers and sizes"]
CheckArgs --> |Invalid| ReturnZero["Return 0"]
CheckArgs --> |Valid| BuildOrParse["Build or parse protobuf message"]
BuildOrParse --> Serialize["Serialize to array"]
Serialize --> Success{"Serialization success?"}
Success --> |No| FreeBuf["Free buffer and return 0"]
Success --> |Yes| OutParams["Set out_data, out_size and return 1"]
ReturnZero --> End([Exit])
FreeBuf --> End
OutParams --> End
```

**Diagram sources**
- [pipeline_convert.cpp:37-99](file://DllHsBaSlicer/pipeline_convert.cpp#L37-L99)

**Section sources**
- [pipeline_convert.h:21-59](file://DllHsBaSlicer/pipeline_convert.h#L21-L59)
- [pipeline_convert.cpp:23-99](file://DllHsBaSlicer/pipeline_convert.cpp#L23-L99)
- [Msg2PipelineConfig.cpp:27-73](file://convert/Msg2PipelineConfig.cpp#L27-L73)
- [PipelineConfig2Msg.cpp:6-59](file://convert/PipelineConfig2Msg.cpp#L6-L59)
- [fdm_pipeline.proto:19-63](file://proto/fdm_pipeline.proto#L19-L63)

### SLA Serialization/Deserialization API
- Functions:
  - HsBaSlaConfigToProtoBytes / HsBaSlaConfigFromProtoBytes
  - HsBaSlaResultToProtoBytes / HsBaSlaResultFromProtoBytes
  - HsBaFreeSlaConfigStrings
- Behavior:
  - Same validation and allocation semantics as FDM
  - Maps SLA-specific fields including image type and dimensions

```mermaid
flowchart TD
Start([Entry]) --> CheckArgs["Validate input pointers and sizes"]
CheckArgs --> |Invalid| ReturnZero["Return 0"]
CheckArgs --> |Valid| BuildOrParse["Build or parse protobuf message"]
BuildOrParse --> Serialize["Serialize to array"]
Serialize --> Success{"Serialization success?"}
Success --> |No| FreeBuf["Free buffer and return 0"]
Success --> |Yes| OutParams["Set out_data, out_size and return 1"]
ReturnZero --> End([Exit])
FreeBuf --> End
OutParams --> End
```

**Diagram sources**
- [pipeline_convert.cpp:117-179](file://DllHsBaSlicer/pipeline_convert.cpp#L117-L179)

**Section sources**
- [pipeline_convert.h:62-99](file://DllHsBaSlicer/pipeline_convert.h#L62-L99)
- [pipeline_convert.cpp:103-179](file://DllHsBaSlicer/pipeline_convert.cpp#L103-L179)
- [Msg2PipelineConfig.cpp:75-126](file://convert/Msg2PipelineConfig.cpp#L75-L126)
- [PipelineConfig2Msg.cpp:61-121](file://convert/PipelineConfig2Msg.cpp#L61-L121)
- [sla_pipeline.proto:18-67](file://proto/sla_pipeline.proto#L18-L67)

### Data Models and Mapping
- FDM model mapping:
  - Config fields include model info, slice parameters, fill settings, support options, path/printing parameters, Lua customization hooks, and output path
  - Result includes success flag, total layers, G-code content, error message, and elapsed time
- SLA model mapping:
  - Config fields include model info, slice/exposure/lift/retract parameters, floor/raft settings, support options, Lua customization hooks, output path, and image export settings
  - Result includes success flag, total layers, export path, error message, and elapsed time

```mermaid
classDiagram
class FdmConfig {
+string model_name
+string model_path
+float layer_height
+float first_layer_height
+double fill_spacing
+enum fill_mode
+double fill_angle
+int wall_count
+int top_layer_count
+int bottom_layer_count
+double infill_density
+int enable_support
+float overhang_angle
+float support_gap
+float support_diameter
+float support_density
+enum support_pattern
+int interface_layers
+float interface_density
+float line_width
+float print_speed
+float travel_speed
+float extrusion_multiplier
+string support_lua_script
+string support_lua_func
+string infill_lua_script
+string infill_lua_func
+string output_path
}
class FdmResult {
+int success
+int total_layers
+string gcode_content
+string error_message
+double elapsed_seconds
}
class SlaConfig {
+string model_name
+string model_path
+float layer_height
+float first_layer_height
+float bottom_exposure_time
+float normal_exposure_time
+float bottom_lift_distance
+float lift_distance
+float lift_speed
+float retract_speed
+float floor_raft_offset
+float floor_border_width
+float floor_fill_spacing
+float floor_fill_angle
+int floor_border_count
+int floor_use_convex_hull
+int enable_support
+float overhang_angle
+float support_gap
+float support_diameter
+float support_density
+enum support_pattern
+string support_lua_script
+string support_lua_func
+string floor_lua_script
+string floor_lua_func
+string export_lua_script
+string export_lua_func
+string output_path
+enum image_type
+int image_width
+int image_height
}
class SlaResult {
+int success
+int total_layers
+string export_path
+string error_message
+double elapsed_seconds
}
```

**Diagram sources**
- [fdm_pipeline.h:35-93](file://DllHsBaSlicer/fdm_pipeline.h#L35-L93)
- [sla_pipeline.h:36-98](file://DllHsBaSlicer/sla_pipeline.h#L36-L98)
- [fdm_pipeline.proto:19-63](file://proto/fdm_pipeline.proto#L19-L63)
- [sla_pipeline.proto:18-67](file://proto/sla_pipeline.proto#L18-L67)

**Section sources**
- [fdm_pipeline.h:35-93](file://DllHsBaSlicer/fdm_pipeline.h#L35-L93)
- [sla_pipeline.h:36-98](file://DllHsBaSlicer/sla_pipeline.h#L36-L98)
- [fdm_pipeline.proto:19-63](file://proto/fdm_pipeline.proto#L19-L63)
- [sla_pipeline.proto:18-67](file://proto/sla_pipeline.proto#L18-L67)

### Memory Management and Ownership
- Output buffers from ToProtoBytes are allocated with malloc; callers must free them after use.
- When deserializing from proto bytes into C structs, string fields are allocated with malloc; callers must free these strings using provided cleanup helpers or their own logic.
- Dedicated cleanup helpers exist for freeing config string fields:
  - HsBaFreeFdmConfigStrings
  - HsBaFreeSlaConfigStrings

```mermaid
flowchart TD
Start([Deserialize Entry]) --> Parse["Parse protobuf message"]
Parse --> MapFields["Map fields to C struct"]
MapFields --> DupStrings["Duplicate string fields (malloc)"]
DupStrings --> ReturnStruct["Return C struct with owned strings"]
ReturnStruct --> Cleanup["Call HsBaFree*ConfigStrings when done"]
Cleanup --> End([Exit])
```

**Diagram sources**
- [Msg2PipelineConfig.cpp:15-23](file://convert/Msg2PipelineConfig.cpp#L15-L23)
- [pipeline_convert.cpp:183-209](file://DllHsBaSlicer/pipeline_convert.cpp#L183-L209)

**Section sources**
- [Msg2PipelineConfig.cpp:15-23](file://convert/Msg2PipelineConfig.cpp#L15-L23)
- [pipeline_convert.cpp:183-209](file://DllHsBaSlicer/pipeline_convert.cpp#L183-L209)

### Integration with Pipeline Execution
- The same C structs used by the conversion API are consumed by the pipeline execution APIs:
  - FDM: HsBaRunFdmPipeline, HsBaRunFdmPipelineAsync
  - SLA: HsBaRunSlaPipeline, HsBaRunSlaPipelineAsync
- Results returned by pipeline execution can be serialized to protobuf via the conversion API for transport or storage.

```mermaid
sequenceDiagram
participant App as "Application"
participant Convert as "Conversion API"
participant Exec as "Pipeline Execution"
App->>Convert : HsBaFdmConfigFromProtoBytes(...)
Convert-->>App : HsBaFdmPipelineConfig_t
App->>Exec : HsBaRunFdmPipeline(&cfg, ...)
Exec-->>App : HsBaFdmPipelineResult_t
App->>Convert : HsBaFdmResultToProtoBytes(&result, &buf, &size)
Convert-->>App : buf, size
```

**Diagram sources**
- [fdm_pipeline.h:107-149](file://DllHsBaSlicer/fdm_pipeline.h#L107-L149)
- [fdm_pipeline.cpp:386-419](file://DllHsBaSlicer/fdm_pipeline.cpp#L386-L419)
- [pipeline_convert.cpp:62-99](file://DllHsBaSlicer/pipeline_convert.cpp#L62-L99)

**Section sources**
- [fdm_pipeline.h:107-149](file://DllHsBaSlicer/fdm_pipeline.h#L107-L149)
- [fdm_pipeline.cpp:386-419](file://DllHsBaSlicer/fdm_pipeline.cpp#L386-L419)
- [pipeline_convert.cpp:62-99](file://DllHsBaSlicer/pipeline_convert.cpp#L62-L99)

## Dependency Analysis
High-level dependencies among components:

```mermaid
graph LR
A["pipeline_convert.cpp"] --> B["Msg2PipelineConfig.cpp"]
A --> C["PipelineConfig2Msg.cpp"]
B --> D["fdm_pipeline.h"]
B --> E["sla_pipeline.h"]
C --> D
C --> E
B --> F["fdm_pipeline.proto"]
B --> G["sla_pipeline.proto"]
C --> F
C --> G
```

**Diagram sources**
- [pipeline_convert.cpp:1-210](file://DllHsBaSlicer/pipeline_convert.cpp#L1-L210)
- [Msg2PipelineConfig.cpp:1-129](file://convert/Msg2PipelineConfig.cpp#L1-L129)
- [PipelineConfig2Msg.cpp:1-124](file://convert/PipelineConfig2Msg.cpp#L1-L124)
- [fdm_pipeline.h:1-156](file://DllHsBaSlicer/fdm_pipeline.h#L1-L156)
- [sla_pipeline.h:1-160](file://DllHsBaSlicer/sla_pipeline.h#L1-L160)
- [fdm_pipeline.proto:1-63](file://proto/fdm_pipeline.proto#L1-L63)
- [sla_pipeline.proto:1-67](file://proto/sla_pipeline.proto#L1-L67)

**Section sources**
- [pipeline_convert.cpp:1-210](file://DllHsBaSlicer/pipeline_convert.cpp#L1-L210)
- [Msg2PipelineConfig.cpp:1-129](file://convert/Msg2PipelineConfig.cpp#L1-L129)
- [PipelineConfig2Msg.cpp:1-124](file://convert/PipelineConfig2Msg.cpp#L1-L124)
- [fdm_pipeline.h:1-156](file://DllHsBaSlicer/fdm_pipeline.h#L1-L156)
- [sla_pipeline.h:1-160](file://DllHsBaSlicer/sla_pipeline.h#L1-L160)
- [fdm_pipeline.proto:1-63](file://proto/fdm_pipeline.proto#L1-L63)
- [sla_pipeline.proto:1-67](file://proto/sla_pipeline.proto#L1-L67)

## Performance Considerations
- Avoid repeated allocations by reusing buffers where possible at the application level.
- Prefer batch operations if sending multiple configs/results over the same connection.
- Be mindful of large payloads (e.g., G-code content) when serializing; consider streaming or chunking strategies outside this API.
- Use async pipeline execution APIs to overlap work while serialization occurs.

[No sources needed since this section provides general guidance]

## Troubleshooting Guide
Common issues and resolutions:
- Invalid arguments: Ensure non-null pointers and positive sizes for all input buffers.
- Serialization failures: Verify that the underlying protobuf message can be serialized; check available memory.
- Memory leaks: Always free buffers returned by ToProtoBytes and call the appropriate HsBaFree*ConfigStrings for deserialized configs.
- Error propagation: Inspect error_message fields in results and handle success flags appropriately.

**Section sources**
- [pipeline_convert.cpp:23-99](file://DllHsBaSlicer/pipeline_convert.cpp#L23-L99)
- [pipeline_convert.cpp:103-179](file://DllHsBaSlicer/pipeline_convert.cpp#L103-L179)
- [Msg2PipelineConfig.cpp:27-73](file://convert/Msg2PipelineConfig.cpp#L27-L73)
- [Msg2PipelineConfig.cpp:75-126](file://convert/Msg2PipelineConfig.cpp#L75-L126)

## Conclusion
The Pipeline Conversion API provides a robust, explicit-memory-management bridge between C-compatible pipeline structures and protobuf messages for both FDM and SLA workflows. By following the documented ownership rules and leveraging the provided helpers, applications can reliably serialize and deserialize pipeline configurations and results across process boundaries or languages.