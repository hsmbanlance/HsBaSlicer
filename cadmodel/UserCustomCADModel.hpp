/** @file UserCustomCADModel.hpp
 * @brief Interface and loader for user-supplied custom CAD models provided by an external DLL/SO.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_NO_DLL_LOADER


#ifndef HSBA_SLICER_USER_CUSTOM_CAD_MODEL_HPP
#define HSBA_SLICER_USER_CUSTOM_CAD_MODEL_HPP

#include <memory>
#include <unordered_map>

#include "base/IModel.hpp"

namespace HsBa::Slicer
{
class UserCustomCADModel;
/**
 * @class IUserCustomCAD
 * @brief Abstract contract a custom CAD library must satisfy to expose model factories and operations.
 *
 * Each getter returns a raw function pointer into the loaded library; the caller owns the lifetime
 * of models created through these functions and must destroy them via the destroy callback.
 */
class IUserCustomCAD
{
public:
    typedef IModel* (*CreateModelFunc)();                                              ///< Create an empty custom model.
    typedef void (*DestroyModelFunc)(IModel*);                                         ///< Destroy a custom model.
    typedef bool (*BooleanOperationFunc)(IModel*, const IModel*, const char*);         ///< Perform a named boolean operation.
    typedef IModel* (*CreateBox)(float x, float y, float z);                           ///< Create a box primitive.
    typedef IModel* (*CreateSphere)(float radius, int subdivisions);                   ///< Create a sphere primitive.
    typedef IModel* (*CreateCylinder)(float radius, float height, int segments);       ///< Create a cylinder primitive.
    typedef void (*SetThicknessFunc)(IModel*, float thickness);                        ///< Set a shell thickness on a model.
    typedef IModel* (*CreatePrismFunc)(HsBaPoly2D_t, HsBaVector3f_t);                   ///< Extrude a polygon into a prism.
    typedef IModel* (*CreatePrismExFunc)(HsBaPolys2D_t, HsBaVector3f_t);                ///< Extrude polygons into a prism.
    virtual CreateModelFunc GetCreateModelFunc() const = 0;                            ///< Retrieve the create-model function.
    virtual DestroyModelFunc GetDestroyModelFunc() const = 0;                          ///< Retrieve the destroy-model function.
    virtual BooleanOperationFunc GetBooleanOperationFunc() const = 0;                  ///< Retrieve the boolean-operation function.
    virtual CreateBox GetCreateBoxFunc() const = 0;                                    ///< Retrieve the box factory.
    virtual CreateSphere GetCreateSphereFunc() const = 0;                              ///< Retrieve the sphere factory.
    virtual CreateCylinder GetCreateCylinderFunc() const = 0;                          ///< Retrieve the cylinder factory.
    virtual SetThicknessFunc GetSetThicknessFunc() const = 0;                          ///< Retrieve the thickness setter.
    virtual CreatePrismFunc GetCreatePrismFunc() const = 0;                            ///< Retrieve the single-polygon extrude factory.
    virtual CreatePrismExFunc GetCreatePrismExFunc() const = 0;                        ///< Retrieve the multi-polygon extrude factory.
};
/**
 * @class UserCustomCADDll
 * @brief Loads a user CAD library (DLL/SO) and resolves the exported factory functions.
 */
class UserCustomCADDll final : public IUserCustomCAD
{
public:
    friend class UserCustomCADModel;
    /// Load the library at dllPath and resolve the entry function named addedFunName.
    UserCustomCADDll(std::string_view dllPath, std::string_view addedFunName);
    CreateModelFunc GetCreateModelFunc() const override;         ///< Retrieve the create-model function.
    DestroyModelFunc GetDestroyModelFunc() const override;       ///< Retrieve the destroy-model function.
    BooleanOperationFunc GetBooleanOperationFunc() const override;  ///< Retrieve the boolean-operation function.
    CreateBox GetCreateBoxFunc() const override;                 ///< Retrieve the box factory.
    CreateSphere GetCreateSphereFunc() const override;           ///< Retrieve the sphere factory.
    CreateCylinder GetCreateCylinderFunc() const override;       ///< Retrieve the cylinder factory.
    SetThicknessFunc GetSetThicknessFunc() const override;       ///< Retrieve the thickness setter.
    CreatePrismFunc GetCreatePrismFunc() const override;         ///< Retrieve the single-polygon extrude factory.
    CreatePrismExFunc GetCreatePrismExFunc() const override;     ///< Retrieve the multi-polygon extrude factory.
    ~UserCustomCADDll();                                         ///< Unload the library.

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @class UserCustomCADModel
 * @brief IModel adapter that delegates to a user-supplied CAD library loaded via UserCustomCADDll.
 */
class UserCustomCADModel : public IModel
{
public:
    UserCustomCADModel() = default;  ///< Construct an empty model with no library attached.
    ~UserCustomCADModel();           ///< Destroy the delegated model and release the library.
    /// Load the CAD library from the given path, resolving the entry function addedFunName.
    void LoadDll(std::string_view dllPath, std::string_view addedFunName);
    /// Unload the currently attached CAD library.
    void UnloadDll();
    /// Load the model from a file via the delegated library.
    bool Load(std::string_view fileName) override;
    /// Save the model to a file in the given format via the delegated library.
    bool Save(std::string_view fileName, const ModelFormat format) const override;

    /// Translate the model by the given vector.
    void Translate(const Eigen::Vector3f& translation) override;
    /// Rotate the model by the given quaternion.
    void Rotate(const Eigen::Quaternionf& rotation) override;
    /// Uniformly scale the model by the given factor.
    void Scale(const float scale) override;
    /// Non-uniformly scale the model by the per-axis factors.
    void Scale(const Eigen::Vector3f& scale) override;
    /// Apply an isometry (rigid) transform to the model.
    void Transform(const Eigen::Isometry3f& transform) override;
    /// Apply a 4x4 homogeneous transform to the model.
    void Transform(const Eigen::Matrix4f& transform) override;
    /// Apply an affine transform to the model.
    void Transform(const Eigen::Transform<float, 3, Eigen::Affine>& transform) override;
    /// Boolean operation with another model; operation is one of "union", "intersection", "difference".
    void BooleanOperation(const UserCustomCADModel& other,
                          const std::string& operation);
    /// Get the axis-aligned bounding box of the model.
    void BoundingBox(Eigen::Vector3f& min,
                     Eigen::Vector3f& max) const override;
    /// Get the signed volume of the model.
    float Volume() const override;
    /// Get the mesh as a (vertices, faces) pair in libigl style.
    std::pair<Eigen::MatrixXf, Eigen::MatrixXi> TriangleMesh() const override;

private:
    std::shared_ptr<UserCustomCADDll> dll_;
    IModel* model_ = nullptr;
};
}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_USER_CUSTOM_CAD_MODEL_HPP

#endif  // !HSBA_NO_DLL_LOADER