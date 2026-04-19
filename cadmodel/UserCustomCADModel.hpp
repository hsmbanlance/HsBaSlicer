#pragma once
#ifdef HAS_BOOST_DLL


#ifndef HSBA_SLICER_USER_CUSTOM_CAD_MODEL_HPP
#define HSBA_SLICER_USER_CUSTOM_CAD_MODEL_HPP

#include <memory>
#include <unordered_map>

#include "base/IModel.hpp"

namespace HsBa::Slicer
{
	class UserCustomCADModel;
	class IUserCustomCAD
	{
	public:
		typedef IModel* (*CreateModelFunc)();
		typedef void (*DestroyModelFunc)(IModel*);
		typedef bool (*BooleanOperationFunc)(IModel*, const IModel*, const char*);
		typedef IModel* (*CreateBox)(float x, float y, float z);
		typedef IModel* (*CreateSphere)(float radius, int subdivisions);
		typedef IModel* (*CreateCylinder)(float radius, float height, int segments);
		typedef void (*SetThicknessFunc)(IModel*, float thickness);
		virtual ~IUserCustomCAD() = default;
		virtual CreateModelFunc GetCreateModelFunc() const = 0;
		virtual DestroyModelFunc GetDestroyModelFunc() const = 0;
		virtual BooleanOperationFunc GetBooleanOperationFunc() const = 0;
		virtual CreateBox GetCreateBoxFunc() const = 0;
		virtual CreateSphere GetCreateSphereFunc() const = 0;
		virtual CreateCylinder GetCreateCylinderFunc() const = 0;
		virtual SetThicknessFunc GetSetThicknessFunc() const = 0;
	};
	class UserCustomCADDll final : public IUserCustomCAD
	{
	public:
		friend class UserCustomCADModel;
		UserCustomCADDll(std::string_view dllPath, std::string_view addedFunName);
		CreateModelFunc GetCreateModelFunc() const override;
		DestroyModelFunc GetDestroyModelFunc() const override;
		BooleanOperationFunc GetBooleanOperationFunc() const override;
		CreateBox GetCreateBoxFunc() const override;
		CreateSphere GetCreateSphereFunc() const override;
		CreateCylinder GetCreateCylinderFunc() const override;
		SetThicknessFunc GetSetThicknessFunc() const override;
		~UserCustomCADDll();
    private:
		class Impl;
		std::unique_ptr<Impl> impl_;
    };

	class UserCustomCADModel : public IModel
	{
	public:
		UserCustomCADModel() = default;
		~UserCustomCADModel() = default;
		void LoadDll(std::string_view dllPath,std::string_view addedFunName); // load the dll
		void UnloadDll(); // unload the dll
        bool Load(std::string_view fileName) override; // load the model from a file
        bool Save(std::string_view fileName, const ModelFormat format) const override; // save the model to a file

        void Translate(const Eigen::Vector3f& translation) override; // translate the model
        void Rotate(const Eigen::Quaternionf& rotation) override; // rotate the model
        void Scale(const float scale) override;
        void Scale(const Eigen::Vector3f& scale) override; // scale the model
        void Transform(const Eigen::Isometry3f& transform) override; // transform the model
        void Transform(const Eigen::Matrix4f& transform) override; // transform the model
        void Transform(const Eigen::Transform<float, 3, Eigen::Affine>& transform) override; // transform the model
		void BooleanOperation(const UserCustomCADModel& other, const std::string& operation); // boolean operation with another model, operation can be "union", "intersection", "difference"
		void BoundingBox(Eigen::Vector3f& min, Eigen::Vector3f& max) const override; // get the AA bounding box of the model
		float Volume() const override; // get the volume of the model

    private:
		std::shared_ptr<UserCustomCADDll> dll_;
		IModel* model_ = nullptr;
	};
}// namespace HsBa::Slicer

#endif // !HSBA_SLICER_USER_CUSTOM_CAD_MODEL_HPP

#endif // HAS_BOOST_DLL