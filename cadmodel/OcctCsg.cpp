/** @file OcctCsg.cpp
 * @brief Implementation of the OCCT DRAW Test Harness bridge for text CSG scripts.
 * @author HsBa
 *
 * This translation unit is the only place in the project that includes Draw headers, so
 * the TKDraw/TKTopTest dependency stays out of the widely included OcctModel.hpp.
 */
#include "OcctCsg.hpp"

#include <string>

#include "base/error.hpp"

#ifdef USE_OCCT_DRAW

#include <algorithm>
#include <mutex>
#include <new>
#include <vector>

#include <BOPTest.hxx>
#include <BRepTest.hxx>
#include <DBRep.hxx>
#include <Draw.hxx>
#include <Draw_Drawable3D.hxx>
#include <Draw_Interpretor.hxx>
#include <Standard_Failure.hxx>

namespace HsBa::Slicer
{
namespace
{
// DRAW keeps one interpreter per process (Draw::GetInterpretor() returns a file scope
// object of TKDraw), and every command group registers itself into it. Tcl interpreters
// are not reentrant, so script evaluation is serialised on this mutex.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) guards the shared interpreter
std::mutex g_csgMutex;

// Tcl_Eval returns TCL_OK, but this file must not include <tcl.h>: TKDraw links Tcl
// privately, so depending on the Tcl header (or import library) here would leak a
// dependency the rest of the project neither sees nor satisfies.
constexpr int kTclOk = 0;

// Shapes live in the process wide DRAW bank, and OCCT exposes no call to remove a name. The
// bank is nonetheless walkable: it is the set Draw::Drawables(), each entry bound to a Tcl
// variable, and Draw::Set() with an empty handle unsets that variable - which is what releases
// the geometry (see tracevar in TKDraw). So the entries that appeared during one call are
// dropped when the call ends.
//
// The baseline is kept here, in C++, deliberately. A previous version stored it in an
// interpreter variable, and a caller controlled shapeName (or any script) could then overwrite
// that variable and turn the cleanup into "unset everything", destroying state belonging to
// later calls. Nothing the bridge needs may live in the namespace scripts run in.
// @pre g_csgMutex is held and declared before this object, so the cleanup still runs inside the
// lock (members are destroyed in reverse declaration order).
class CsgBankScope
{
public:
    CsgBankScope()
    {
        for (const occ::handle<Draw_Drawable3D>& drawable : Draw::Drawables())
        {
            myBefore.push_back(drawable);
        }
    }

    /// Drops the shapes added since construction. Must not throw: it also runs while a CSG
    /// error is propagating. A leftover entry costs memory and isolation, never a wrong return
    /// value, because ShapeFromCsgScript clears the name it reads back before running the script.
    ~CsgBankScope()
    {
        // Copy first: unsetting a variable removes the entry from the map being walked.
        std::vector<occ::handle<Draw_Drawable3D>> now;
        now.reserve(static_cast<size_t>(Draw::Drawables().Extent()));
        for (const occ::handle<Draw_Drawable3D>& drawable : Draw::Drawables())
        {
            now.push_back(drawable);
        }
        for (const occ::handle<Draw_Drawable3D>& drawable : now)
        {
            // An unnamed drawable has no variable to unset, and Draw::Set() would dereference it.
            if (drawable->Name() == nullptr || std::find(myBefore.begin(), myBefore.end(), drawable) != myBefore.end())
            {
                continue;
            }
            try
            {
                Draw::Set(drawable->Name(), occ::handle<Draw_Drawable3D>());
            }
            catch (const Standard_Failure&)  // NOLINT(bugprone-empty-catch) documented above; nothing to recover
            {
            }
        }
    }

    CsgBankScope(const CsgBankScope&) = delete;
    CsgBankScope& operator=(const CsgBankScope&) = delete;

private:
    /// Objects held by reference counting on purpose: the point of the baseline is that the
    /// entry may leave the bank while a shape from an earlier call is still being compared.
    std::vector<occ::handle<Draw_Drawable3D>> myBefore;
};

/// One DRAW command group: a name for diagnostics plus its registration entry point.
struct CommandGroup
{
    const char* name;
    void (*add)(Draw_Interpretor&);
};

// The documented CSG vocabulary: primitives, transforms, bank utilities and booleans.
// BRepTest::AllCommands() would add twenty more groups (curves, surfaces, fillets, sweeps,
// viewer helpers); they stay out so the geometry vocabulary does not grow by accident. This
// is not a sandbox - Init() creates a full Tcl interpreter, whose core commands (exec, open,
// source, ...) are registered by Tcl itself and cannot be limited by choosing groups here.
constexpr CommandGroup g_commandGroups[] = {
    // compound, explode, orientation, nbshapes, check, readbrep/binsave/binrestore
    {.name = "DBRep::BasicCommands", .add = DBRep::BasicCommands},
    // box, wedge, pcylinder, pcone, psphere, ptorus
    {.name = "BRepTest::PrimitiveCommands", .add = BRepTest::PrimitiveCommands},
    // ttranslate, trotate, tscale, tmirror, tcopy, reset
    {.name = "BRepTest::BasicCommands", .add = BRepTest::BasicCommands},
    // lprops, sprops, vprops
    {.name = "BRepTest::GPropCommands", .add = BRepTest::GPropCommands},
    // checkshape, checksection, checkdiff, ...
    {.name = "BRepTest::CheckCommands", .add = BRepTest::CheckCommands},
    // bcut, bfuse, bcommon, bsection and the bop* family
    {.name = "BOPTest::BOPCommands", .add = BOPTest::BOPCommands},
};

std::string FailureText(const Standard_Failure& failure)
{
    // OCCT 8 documents what() as returning "" rather than a null pointer when no message was set.
    const char* const message = failure.what();
    return (message != nullptr && message[0] != '\0') ? message : "no message";
}

/// Register one DRAW command group; empty on success, otherwise the reason it failed.
std::string AddCommandGroup(const CommandGroup& group, Draw_Interpretor& interpretor)
{
    try
    {
        group.add(interpretor);
        return {};
    }
    catch (const Standard_Failure& failure)
    {
        return std::string("DRAW command group ") + group.name + " failed to register: " + FailureText(failure);
    }
    catch (const std::bad_alloc&)
    {
        // Registration itself only raises Standard_Failure; the message building is what can run out
        // of memory here.
        return std::string("DRAW command group ") + group.name + " failed to register: out of memory";
    }
}

/**
 * @brief Create the shared interpreter and register the command groups, once per process.
 * @return Empty on success, otherwise the reason why CSG scripts cannot be evaluated.
 *
 * The reason is returned instead of thrown, and the failure modes OCCT and Tcl produce are
 * captured: DRAW's registration functions flag themselves as done before they register anything,
 * so an exception escaping here would reset the interpreter on the next call (Init() deletes the
 * Tcl interpreter) while the groups silently skip re-registration - leaving an interpreter
 * with no commands and no trace of why. Fixing that would need a retry that cannot work,
 * therefore the first failure is turned into a permanent message.
 * @pre g_csgMutex is held.
 */
const std::string& InterpretorSetup()
{
    static const std::string problem = []
    {
        try
        {
            Draw_Interpretor& interpretor = Draw::GetInterpretor();
            // The default constructor does not create the Tcl interpreter, Init() does. Every
            // call resets it, which also drops previously registered commands - hence once only.
            interpretor.Init();
            for (const CommandGroup& group : g_commandGroups)
            {
                if (std::string reason = AddCommandGroup(group, interpretor); !reason.empty())
                {
                    return reason;
                }
            }
            // Same flag BRepTest::AllCommands sets: shapes are kept topologically, there is no
            // viewer to draw into.
            if (interpretor.Eval("set Draw_TOPOLOGY 1") != kTclOk)
            {
                const char* const message = interpretor.Result();
                return std::string("DRAW interpreter setup failed: ") +
                       (message != nullptr ? message : "no error message");
            }
            return std::string();
        }
        catch (const Standard_Failure& failure)
        {
            return std::string("DRAW interpreter setup failed: ") + FailureText(failure);
        }
        catch (const std::bad_alloc&)
        {
            // Draw/Init/Eval only raise Standard_Failure, so the two catches above cover everything
            // OCCT and Tcl can throw. Nothing may escape, though: a failed static initializer runs the
            // lambda again, and the second Init() would delete the interpreter while the command
            // groups keep their "already done" flag.
            return std::string("DRAW interpreter setup failed: out of memory");
        }
    }();
    return problem;
}
}  // namespace

TopoDS_Shape ShapeFromCsgScript(std::string_view script, std::string_view shapeName)
{
    if (script.empty())
    {
        throw InvalidArgumentError("Empty CSG script");
    }
    if (script.find('\0') != std::string_view::npos || shapeName.find('\0') != std::string_view::npos)
    {
        // Eval() and DBRep::Get() take C strings, so an embedded NUL would silently truncate
        // the script (or the variable name) and hand back the geometry of a partial program.
        throw InvalidArgumentError("CSG script and shape name must not contain a NUL byte");
    }
    const std::string program(script);
    const std::string resultName = shapeName.empty() ? std::string(kCsgDefaultShapeName) : std::string(shapeName);

    std::scoped_lock lock(g_csgMutex);
    if (const std::string& problem = InterpretorSetup(); !problem.empty())
    {
        throw NotSupportedError(problem);
    }
    Draw_Interpretor& interpretor = Draw::GetInterpretor();
    const CsgBankScope bankScope;

    // The scope above drops whatever the previous call left in the bank, but it gives up if a
    // shape could not be unset; publishing an empty shape under resultName makes "this script
    // produced nothing" independent of that, so a stale shape can never be read back.
    DBRep::Set(resultName.c_str(), TopoDS_Shape());

    try
    {
        if (const int rc = interpretor.Eval(program.c_str()); rc != kTclOk)
        {
            const char* const message = interpretor.Result();
            throw InvalidArgumentError(std::string("CSG script failed: ") + (message ? message : "no error message"));
        }
    }
    catch (const Standard_Failure& failure)
    {
        // Commands report malformed arguments by raising instead of returning an error code.
        throw InvalidArgumentError("CSG script failed: " + FailureText(failure));
    }

    // DBRep::Get takes the name by non-const pointer reference (it reports it back on error).
    // Reading before the scope ends is required: the lookup goes through the variable.
    const char* namePtr = resultName.c_str();
    const TopoDS_Shape shape = DBRep::Get(namePtr);
    if (shape.IsNull())
    {
        throw NotFoundError("CSG script produced no shape named '" + resultName + "'");
    }
    // The copy above owns its TShape, so releasing the bank entry on the way out is safe.
    return shape;
}

std::size_t CsgBankExtent()
{
    std::scoped_lock lock(g_csgMutex);
    return static_cast<std::size_t>(Draw::Drawables().Extent());
}
}  // namespace HsBa::Slicer

#else  // !USE_OCCT_DRAW

namespace HsBa::Slicer
{
TopoDS_Shape ShapeFromCsgScript(std::string_view, std::string_view)
{
    throw NotSupportedError(
        "CSG scripts need OpenCASCADE built with the DRAW Test Harness (vcpkg feature opencascade[tcl])");
}

std::size_t CsgBankExtent()
{
    return 0;
}
}  // namespace HsBa::Slicer

#endif  // USE_OCCT_DRAW
