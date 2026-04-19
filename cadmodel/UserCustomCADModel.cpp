#ifdef HAS_BOOST_DLL


#include "UserCustomCADModel.hpp"

#include <unordered_map>

#include <boost/dll.hpp>

#include "base/error.hpp"
#include "base/object_pool.hpp"
#include "base/ModelFormat.hpp"

namespace HsBa::Slicer
{
	namespace 
	{
		static NamedObjectPool<UserCustomCADDll, 10> dllPool;
	}
	class UserCustomCADDll::Impl 
	{
	public:
		boost::dll::shared_library dll_;
		std::string addedFunName_;
		Impl(std::string_view dllPath, std::string_view addedFunName)
			: dll_(dllPath.data()), addedFunName_(addedFunName)
		{}
	};
	UserCustomCADDll::UserCustomCADDll(std::string_view dllPath, std::string_view addedFunName)
		: impl_(std::make_unique<Impl>(dllPath, addedFunName))
	{}
	UserCustomCADDll::CreateModelFunc UserCustomCADDll::GetCreateModelFunc() const
	{
		if (impl_->dll_.has(impl_->addedFunName_ + "_create_model"))
		{
			return impl_->dll_.get<CreateModelFunc>(impl_->addedFunName_+"_create_model");
		}
		return nullptr;
	}
	UserCustomCADDll::CreateBox UserCustomCADDll::GetCreateBoxFunc() const
	{
		if (impl_->dll_.has(impl_->addedFunName_+"_create_box"))
		{
			return impl_->dll_.get<CreateBox>(impl_->addedFunName_);
		}
		return nullptr;
	}

	UserCustomCADDll::CreateSphere UserCustomCADDll::GetCreateSphereFunc() const
	{
		if (impl_->dll_.has(impl_->addedFunName_ + "_create_sphere"))
		{
			return impl_->dll_.get<CreateSphere>(impl_->addedFunName_ + "_create_sphere");
		}
		return nullptr;
	}

	UserCustomCADDll::CreateCylinder UserCustomCADDll::GetCreateCylinderFunc() const
	{
		if (impl_->dll_.has(impl_->addedFunName_ + "_create_cylinder"))
		{
			return impl_->dll_.get<CreateCylinder>(impl_->addedFunName_ + "_create_cylinder");
		}
		return nullptr;
	}

	UserCustomCADDll::SetThicknessFunc UserCustomCADDll::GetSetThicknessFunc() const
	{
		if(impl_->dll_.has(impl_->addedFunName_ + "_set_thickness"))
		{
			return impl_->dll_.get<SetThicknessFunc>(impl_->addedFunName_ + "_set_thickness");
		}
		return nullptr;
	}

	UserCustomCADDll::DestroyModelFunc UserCustomCADDll::GetDestroyModelFunc() const
	{
		if (impl_->dll_.has(impl_->addedFunName_ + "_destroy_model"))
		{
			return impl_->dll_.get<DestroyModelFunc>(impl_->addedFunName_+"_destroy_model");
		}
		return nullptr;
	}

	UserCustomCADDll::BooleanOperationFunc UserCustomCADDll::GetBooleanOperationFunc() const
	{
		if (impl_->dll_.has(impl_->addedFunName_ + "_boolean_operation"))
		{
			return impl_->dll_.get<BooleanOperationFunc>(impl_->addedFunName_ + "_boolean_operation");
		}
		return nullptr;
	}


	UserCustomCADDll::~UserCustomCADDll() {}


	void UserCustomCADModel::LoadDll(std::string_view dllPath, std::string_view addedFunName)
	{
		if (dllPool.Contains(dllPath.data()))
		{
			dll_ = dllPool.get(dllPath.data());
		}
		else
		{
			dll_ = dllPool.emplace(dllPath.data(), dllPath, addedFunName);
		}
	}

	void UserCustomCADModel::UnloadDll()
	{
		dll_.reset();
	}

	bool UserCustomCADModel::Load(std::string_view fileName)
	{
		model_ = dll_->GetCreateModelFunc()();
		return model_->Load(fileName);
	}

	bool UserCustomCADModel::Save(std::string_view fileName, const ModelFormat format) const
	{
		if (model_)
		{
			return model_->Save(fileName, format);
		}
		return false;
	}

	void UserCustomCADModel::Translate(const Eigen::Vector3f& translation)
	{
		if(model_)
		{
			model_->Translate(translation);
		}
		else
		{
			throw RuntimeError("Model not loaded");
		}
	}

	void UserCustomCADModel::Rotate(const Eigen::Quaternionf& rotation)
	{
		if(model_)
		{
			model_->Rotate(rotation);
		}
		else
		{
			throw RuntimeError("Model not loaded");
		}
	}

	void UserCustomCADModel::Scale(const float scale)
	{
		if (model_)
		{
			model_->Scale(scale);
		}
		else
		{
			throw RuntimeError("Model not loaded");
		}
	}

	void UserCustomCADModel::Scale(const Eigen::Vector3f& scale)
	{
		if (model_)
		{
			model_->Scale(scale);
		}
		else
		{
			throw RuntimeError("Model not loaded");
		}
	}

	void UserCustomCADModel::Transform(const Eigen::Isometry3f& transform)
	{
		if(model_)
		{
			model_->Transform(transform);
		}
		else
		{
			throw RuntimeError("Model not loaded");
		}
	}

	void UserCustomCADModel::Transform(const Eigen::Matrix4f& transform)
	{
		if (model_)
		{
			model_->Transform(transform);
		}
		else
		{
			throw RuntimeError("Model not loaded");
		}
	}

	void UserCustomCADModel::Transform(const Eigen::Transform<float, 3, Eigen::Affine>& transform)
	{
		if (model_)
		{
			model_->Transform(transform);
		}
		else
		{
			throw RuntimeError("Model not loaded");
		}
	}

	void UserCustomCADModel::BooleanOperation(const UserCustomCADModel& other, const std::string& operation)
	{
		if (model_ && other.dll_ == dll_)
		{
			auto booleanOpFunc = dll_->GetBooleanOperationFunc();
			if (booleanOpFunc)
			{
				if(!booleanOpFunc(model_, other.model_, operation.c_str()))
				{
					throw RuntimeError("Boolean operation failed");
				}
			}
			else
			{
				throw RuntimeError("Boolean operation function not found in DLL");
			}

		}
		else
		{
			throw RuntimeError("Model not loaded or incompatible DLLs");
		}
	}

	void UserCustomCADModel::BoundingBox(Eigen::Vector3f& min, Eigen::Vector3f& max) const
	{
		if(model_)
		{
			model_->BoundingBox(min, max);
		}
		else
		{
			throw RuntimeError("Model not loaded");
		}
	}

	float UserCustomCADModel::Volume() const
	{
		if (model_)
		{
			return model_->Volume();

		}
		else
		{
			throw RuntimeError("Model not loaded");
		}
	}

	// static NamedObjectPool<UserCustomCADDll, 10> dllPool;
}

#endif // HAS_BOOST_DLL