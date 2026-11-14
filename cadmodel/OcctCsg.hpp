/** @file OcctCsg.hpp
 * @brief Build OpenCascade B-Rep shapes from OCCT DRAW Test Harness scripts (text CSG).
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_OCCTCSG_HPP
#define HSBA_SLICER_OCCTCSG_HPP

#include <cstddef>
#include <string_view>

#include <TopoDS_Shape.hxx>

namespace HsBa::Slicer
{
/// DRAW variable ShapeFromCsgScript reads back when the caller does not name one.
inline constexpr std::string_view kCsgDefaultShapeName = "result";

/**
 * @brief Evaluate an OCCT DRAW script and return the shape it leaves in the DRAW bank.
 *
 * The script is plain DRAW Test Harness text: one command per line, Tcl `set`/`expr`
 * available. The registered geometry vocabulary is the one a CSG description needs -
 * primitives (box, wedge, pcylinder, pcone, psphere, ptorus), transforms (ttranslate,
 * trotate, tscale, ...), bank utilities (compound, explode, orientation, nbshapes,
 * checkshape, vprops) and the boolean operations `bcut`/`bfuse`/`bcommon`/`bsection`. The
 * shape produced by the last command naming @p shapeName is what is returned; boolean
 * commands wrap their result in a compound, so callers needing a single solid have to
 * explore for a `TopAbs_SOLID` child (or run `explode` in the script).
 *
 * A script runs in a full Tcl interpreter: the DRAW command groups chosen here only limit
 * the geometry vocabulary, not the language. Tcl registers `exec`, `open`, `source`,
 * `file` and friends when the interpreter is created, and `readbrep`/`binsave`/`binrestore`
 * are available as well, so a script is equivalent to a local shell session and must come
 * from a trusted source. The viewer and the STEP/IGES/mesh exchanges behind `restore` and
 * `saveas` are not registered.
 *
 * The bank is shared by the whole process, and OCCT exposes no API to erase a name, so each
 * call is made hermetic instead: the shape objects the script adds are unbound when it returns,
 * which is also what releases them (DRAW keeps shapes in Tcl variables whose unset trace drops
 * them from the global drawable set). A script therefore cannot read a shape an earlier call
 * left behind, and calling with fresh names does not grow memory without bound. The bridge
 * keeps no state of its own in the interpreter, so no script can reach that cleanup; plain Tcl
 * variables a script creates for itself (via `set`, as opposed to shapes) do stay in the
 * interpreter for the life of the process, which is why CSG descriptions should reuse names.
 *
 * @param script DRAW commands, lines separated by '\\n'; must not contain a NUL byte.
 * @param shapeName DRAW variable to read back; empty selects kCsgDefaultShapeName.
 * @return The resulting shape.
 * @throws InvalidArgumentError If @p script is empty, holds a NUL byte, or a command reports
 *         an error.
 * @throws NotFoundError If the script leaves no shape under @p shapeName.
 * @throws NotSupportedError If the build has no DRAW Test Harness (opencascade[tcl]), or a
 *         command group of it failed to register.
 */
TopoDS_Shape ShapeFromCsgScript(std::string_view script, std::string_view shapeName = {});

/**
 * @brief Number of shapes currently held by the process wide DRAW bank.
 *
 * Diagnostic for the isolation ShapeFromCsgScript promises: it stays flat across calls because
 * every call releases the shapes it added. Zero when the build has no DRAW Test Harness.
 */
std::size_t CsgBankExtent();

/**
 * @brief Report whether this build can evaluate CSG scripts.
 *
 * False when OpenCASCADE was built without the Draw module, in which case
 * ShapeFromCsgScript throws NotSupportedError. Builds that must not ship without text CSG
 * should keep HSBA_REQUIRE_TEXT_CSG=ON, which fails the CMake configuration instead of
 * letting this flag quietly turn false.
 */
constexpr bool HasCsgSupport()
{
#ifdef USE_OCCT_DRAW
    return true;
#else
    return false;
#endif
}
}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_OCCTCSG_HPP
