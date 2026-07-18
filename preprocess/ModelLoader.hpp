#pragma once
#ifndef HSBA_SLICER_MODEL_LOADER_HPP
#define HSBA_SLICER_MODEL_LOADER_HPP

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/IModel.hpp"
#include "base/object_pool.hpp"

#ifdef USE_CGAL
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

private:
    NamedObjectPool<IModel, HSBA_MODEL_POOL_SIZE> pool_;
};
}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_MODEL_LOADER_HPP
