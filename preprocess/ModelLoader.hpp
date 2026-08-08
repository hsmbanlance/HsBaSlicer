#pragma once
#ifndef HSBA_SLICER_MODEL_LOADER_HPP
#define HSBA_SLICER_MODEL_LOADER_HPP

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/IModel.hpp"
#include "base/object_pool.hpp"

#if defined(USE_CGAL) || defined(USE_OPENVDB)
#include <Eigen/Core>
#endif

namespace HsBa::Slicer
{
/** @brief Unified model loader that automatically selects the appropriate
 * kernel (IGL for mesh formats, OCCT for BRep formats) based on file extension.
 *
 * Models are managed through a NamedObjectPool whose capacity is controlled
 * by the compile-time macro HSBA_MODEL_POOL_SIZE (10 when CGAL is available,
 * 1 otherwise).
 */
class ModelLoader
{
public:
    ModelLoader() = default;
    ~ModelLoader() = default;

    ModelLoader(const ModelLoader&) = delete;
    ModelLoader& operator=(const ModelLoader&) = delete;
    ModelLoader(ModelLoader&&) = delete;
    ModelLoader& operator=(ModelLoader&&) = delete;

    /** @brief Load a model from file and store it in the pool.
     *
     * Mesh formats (STL, OBJ, PLY, OFF) are loaded via IglModel.
     * BRep formats (STEP, IGES, VRML, BREP) are loaded via OcctModel
     * (requires USE_OCCT; throws RuntimeError otherwise).
     *
     * @param name A unique name used to retrieve the model later.
     * @param filePath Path to the model file.
     * @return Shared pointer to the loaded IModel.
     * @throws InvalidArgumentError if the name already exists or format is unsupported.
     * @throws RuntimeError if the file cannot be loaded.
     */
    std::shared_ptr<IModel> LoadModel(const std::string& name, std::string_view filePath);

    /** @brief Retrieve a previously loaded model by name.
     * @param name The model name.
     * @return Shared pointer to the model, or nullptr if not found.
     */
    const std::shared_ptr<IModel> GetModel(const std::string& name) const;

    /** @brief Retrieve a model via subscript operator. */
    const std::shared_ptr<IModel> operator[](const std::string& name) const;

    /** @brief Insert a pre-constructed model into the pool.
     * @param name A unique name for the model.
     * @param model Shared pointer to the model (must be a derived IModel).
     * @return The same shared pointer.
     * @throws InvalidArgumentError if the name already exists.
     * @throws RuntimeError if the pool is full.
     */
    std::shared_ptr<IModel> InsertModel(const std::string& name, std::shared_ptr<IModel> model);

    /** @brief Remove a model from the pool. */
    void RemoveModel(const std::string& name);

    /** @brief Check whether a model with the given name exists. */
    bool ContainsModel(const std::string& name) const;

    /** @brief Return the number of models currently in the pool. */
    std::size_t ModelCount() const;

    /** @brief Return the names of all models in the pool. */
    std::vector<std::string> GetModelNames() const;

    /** @brief Clean up models that are no longer referenced externally. */
    std::size_t Cleanup();

#ifdef USE_CGAL
    // ----- Advanced model operations (require CGAL) -----

    /** @brief Perform a thick-solid (shell) operation on an OcctModel.
     * @param sourceName Name of the source model in the pool (must be an OcctModel).
     * @param resultName Name to store the result under.
     * @param thickness Shell thickness (positive = inward).
     * @return Shared pointer to the resulting model.
     * @throws RuntimeError if the source is not an OcctModel.
     */
    std::shared_ptr<IModel> ThickSolidModel(const std::string& sourceName, const std::string& resultName,
                                            float thickness);

    /** @brief Perform a thick-solid operation, excluding specified faces.
     * @param sourceName Name of the source model (must be an OcctModel).
     * @param resultName Name to store the result under.
     * @param closingFaces Faces to leave open (specified as vertex loops).
     * @param thickness Shell thickness.
     * @return Shared pointer to the resulting model.
     */
    std::shared_ptr<IModel> ThickSolidModel(const std::string& sourceName, const std::string& resultName,
                                            const std::vector<std::vector<Eigen::Vector3f>>& closingFaces,
                                            float thickness);

    /** @brief Boolean union of two models. */
    std::shared_ptr<IModel> BooleanUnion(const std::string& leftName, const std::string& rightName,
                                         const std::string& resultName);

    /** @brief Boolean intersection of two models. */
    std::shared_ptr<IModel> BooleanIntersection(const std::string& leftName, const std::string& rightName,
                                                const std::string& resultName);

    /** @brief Boolean difference of two models (left - right). */
    std::shared_ptr<IModel> BooleanDifference(const std::string& leftName, const std::string& rightName,
                                              const std::string& resultName);

    /** @brief Boolean XOR of two models. */
    std::shared_ptr<IModel> BooleanXor(const std::string& leftName, const std::string& rightName,
                                       const std::string& resultName);
#endif

#ifdef USE_OPENVDB
    // ----- Point cloud operations (require OpenVDB) -----

    /** @brief Convert a point cloud model to a triangle mesh via level set reconstruction.
     * @param sourceName Name of the point cloud model in the pool (must be an OpenVdbModel).
     * @param resultName Name to store the resulting mesh model under.
     * @param voxelSize Voxel size for reconstruction (0 = auto estimate).
     * @param particleRadius Particle radius for sphere rasterization (0 = auto estimate).
     * @return Shared pointer to the resulting IglModel mesh.
     * @throws RuntimeError if the source is not an OpenVdbModel.
     */
    std::shared_ptr<IModel> PointCloudToMesh(const std::string& sourceName, const std::string& resultName,
                                             float voxelSize = 0.0f, float particleRadius = 0.0f);

    /** @brief Merge two point cloud models into one.
     * @param leftName Name of the first point cloud.
     * @param rightName Name of the second point cloud.
     * @param resultName Name to store the merged result under.
     * @return Shared pointer to the merged OpenVdbModel.
     * @throws RuntimeError if either source is not an OpenVdbModel.
     */
    std::shared_ptr<IModel> MergePointClouds(const std::string& leftName, const std::string& rightName,
                                             const std::string& resultName);

    /** @brief Downsample a point cloud using voxel grid filtering.
     * @param sourceName Name of the point cloud model.
     * @param resultName Name to store the downsampled result under.
     * @param voxelSize Voxel size for downsampling.
     * @return Shared pointer to the downsampled OpenVdbModel.
     */
    std::shared_ptr<IModel> DownsamplePointCloud(const std::string& sourceName, const std::string& resultName,
                                                 float voxelSize);

    /** @brief Remove statistical outliers from a point cloud.
     * @param sourceName Name of the point cloud model.
     * @param resultName Name to store the filtered result under.
     * @param k Number of neighbors for mean distance estimation.
     * @param multiplier Standard deviation multiplier threshold.
     * @return Shared pointer to the filtered OpenVdbModel.
     */
    std::shared_ptr<IModel> RemovePointCloudOutliers(const std::string& sourceName, const std::string& resultName,
                                                     std::size_t k, float multiplier = 1.0f);

    /** @brief Compute the centroid of a point cloud.
     * @param sourceName Name of the point cloud model.
     * @return The centroid position.
     * @throws RuntimeError if the source is not an OpenVdbModel.
     */
    Eigen::Vector3f PointCloudCentroid(const std::string& sourceName) const;

    /** @brief Compute per-point normals for a point cloud.
     * @param sourceName Name of the point cloud model.
     * @param k Number of neighbors used for normal estimation.
     * @return Nx3 matrix of unit normals.
     * @throws RuntimeError if the source is not an OpenVdbModel.
     */
    Eigen::MatrixXf PointCloudNormals(const std::string& sourceName, std::size_t k = 12) const;

    /** @brief Get the point count of a point cloud model.
     * @param sourceName Name of the point cloud model.
     * @return Number of points.
     */
    std::size_t PointCloudCount(const std::string& sourceName) const;
#endif

private:
    NamedObjectPool<IModel, HSBA_MODEL_POOL_SIZE> pool_;
};
}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_MODEL_LOADER_HPP
