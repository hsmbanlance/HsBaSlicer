# CAD Model Integration

<cite>
**Referenced Files in This Document**
- [OcctModel.hpp](file://cadmodel/OcctModel.hpp)
- [OcctModel.cpp](file://cadmodel/OcctModel.cpp)
- [OcctCsg.hpp](file://cadmodel/OcctCsg.hpp)
- [OcctCsg.cpp](file://cadmodel/OcctCsg.cpp)
- [OcctTypeAliases.hpp](file://cadmodel/OcctTypeAliases.hpp)
- [UserCustomCADModel.hpp](file://cadmodel/UserCustomCADModel.hpp)
- [UserCustomCADModel.cpp](file://cadmodel/UserCustomCADModel.cpp)
- [IModel.hpp](file://base/IModel.hpp)
- [IglModel.hpp](file://meshmodel/IglModel.hpp)
- [model_preprocess.hpp](file://LibHsBaSlicer/Preprocess/model_preprocess.hpp)
- [model_preprocess.cpp](file://LibHsBaSlicer/Preprocess/model_preprocess.cpp)
- [ModelLoader.cpp](file://preprocess/ModelLoader.cpp)
- [README.md](file://docs/en/LibHsBaSlicer/README.md)
</cite>

## Update Summary
**Changes Made**
- Added comprehensive documentation for the new text-based CSG modeling system through OcctCsg.cpp/OcctCsg.hpp
- Updated architecture diagrams to include OCCT DRAW Test Harness integration
- Enhanced parametric operations section with Tcl scripting capabilities
- Added security considerations for unsandboxed Tcl interpreter
- Updated build configuration and dependency information

## Table of Contents
1. [Introduction](#introduction)
2. [Project Structure](#project-structure)
3. [Core Components](#core-components)
4. [Architecture Overview](#architecture-overview)
5. [Detailed Component Analysis](#detailed-component-analysis)
6. [Dependency Analysis](#dependency-analysis)
7. [Performance Considerations](#performance-considerations)
8. [Security Considerations](#security-considerations)
9. [Troubleshooting Guide](#troubleshooting-guide)
10. [Conclusion](#conclusion)
11. [Appendices](#appendices)

## Introduction
This document explains how HsBaSlicer integrates CAD models through OpenCascade (OCCT) using the `OcctModel` class, and how it supports custom CAD implementations through `UserCustomCADModel`. The system now includes a comprehensive text-based CSG modeling system that leverages OCCT's DRAW Test Harness with Tcl scripting support for geometric primitives and boolean operations. It focuses on B-Rep capabilities, precise geometric modeling, parametric operations, conversion between B-Rep and mesh representations, and integration with the slicing pipeline. It also clarifies when to use CAD models versus polygonal meshes, and documents performance considerations for complex geometries.

## Project Structure
The CAD model integration lives primarily under the `cadmodel` directory, while the common model interface is defined in `base`, and the mesh representation used by the slicing pipeline is implemented in `meshmodel`. The preprocessing layer connects user-facing APIs to OCCT-specific operations, including the new text-based CSG functionality.

```mermaid
graph TB
IModel["IModel<br/>Common 3D model interface"] --> OcctModel["OcctModel<br/>OCCT B-Rep implementation"]
IModel --> IglModel["IglModel<br/>libigl triangle mesh implementation"]
IModel --> UserCustomCADModel["UserCustomCADModel<br/>External CAD DLL adapter"]
OcctModel --> |"Load/Save STEP/IGES/VRML/BREP"| FileIO["CAD file formats"]
OcctModel --> |"TriangleMesh()"| IglModel
OcctModel --> |"CreateFromCsg()"| OcctCsg["OcctCsg<br/>Text CSG via OCCT DRAW"]
OcctCsg --> |"Tcl Scripting"| DrawHarness["OCCT DRAW Test Harness"]
Preprocess["Model preprocessing API"] --> OcctModel
Preprocess --> IglModel
Preprocess --> OcctCsg
```

**Diagram sources**
- [IModel.hpp:110-139](file://base/IModel.hpp#L110-L139)
- [OcctModel.hpp:22-124](file://cadmodel/OcctModel.hpp#L22-L124)
- [OcctCsg.hpp:14-82](file://cadmodel/OcctCsg.hpp#L14-L82)
- [IglModel.hpp:18-121](file://meshmodel/IglModel.hpp#L18-L121)
- [UserCustomCADModel.hpp:75-121](file://cadmodel/UserCustomCADModel.hpp#L75-L121)

**Section sources**
- [IModel.hpp:110-139](file://base/IModel.hpp#L110-L139)
- [OcctModel.hpp:22-124](file://cadmodel/OcctModel.hpp#L22-L124)
- [OcctCsg.hpp:14-82](file://cadmodel/OcctCsg.hpp#L14-L82)
- [IglModel.hpp:18-121](file://meshmodel/IglModel.hpp#L18-L121)
- [UserCustomCADModel.hpp:75-121](file://cadmodel/UserCustomCADModel.hpp#L75-L121)

## Core Components
- `IModel`: Abstract interface defining load/save, transforms, bounding box, volume, and triangle mesh access.
- `OcctModel`: OCCT-based B-Rep implementation supporting STEP/IGES/VRML/BREP, boolean operations, thick-solid shell creation, primitives, extrusion from 2D polygons, and text-based CSG modeling.
- `OcctCsg`: Text-based CSG system using OCCT's DRAW Test Harness with Tcl scripting support for geometric primitives and boolean operations.
- `IglModel`: libigl-backed triangle mesh model used by the slicing pipeline.
- `UserCustomCADModel`: Adapter that loads a user-provided CAD library via dynamic loading and delegates model operations to it.

Key responsibilities:
- `OcctModel` provides exact B-Rep geometry, precise volume, robust boolean/shell operations, and text-based CSG capabilities.
- `OcctCsg` enables programmatic generation of B-Rep shapes through Tcl scripts executed in OCCT's DRAW environment.
- `IglModel` provides efficient mesh storage and operations suitable for slicing.
- `UserCustomCADModel` enables pluggable CAD kernels without recompiling the slicer.

**Section sources**
- [IModel.hpp:110-139](file://base/IModel.hpp#L110-L139)
- [OcctModel.hpp:22-124](file://cadmodel/OcctModel.hpp#L22-L124)
- [OcctCsg.hpp:14-82](file://cadmodel/OcctCsg.hpp#L14-L82)
- [IglModel.hpp:18-121](file://meshmodel/IglModel.hpp#L18-L121)
- [UserCustomCADModel.hpp:75-121](file://cadmodel/UserCustomCADModel.hpp#L75-L121)

## Architecture Overview
The architecture separates exact CAD geometry from the mesh-centric slicing pipeline. CAD models can be converted to meshes when needed, but CAD-native operations remain available where precision matters. The new text-based CSG system provides an additional pathway for generating B-Rep shapes programmatically through Tcl scripting.

```mermaid
sequenceDiagram
participant App as "Application"
participant Pre as "Preprocessing API"
participant CAD as "OcctModel"
participant CSG as "OcctCsg"
participant Mesh as "IglModel"
participant Slice as "Slicing Pipeline"
App->>Pre : LoadModel("body", "path.step")
Pre->>CAD : Load(fileName)
CAD-->>Pre : IModel*
App->>Pre : BuildCsgModel("bracket", script)
Pre->>CSG : ShapeFromCsgScript(script)
CSG->>DrawHarness : Execute Tcl commands
DrawHarness-->>CSG : TopoDS_Shape
CSG-->>Pre : TopoDS_Shape
Pre->>CAD : CreateFromCsg(shape)
CAD-->>Pre : IModel*
App->>Slice : RunPipeline(mesh_model)
Pre->>CAD : TriangleMesh()
CAD-->>Mesh : vertices, faces
Mesh-->>Slice : Triangle mesh
```

**Diagram sources**
- [model_preprocess.hpp:244-265](file://LibHsBaSlicer/Preprocess/model_preprocess.hpp#L244-L265)
- [model_preprocess.cpp:327-338](file://LibHsBaSlicer/Preprocess/model_preprocess.cpp#L327-L338)
- [OcctModel.cpp:470-472](file://cadmodel/OcctModel.cpp#L470-L472)
- [OcctCsg.cpp:238-290](file://cadmodel/OcctCsg.cpp#L238-L290)

## Detailed Component Analysis

### OcctModel: OCCT B-Rep Implementation
`OcctModel` wraps an OCCT `TopoDS_Shape` and implements the full `IModel` contract. It supports:
- Loading/saving: STEP, IGES, VRML, BREP.
- Transformations: translation, rotation, uniform/non-uniform scaling, arbitrary affine transforms.
- Geometry queries: axis-aligned bounding box, signed volume.
- Tessellation: conversion to triangle mesh for downstream mesh processing.
- Boolean operations: union, intersection, difference, XOR.
- Shell/thick-solid: inward/outward thickening with optional face exclusion.
- Primitives and extrusions: box, sphere, cylinder, cone, torus, and polygon extrusion into solids.
- **Text-based CSG**: Creation from OCCT DRAW Test Harness scripts via `CreateFromCsg()`.

```mermaid
classDiagram
class IModel {
+Load(fileName) bool
+Save(fileName, format) bool
+Translate(translation) void
+Rotate(rotation) void
+Scale(scale) void
+Scale(vector3f) void
+Transform(isometry) void
+Transform(matrix4f) void
+Transform(affine) void
+BoundingBox(min, max) void
+Volume() float
+TriangleMesh() pair
}
class OcctModel {
+AddShape(model) void
+AddShape(shape) void
+Load(fileName) bool
+Save(fileName, format) bool
+Translate(translation) void
+Rotate(rotation) void
+Scale(scale) void
+Scale(vector3f) void
+Transform(isometry) void
+Transform(matrix4f) void
+Transform(affine) void
+BoundingBox(min, max) void
+Volume() float
+TriangleMesh() pair
+UnionAll() bool
+CreateBox(size) static
+CreateSphere(radius, subdivisions) static
+CreateCylinder(radius, height, segments) static
+CreateCone(radius, height, segments) static
+CreateTorus(majorRadius, minorRadius, majorSegments, minorSegments) static
+CreatePrime(polygon, direction) static
+CreatePrime(polygons, direction) static
+CreateFromCsg(script, shapeName) static
}
class OcctCsg {
+ShapeFromCsgScript(script, shapeName) TopoDS_Shape
+CsgBankExtent() size_t
+HasCsgSupport() bool
}
class IglModel {
+Load(fileName) bool
+Save(fileName, format) bool
+Translate(translation) void
+Rotate(rotation) void
+Scale(scale) void
+Scale(vector3f) void
+Transform(isometry) void
+Transform(matrix4f) void
+Transform(affine) void
+BoundingBox(min, max) void
+Volume() float
+TriangleMesh() pair
+ComputeNormals() void
+ComputeVertexNormals() matrix
+ComputeFaceNormals() matrix
+CreateBox(size) static
+CreateSphere(radius, subdivisions) static
+CreateCylinder(radius, height, segments) static
+CreateCone(radius, height, segments) static
+CreateTorus(majorRadius, minorRadius, majorSegments, minorSegments) static
+CreatePrime(polygon, direction) static
+CreatePrime(polygons, direction) static
}
IModel <|-- OcctModel
IModel <|-- IglModel
OcctModel --> OcctCsg
```

**Diagram sources**
- [IModel.hpp:110-139](file://base/IModel.hpp#L110-L139)
- [OcctModel.hpp:22-124](file://cadmodel/OcctModel.hpp#L22-L124)
- [OcctCsg.hpp:14-82](file://cadmodel/OcctCsg.hpp#L14-L82)
- [IglModel.hpp:18-121](file://meshmodel/IglModel.hpp#L18-L121)

#### B-Rep Capabilities and Precise Modeling
- Exact topology: `TopoDS_Shape` preserves edges, faces, wires, and solids.
- Volume computation uses OCCT mass properties for accurate signed volume.
- Bounding box is computed by iterating over vertices of the shape.
- Tessellation extracts triangulation per face, applying location transformations and orientation handling.

```mermaid
flowchart TD
Start(["OcctModel::TriangleMesh"]) --> IterateFaces["Iterate TopoDS_FACE"]
IterateFaces --> GetTriangulation["Get Poly_Triangulation"]
GetTriangulation --> TriNull{"Triangulation null?"}
TriNull --> |Yes| NextFace["Next face"]
TriNull --> |No| ApplyLoc["Apply location transformation"]
ApplyLoc --> CollectVertices["Collect vertices"]
CollectVertices --> CollectFaces["Collect triangles with orientation fix"]
CollectFaces --> NextFace
NextFace --> Done{"More faces?"}
Done --> |Yes| IterateFaces
Done --> |No| BuildEigen["Build Eigen matrices"]
BuildEigen --> Return(["Return vertices, faces"])
```

**Diagram sources**
- [OcctModel.cpp:355-407](file://cadmodel/OcctModel.cpp#L355-L407)

#### Parametric Operations and Workflows
- Boolean operations: `Union`, `Intersection`, `Difference`, `Xor`.
- Thick-solid shell: global thickness or selective face removal before thickening.
- Primitives: box, sphere, cylinder, cone, torus.
- Extrusion: single closed polygon or multiple polygons (outer contour plus holes).
- **Text-based CSG**: Programmatic shape generation through Tcl scripts with primitives, transforms, and boolean operations.

```mermaid
flowchart TD
Start(["Parametric Operation"]) --> Op{"Operation type"}
Op --> |Boolean| BoolOps["Union/Intersection/Difference/XOR"]
Op --> |ThickSolid| Shell["MakeThickSolidBySimple or ByJoin"]
Op --> |Primitive| Prim["Box/Sphere/Cylinder/Cone/Torus"]
Op --> |Extrude| Extrude["MakePolygon -> MakeFace -> MakePrism"]
Op --> |TextCSG| Csg["DRAW Test Harness Tcl Script"]
BoolOps --> Result["OcctModel result"]
Shell --> Result
Prim --> Result
Extrude --> Result
Csg --> Result
```

**Diagram sources**
- [OcctModel.cpp:451-540](file://cadmodel/OcctModel.cpp#L451-L540)
- [OcctModel.cpp:422-449](file://cadmodel/OcctModel.cpp#L422-L449)
- [OcctModel.cpp:542-663](file://cadmodel/OcctModel.cpp#L542-L663)
- [OcctCsg.cpp:238-290](file://cadmodel/OcctCsg.cpp#L238-L290)

**Section sources**
- [OcctModel.hpp:22-124](file://cadmodel/OcctModel.hpp#L22-L124)
- [OcctModel.cpp:76-190](file://cadmodel/OcctModel.cpp#L76-L190)
- [OcctModel.cpp:213-329](file://cadmodel/OcctModel.cpp#L213-L329)
- [OcctModel.cpp:331-407](file://cadmodel/OcctModel.cpp#L331-L407)
- [OcctModel.cpp:409-663](file://cadmodel/OcctModel.cpp#L409-L663)
- [OcctCsg.cpp:238-290](file://cadmodel/OcctCsg.cpp#L238-L290)

### OcctCsg: Text-Based CSG System
`OcctCsg` provides a bridge to OCCT's DRAW Test Harness, enabling programmatic generation of B-Rep shapes through Tcl scripts. Key features:
- **Tcl Scripting**: Full Tcl interpreter with DRAW command groups for geometry operations.
- **Geometric Primitives**: box, wedge, pcylinder, pcone, psphere, ptorus.
- **Transforms**: ttranslate, trotate, tscale, tmirror, tcopy, reset.
- **Boolean Operations**: bcut, bfuse, bcommon, bsection and the bop* family.
- **Utilities**: compound, explode, orientation, nbshapes, checkshape, vprops.
- **Parameter Substitution**: Tcl set/expr for dynamic parameter values.

```mermaid
sequenceDiagram
participant App as "Application"
participant CSG as "OcctCsg"
participant Tcl as "Tcl Interpreter"
participant Draw as "DRAW Commands"
participant Bank as "DRAW Bank"
App->>CSG : ShapeFromCsgScript(script, shapeName)
CSG->>Tcl : Eval(script)
Tcl->>Draw : Execute primitive/transform/boolean commands
Draw->>Bank : Store shapes in named variables
Tcl-->>CSG : Execution complete
CSG->>Bank : Retrieve shape by name
Bank-->>CSG : TopoDS_Shape
CSG-->>App : Shape object
```

**Diagram sources**
- [OcctCsg.cpp:238-290](file://cadmodel/OcctCsg.cpp#L238-L290)

**Section sources**
- [OcctCsg.hpp:14-82](file://cadmodel/OcctCsg.hpp#L14-L82)
- [OcctCsg.cpp:14-316](file://cadmodel/OcctCsg.cpp#L14-L316)

### UserCustomCADModel: Custom CAD Integration
`UserCustomCADModel` allows users to supply their own CAD kernel via a dynamically loaded library. It exposes factory functions for creating models, primitives, boolean operations, thickness setting, and extrusion.

Key behaviors:
- Dynamic loading of a shared library with a named entry function prefix.
- Function resolution for create/destroy, primitives, boolean operations, thickness, and extrusion.
- Delegation of all `IModel` methods to the underlying custom model instance.
- Safety checks for missing DLL, missing functions, and incompatible DLLs.

```mermaid
sequenceDiagram
participant App as "Application"
participant UCM as "UserCustomCADModel"
participant DLL as "UserCustomCADDll"
participant Ext as "External CAD Library"
App->>UCM : LoadDll(dllPath, addedFunName)
UCM->>DLL : Construct with path and symbol prefix
DLL->>Ext : Resolve symbols (_create_model, _create_box, ...)
App->>UCM : Load(fileName)
UCM->>Ext : create_model()
Ext-->>UCM : IModel*
UCM->>Ext : Load(fileName)
App->>UCM : BooleanOperation(other, "union")
UCM->>Ext : boolean_operation(model, other, "union")
```

**Diagram sources**
- [UserCustomCADModel.hpp:27-73](file://cadmodel/UserCustomCADModel.hpp#L27-L73)
- [UserCustomCADModel.cpp:31-115](file://cadmodel/UserCustomCADModel.cpp#L31-L115)
- [UserCustomCADModel.cpp:134-173](file://cadmodel/UserCustomCADModel.cpp#L134-L173)
- [UserCustomCADModel.cpp:268-289](file://cadmodel/UserCustomCADModel.cpp#L268-L289)

**Section sources**
- [UserCustomCADModel.hpp:27-121](file://cadmodel/UserCustomCADModel.hpp#L27-L121)
- [UserCustomCADModel.cpp:24-149](file://cadmodel/UserCustomCADModel.cpp#L24-L149)
- [UserCustomCADModel.cpp:151-325](file://cadmodel/UserCustomCADModel.cpp#L151-L325)

### Mesh-Based vs B-Rep Approaches
- Mesh-based approach:
  - Represents geometry as vertex/face arrays.
  - Fast for rendering and slicing; widely supported by the pipeline.
  - Limited precision; no exact topology or analytical properties.
- B-Rep approach:
  - Represents geometry with topological entities (vertices, edges, faces, solids).
  - Enables exact boolean operations, shell/thick-solid, and accurate volume.
  - Requires tessellation for mesh-dependent steps like slicing.
- **Text-based CSG approach**:
  - Defines geometry through Tcl scripts executed in OCCT's DRAW environment.
  - Combines B-Rep precision with programmatic flexibility.
  - Supports parameter substitution and complex procedural generation.

When to use:
- Use CAD models (B-Rep) when you need precise geometry, boolean operations, shells, or analytical properties.
- Use text-based CSG when you need programmatic shape generation with parameterization.
- Use polygonal meshes when you only need fast visualization, slicing, or when working with scanned data.

Conversion process:
- `OcctModel::TriangleMesh()` converts B-Rep to mesh for slicing.
- `OcctModel::Save()` can export mesh formats by converting to `IglModel` internally.
- Direct conversion from mesh to `OcctModel` is not currently supported in the loader.

**Section sources**
- [OcctModel.cpp:238-270](file://cadmodel/OcctModel.cpp#L238-L270)
- [OcctModel.cpp:355-407](file://cadmodel/OcctModel.cpp#L355-L407)
- [ModelLoader.cpp:163-167](file://preprocess/ModelLoader.cpp#L163-L167)

### Slicing Pipeline Integration
The slicing pipeline consumes triangle meshes. When CAD models are involved:
- Load CAD files into `OcctModel` or generate via text-based CSG.
- Perform CAD-native operations (boolean, shell, extrusion).
- Convert to mesh via `TriangleMesh()` or save to mesh format.
- Feed the resulting mesh into the slicing pipeline.

```mermaid
flowchart TD
A["Load CAD file or Generate via CSG"] --> B["OcctModel"]
B --> C["CAD operations<br/>boolean/shell/extrusion/text CSG"]
C --> D["TriangleMesh()"]
D --> E["IglModel"]
E --> F["Slicing pipeline"]
```

**Diagram sources**
- [model_preprocess.hpp:244-265](file://LibHsBaSlicer/Preprocess/model_preprocess.hpp#L244-L265)
- [README.md:336-378](file://docs/en/LibHsBaSlicer/README.md#L336-L378)

**Section sources**
- [model_preprocess.hpp:244-265](file://LibHsBaSlicer/Preprocess/model_preprocess.hpp#L244-L265)
- [README.md:336-378](file://docs/en/LibHsBaSlicer/README.md#L336-L378)

## Dependency Analysis
- `OcctModel` depends on OCCT libraries for B-Rep operations and file I/O.
- `OcctCsg` depends on OCCT's DRAW Test Harness (TKDraw/TKTopTest) and Tcl interpreter when available.
- `UserCustomCADModel` depends on Boost.DLL for dynamic loading and resolves exported symbols based on a naming convention.
- Both `OcctModel` and `IglModel` implement `IModel`, enabling polymorphic usage in the preprocessing and slicing layers.
- The preprocessing layer routes thick-solid and boolean operations to appropriate kernels: OCCT for B-Rep, and mesh kernels for polygonal models.

```mermaid
graph LR
IModel["IModel"] --> OcctModel["OcctModel"]
IModel --> IglModel["IglModel"]
IModel --> UserCustomCADModel["UserCustomCADModel"]
OcctModel --> OCCT["OpenCascade (OCCT)"]
OcctModel --> OcctCsg["OcctCsg"]
OcctCsg --> DrawHarness["OCCT DRAW Test Harness"]
DrawHarness --> Tcl["Tcl Interpreter"]
UserCustomCADModel --> BoostDLL["Boost.DLL"]
Preprocess["Preprocessing API"] --> OcctModel
Preprocess --> IglModel
Preprocess --> OcctCsg
```

**Diagram sources**
- [IModel.hpp:110-139](file://base/IModel.hpp#L110-L139)
- [OcctModel.cpp:8-59](file://cadmodel/OcctModel.cpp#L8-L59)
- [OcctCsg.cpp:14-28](file://cadmodel/OcctCsg.cpp#L14-L28)
- [UserCustomCADModel.cpp:12-16](file://cadmodel/UserCustomCADModel.cpp#L12-L16)
- [model_preprocess.hpp:244-265](file://LibHsBaSlicer/Preprocess/model_preprocess.hpp#L244-L265)

**Section sources**
- [OcctModel.cpp:8-59](file://cadmodel/OcctModel.cpp#L8-L59)
- [OcctCsg.cpp:14-28](file://cadmodel/OcctCsg.cpp#L14-L28)
- [UserCustomCADModel.cpp:12-16](file://cadmodel/UserCustomCADModel.cpp#L12-L16)
- [model_preprocess.hpp:244-265](file://LibHsBaSlicer/Preprocess/model_preprocess.hpp#L244-L265)

## Performance Considerations
- Tessellation cost: Converting complex B-Rep to mesh via `TriangleMesh()` can be expensive for large shapes with many faces.
- Boolean operations: OCCT boolean operations may produce complex topologies; consider simplifying inputs or reducing segment counts for primitives.
- Shell operations: `ThickSolid` can generate intricate geometry; limiting excluded faces and using reasonable tolerances helps stability.
- Memory usage: Large CAD models increase memory consumption during both B-Rep and tessellation phases.
- Conversion overhead: Saving CAD models to mesh formats triggers tessellation and mesh construction; batch operations where possible.
- External CAD libraries: Ensure the user-provided DLL is optimized and compatible; mismatched DLLs cause runtime errors.
- **Text CSG performance**: Tcl script execution and DRAW command evaluation add overhead; cache frequently used CSG definitions and avoid excessive script complexity.
- **Interpreter initialization**: The first CSG script call incurs interpreter setup costs; subsequent calls benefit from cached state.

## Security Considerations
**Updated** Added security warnings for text-based CSG functionality due to unsandboxed Tcl interpreter.

The text-based CSG system executes Tcl scripts in a full Tcl interpreter that is **not sandboxed**. This means:
- Scripts have access to system commands (`exec`, `open`, `source`, `file`).
- Scripts can read/write files and execute arbitrary code.
- Only trusted scripts should be passed to the CSG system.
- User-supplied CSG scripts must be validated and sanitized before execution.
- Consider implementing input validation and script whitelisting in application layers.

**Section sources**
- [OcctCsg.hpp:31-36](file://cadmodel/OcctCsg.hpp#L31-L36)
- [OcctCsg.cpp:31-36](file://cadmodel/OcctCsg.cpp#L31-L36)

## Troubleshooting Guide
Common issues and resolutions:
- Unsupported model format:
  - Occurs when attempting to load/save formats not handled by `OcctModel`.
  - Resolution: Use supported formats (STEP, IGES, VRML, BREP) or convert to mesh formats via `IglModel`.
- Direct mesh-to-CAD conversion unsupported:
  - The loader throws when trying to convert mesh models to `OcctModel`.
  - Resolution: Use CAD-native sources for boolean/shell operations.
- Missing DLL or functions:
  - `UserCustomCADModel` throws if the DLL is not loaded or required symbols are missing.
  - Resolution: Verify DLL path, symbol naming convention, and ensure the external library exports the expected functions.
- Incompatible DLLs:
  - Boolean operations require both models to share the same DLL.
  - Resolution: Use models created by the same external CAD library.
- **Text CSG unavailable**:
  - CSG scripts fail when OpenCASCADE is built without the DRAW module.
  - Resolution: Rebuild OpenCASCADE with the 'tcl' feature or configure with `HSBA_REQUIRE_TEXT_CSG=OFF`.
- **Malformed CSG script**:
  - Invalid Tcl syntax or unknown DRAW commands cause script execution failures.
  - Resolution: Validate script syntax and ensure all referenced shapes exist.
- **Missing shape reference**:
  - Script references a shape variable that was never defined.
  - Resolution: Check shape names and ensure proper sequence of operations.

**Section sources**
- [OcctModel.cpp:213-270](file://cadmodel/OcctModel.cpp#L213-L270)
- [ModelLoader.cpp:163-167](file://preprocess/ModelLoader.cpp#L163-L167)
- [UserCustomCADModel.cpp:151-173](file://cadmodel/UserCustomCADModel.cpp#L151-L173)
- [UserCustomCADModel.cpp:268-289](file://cadmodel/UserCustomCADModel.cpp#L268-L289)
- [OcctCsg.cpp:238-290](file://cadmodel/OcctCsg.cpp#L238-L290)

## Conclusion
HsBaSlicer's CAD integration centers on `OcctModel` for precise B-Rep modeling and `UserCustomCADModel` for extensible CAD kernels. The addition of the text-based CSG system through `OcctCsg` provides powerful programmatic shape generation capabilities using OCCT's DRAW Test Harness. The system bridges exact CAD geometry with the mesh-centric slicing pipeline through controlled conversions. For workflows requiring precision, boolean operations, shell generation, or programmatic shape definition, prefer CAD models; for fast slicing and visualization, use polygonal meshes. Careful management of complexity, conversion steps, and security considerations ensures reliable performance even with intricate geometries.

## Appendices

### Example CAD Operations
- Boolean union/difference/intersection/XOR between two `OcctModel` instances.
- Creating a shell around a solid with optional face exclusions.
- Building primitives and extruding 2D polygons (including holes) into 3D solids.
- Loading STEP/IGES/VRML/BREP files and saving to mesh formats via internal conversion.
- **Text-based CSG**: Generating shapes through Tcl scripts with primitives, transforms, and boolean operations.

**Section sources**
- [OcctModel.cpp:451-663](file://cadmodel/OcctModel.cpp#L451-L663)
- [OcctModel.cpp:213-270](file://cadmodel/OcctModel.cpp#L213-L270)
- [OcctCsg.cpp:238-290](file://cadmodel/OcctCsg.cpp#L238-L290)

### Parametric Modeling Workflow
- Start with 2D polygons representing outer contours and holes.
- Create a face from the outer wire and add hole wires.
- Extrude the face along a direction to form a solid.
- Optionally apply boolean operations or shell thickness.
- **Text-based CSG workflow**: Define shapes using Tcl scripts with parameter substitution, then execute to generate B-Rep geometry.

**Section sources**
- [OcctModel.cpp:542-663](file://cadmodel/OcctModel.cpp#L542-L663)
- [OcctCsg.hpp:22-36](file://cadmodel/OcctCsg.hpp#L22-L36)

### Custom CAD Model Implementation
- Implement the required factory functions in an external library with a consistent symbol prefix.
- Export functions for model creation/destruction, primitives, boolean operations, thickness, and extrusion.
- Load the library via `UserCustomCADModel::LoadDll` and use standard `IModel` methods.

**Section sources**
- [UserCustomCADModel.hpp:27-73](file://cadmodel/UserCustomCADModel.hpp#L27-L73)
- [UserCustomCADModel.cpp:31-115](file://cadmodel/UserCustomCADModel.cpp#L31-L115)

### Text-Based CSG Examples
- Basic primitives: box, sphere, cylinder, cone, torus.
- Transformations: translation, rotation, scaling, mirroring.
- Boolean operations: union, intersection, difference.
- Parameter substitution using Tcl variables and expressions.
- Complex procedural generation combining multiple operations.

**Section sources**
- [OcctCsg.hpp:22-36](file://cadmodel/OcctCsg.hpp#L22-L36)
- [OcctCsg.cpp:140-153](file://cadmodel/OcctCsg.cpp#L140-L153)

### Build Configuration
- Text CSG requires OpenCASCADE built with the 'tcl' feature (vcpkg feature `opencascade[tcl]`).
- Configure with `HSBA_REQUIRE_TEXT_CSG=ON` (default) to fail configuration if CSG is unavailable.
- Configure with `HSBA_REQUIRE_TEXT_CSG=OFF` to build without text CSG capability.
- Static builds may require additional Tcl runtime initialization (`HSBA_OCCT_STATIC_TCL`).

**Section sources**
- [cadmodel/CMakeLists.txt:26-70](file://cadmodel/CMakeLists.txt#L26-L70)
- [OcctCsg.hpp:67-81](file://cadmodel/OcctCsg.hpp#L67-L81)