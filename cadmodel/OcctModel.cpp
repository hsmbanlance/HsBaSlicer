#include "OcctModel.hpp"

#include <Standard_Failure.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <gp_Quaternion.hxx>
#include <TopoDS.hxx>
#include <TopTools_ListOfShape.hxx>
#include <BRepBndLib.hxx> 

#include <BRepBuilderAPI_Transform.hxx>
#include <BRepAlgoAPI_BooleanOperation.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Common.hxx>

#include <StepData_ReadWriteModule.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_Reader.hxx> 
#include <IGESControl_Reader.hxx>
#include <STEPControl_Writer.hxx>
#include <IGESControl_Writer.hxx>
#include <BRepTools.hxx>

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <Poly_Triangulation.hxx>

#include <TopExp_Explorer.hxx>

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepExtrema_DistShapeShape.hxx> 

#include "base/error.hpp"
#include "base/encoding_convert.hpp"
#include "meshmodel/IglModel.hpp"
#include <BRepAdaptor_Surface.hxx>

namespace HsBa::Slicer
{
	OcctModel::OcctModel(const TopoDS_Shape& shape)
	{
		// Directly store the incoming shape. Avoid using BRep_Builder::Add on an
		// uninitialized `shape_` which can lead to memory access violations.
		shape_ = shape;
	}

	OcctModel::OcctModel(TopoDS_Shape&& shape)
	{
		// Move the provided shape into the member.
		shape_ = std::move(shape);
	}

	void OcctModel::ReadStep(const std::string& path)
	{
		STEPControl_Reader reader;
		IFSelect_ReturnStatus status = reader.ReadFile(path.c_str());
		if (status == IFSelect_RetFail)
		{
			throw IOError("Read Step File Error");
		}

		reader.TransferRoot();
		BRep_Builder builder;
		shape_.Nullify();
		for (int i = 1; i <= reader.NbShapes(); ++i)
		{
			builder.Add(shape_, reader.Shape(i));
		}
	}
	void OcctModel::ReadIGES(const std::string& path)
	{
		IGESControl_Reader reader;
		IFSelect_ReturnStatus status = reader.ReadFile(path.c_str());
		if (status == IFSelect_RetFail)
		{
			throw IOError("Read IGES File Error");
		}

		BRep_Builder builder;
		shape_.Nullify();
		for (int i = 1; i <= reader.NbShapes(); ++i)
		{
			builder.Add(shape_, reader.Shape(i));
		}
	}
	bool OcctModel::WriteStep(const std::string& path) const
	{
		STEPControl_Writer writer;
		writer.Transfer(shape_, STEPControl_AsIs);
		auto succeed = writer.Write(path.c_str());
		if (succeed == IFSelect_RetFail)
		{
			throw IOError("Write Step Error");
		}
		return succeed != IFSelect_RetDone;
	}
	bool OcctModel::WriteIGES(const std::string& path) const
	{
		IGESControl_Writer writer;
		writer.AddShape(shape_);
		return writer.Write(path.c_str());
	}

	void OcctModel::AddShape(const OcctModel& o)
	{
		BRep_Builder builder;
		builder.Add(shape_, o.shape_);
	}
	void OcctModel::AddShape(OcctModel&& o)
	{
		BRep_Builder builder;
		builder.Add(shape_, std::move(o.shape_));
	}
	void OcctModel::AddShape(const TopoDS_Shape& o)
	{
		BRep_Builder builder;
		builder.Add(shape_, o);
	}
	void OcctModel::AddShape(TopoDS_Shape&& o)
	{
		BRep_Builder builder;
		builder.Add(shape_, std::move(o));
	}

	bool OcctModel::Load(std::string_view fileName)
	{
		fileName_ = std::string{ fileName };
		ModelFormat format = ModelTypeFromExtName(fileName_);
		auto path = utf8_to_local(fileName_);
		switch (format)
		{
		case HsBa::Slicer::ModelFormat::STEP:
			ReadStep(path);
			break;
		case HsBa::Slicer::ModelFormat::IGES:
			ReadIGES(path);
			break;
		default:
			throw NotSupportedError("Unsupported Model Format");
			break;
		}
		return !shape_.IsNull();
	}
	bool OcctModel::Save(std::string_view fileName, const ModelFormat format) const
	{
		auto path = utf8_to_local(std::string{fileName});
		Standard_Boolean succeed = false;
		switch (format)
		{
		case HsBa::Slicer::ModelFormat::STEP:
			succeed = WriteStep(path);
			break;
		case HsBa::Slicer::ModelFormat::IGES:
			succeed = WriteIGES(path);
			break;
		default:
			if (IsMeshFormat(format))
			{
				if (shape_.IsNull())
				{
					return false;
				}
				auto [v,f] = TriangleMesh();
				IglModel mesh_model(v, f);
				return mesh_model.Save(fileName, format);
			}
			throw NotSupportedError("Unsupported Model Format");
			break;
		}
		return succeed;
	}
	void OcctModel::Translate(const Eigen::Vector3f& translation)
	{
		gp_Trsf tran;
		gp_Vec vec(translation.x(), translation.y(), translation.z());
		tran.SetTranslation(vec);
		BRepBuilderAPI_Transform transform(shape_, tran);
		shape_ = transform.Shape();
	}
	void OcctModel::Rotate(const Eigen::Quaternionf& rotation)
	{
		gp_Trsf tran;
		gp_Quaternion q{ rotation.x(), rotation.y(), rotation.z(), rotation.w() };
		tran.SetRotation(q);
		BRepBuilderAPI_Transform transform(shape_, tran);
		shape_ = transform.Shape();
	}
	void OcctModel::Scale(const float scale)
	{
		gp_Trsf tran;
		tran.SetScale(gp_Pnt{}, scale);
		BRepBuilderAPI_Transform transform(shape_, tran);
		shape_ = transform.Shape();
	}
	void OcctModel::Scale(const Eigen::Vector3f& scale)
	{
		gp_Trsf tran;
		tran.SetValues(scale.x(), 0, 0, 0,
			0, scale.y(), 0, 0,
			0, 0, scale.z(), 0);
		BRepBuilderAPI_Transform transform(shape_, tran);
		shape_ = transform.Shape();
	}
	void OcctModel::Transform(const Eigen::Isometry3f& transform)
	{
		gp_Trsf tran;
		tran.SetValues(transform(0, 0), transform(0, 1), transform(0, 2), transform(0, 3),
			transform(1, 0), transform(1, 1), transform(1, 2), transform(1, 3),
			transform(2, 0), transform(2, 1), transform(2, 2), transform(2, 3));
		BRepBuilderAPI_Transform transformer(shape_, tran);
		shape_ = transformer.Shape();
	}
	void OcctModel::Transform(const Eigen::Matrix4f& transform)
	{
		gp_Trsf tran;
		tran.SetValues(transform(0, 0), transform(0, 1), transform(0, 2), transform(0, 3),
			transform(1, 0), transform(1, 1), transform(1, 2), transform(1, 3),
			transform(2, 0), transform(2, 1), transform(2, 2), transform(2, 3));
		BRepBuilderAPI_Transform transformer(shape_, tran);
		shape_ = transformer.Shape();
	}
	void OcctModel::Transform(const Eigen::Transform<float, 3, Eigen::Affine>& transform)
	{
		gp_Trsf tran;
		tran.SetValues(transform.matrix()(0, 0), transform.matrix()(0, 1), transform.matrix()(0, 2), transform.matrix()(0, 3),
			transform.matrix()(1, 0), transform.matrix()(1, 1), transform.matrix()(1, 2), transform.matrix()(1, 3),
			transform.matrix()(2, 0), transform.matrix()(2, 1), transform.matrix()(2, 2), transform.matrix()(2, 3));
		BRepBuilderAPI_Transform transformer(shape_, tran);
		shape_ = transformer.Shape();
	}

	void OcctModel::BoundingBox(Eigen::Vector3f& min, Eigen::Vector3f& max) const
	{
		if (shape_.IsNull())
		{
			return;
		}
		min = Eigen::Vector3f(FLT_MAX, FLT_MAX, FLT_MAX);
		max = Eigen::Vector3f(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		for (TopExp_Explorer exp(shape_, TopAbs_VERTEX); exp.More(); exp.Next())
		{
			const TopoDS_Vertex& vertex = TopoDS::Vertex(exp.Current());
			BRep_Tool tool;
			const gp_Pnt& pnt = tool.Pnt(vertex);
			min.x() = std::min(min.x(), static_cast<float>(pnt.X()));
			min.y() = std::min(min.y(), static_cast<float>(pnt.Y()));
			min.z() = std::min(min.z(), static_cast<float>(pnt.Z()));
			max.x() = std::max(max.x(), static_cast<float>(pnt.X()));
			max.y() = std::max(max.y(), static_cast<float>(pnt.Y()));
			max.z() = std::max(max.z(), static_cast<float>(pnt.Z()));
		}
	}

	std::pair<Eigen::MatrixXf, Eigen::MatrixXi> OcctModel::TriangleMesh() const
	{
		std::vector<Eigen::Vector3f> vertices;
		std::vector<Eigen::Vector3i> faces;
		for (TopExp_Explorer anExpSF(shape_, TopAbs_FACE); anExpSF.More(); anExpSF.Next()) 
		{
			const int aNodeOffset = static_cast<int>(vertices.size());
			const TopoDS_Shape& aFace = anExpSF.Current();
			TopLoc_Location aLoc;
			Handle(Poly_Triangulation) aTriangulation = BRep_Tool::Triangulation(TopoDS::Face(aFace), aLoc);
			if (aTriangulation.IsNull())
			{
				continue;
			}
			gp_Trsf aTrsf = aLoc.Transformation();
			for (Standard_Integer aNodeIter = 1; aNodeIter <= aTriangulation->NbNodes(); ++aNodeIter) 
			{
				gp_Pnt aPnt = aTriangulation->Node(aNodeIter);
				aPnt.Transform(aTrsf);
				vertices.push_back({ static_cast<float>(aPnt.X()), static_cast<float>(aPnt.Y()), static_cast<float>(aPnt.Z()) });
			}
			const TopAbs_Orientation anOrientation = anExpSF.Current().Orientation();
			for (Standard_Integer aTriIter = 1; aTriIter <= aTriangulation->NbTriangles(); ++aTriIter) {
				Poly_Triangle aTri = aTriangulation->Triangle(aTriIter);

				Standard_Integer anId[3]{ 0,0,0 };
				aTri.Get(anId[0], anId[1], anId[2]);
				if (anOrientation == TopAbs_REVERSED)
					std::swap(anId[1], anId[2]);
				faces.push_back({ anId[0] - 1 + aNodeOffset,
				   anId[1] - 1 + aNodeOffset,
				   anId[2] - 1 + aNodeOffset });
			}

		}
		Eigen::MatrixXf v;
		Eigen::MatrixXi f;
		v.resize(vertices.size(), 3);
		f.resize(faces.size(), 3);
		for (std::size_t i = 0; i != vertices.size(); ++i)
		{
			v.row(i) = vertices[i];
		}
		for (std::size_t i = 0; i != faces.size(); ++i)
		{
			f.row(i) = faces[i];
		}
		return std::make_pair(v, f);
	}

	float OcctModel::Volume() const
	{
		GProp_GProps props;
		BRepGProp::VolumeProperties(shape_, props);
		return static_cast<float>(props.Mass());
	}

	bool OcctModel::UnionAll()
	{
		TopoDS_Shape shape;
		for (TopExp_Explorer anExpSolid(shape_, TopAbs_SOLID); anExpSolid.More(); anExpSolid.Next())
		{
			const TopoDS_Shape& aSolid = anExpSolid.Value();
			BRepAlgoAPI_Fuse fuse(shape, aSolid);
			shape = fuse.Shape();
		}
		shape_ = std::move(shape);
		return !shape_.IsNull();
	}

	OcctModel OcctModel::CreateBox(const Eigen::Vector3f& size)
	{
		gp_Pnt o1{ 0,0,0 };
		BRepPrimAPI_MakeBox maker{ o1, size.x(),size.y(),size.z() };
		return OcctModel(maker.Shape());
	}
	OcctModel OcctModel::CreateSphere(const float radius, const int subdivisions)
	{
		gp_Pnt center{ 0,0,0 };
		BRepPrimAPI_MakeSphere maker{ center, radius };
		return OcctModel(maker.Shape());
	}
	OcctModel OcctModel::CreateCylinder(const float radius, const float height, const int segments)
	{
		BRepPrimAPI_MakeCylinder maker{ radius, height };
		return OcctModel(maker.Shape());
	}
	OcctModel OcctModel::CreateCone(const float radius, const float height, const int segments)
	{
		BRepPrimAPI_MakeCone maker{ radius, 0.0f, height };
		return OcctModel(maker.Shape());
	}
	OcctModel OcctModel::CreateTorus(const float majorRadius, const float minorRadius, const int majorSegments, const int minorSegments)
	{
		BRepPrimAPI_MakeTorus maker{ majorRadius, minorRadius };
		return OcctModel(maker.Shape());
	}

	OcctModel Union(const OcctModel& left, const OcctModel& right)
	{
		BRepAlgoAPI_Fuse fuse(left.shape_, right.shape_);
		return OcctModel(fuse.Shape());
	}
	OcctModel Intersection(const OcctModel& left, const OcctModel& right)
	{
		BRepAlgoAPI_Common common(left.shape_, right.shape_);
		return OcctModel(common.Shape());
	}
	OcctModel Difference(const OcctModel& left, const OcctModel& right)
	{
		BRepAlgoAPI_Cut cut(left.shape_, right.shape_);
		return OcctModel(cut.Shape());
	}
	OcctModel Xor(const OcctModel& left, const OcctModel& right)
	{
		return Difference(Union(left, right), Intersection(left, right));
	}

	OcctModel ThickSolid(const OcctModel& model, float thickness)
	{
		BRepOffsetAPI_MakeThickSolid maker;
		maker.MakeThickSolidBySimple(model.shape_, thickness);
		return OcctModel(maker.Shape());
	}
	OcctModel ThickSolid(const OcctModel& model, const std::vector<std::vector<Eigen::Vector3f>>& faces, float thickness)
	{
		constexpr double tolerance = 1e-6;
		//make in faces to TopoDS_Face vector
		std::vector<TopoDS_Face> faces_waiter;
		for (const auto& f : faces)
		{
			if(f.size() < 3)
			{
				continue;
			}
			BRepBuilderAPI_MakeWire wire_maker;
			gp_Pnt p1{ f[0].x(),f[0].y(),f[0].z() };
			for (size_t i = 1; i != faces.size(); ++i)
			{
				gp_Pnt p2{ f[i].x(),f[i].y(),f[i].z() };
				BRepBuilderAPI_MakeEdge edge_maker{ p1,p2 };
				wire_maker.Add(edge_maker.Edge());
			}
			BRepBuilderAPI_MakeFace face_maker{ wire_maker.Wire() };
			faces_waiter.emplace_back(face_maker.Face());
		}
		const auto faces_close = [tolerance](const TopoDS_Face& l, const TopoDS_Face& r) {
			if(l.IsNull() || r.IsNull())
			{
				return false;
			}	
			//Get Distance
			BRepExtrema_DistShapeShape distSS(l, r);
			if (!distSS.Perform()) {
				return false;
			}
			double distance = distSS.Value();
			const auto& trsf_l = l.Location().Transformation();
			const auto& trsf_r = r.Location().Transformation();
			Standard_Real trace_l = trsf_l.Value(0, 0) + trsf_l.Value(1, 1) + trsf_l.Value(2, 2);
			Standard_Real trace_r = trsf_r.Value(0, 0) + trsf_r.Value(1, 1) + trsf_r.Value(2, 2);
			return distance <= tolerance && abs(trace_l - trace_r) <= tolerance;
			};
		//add faces into TopTools_ListOfShape
		TopTools_ListOfShape remove_shape;
		BRepOffsetAPI_MakeThickSolid maker;		
		for (TopExp_Explorer anExpSF(model.shape_, TopAbs_FACE); anExpSF.More(); anExpSF.Next())
		{
			const TopoDS_Face& face = TopoDS::Face(anExpSF.Current());
			if (face.IsNull())
			{
				continue;
			}
			if (false)
			{
				remove_shape.Append(face);
			}
			if (std::find_if(faces_waiter.begin(), faces_waiter.end(), [&face, &faces_close](const auto& f) {
				return faces_close(f, face);
				}) != faces_waiter.end())
			{
				remove_shape.Append(face);
			}
		}
		maker.MakeThickSolidByJoin(model.shape_, remove_shape, thickness, tolerance);
		return OcctModel(maker.Shape());
	}
}// namespace HsBa::Slicer

std::size_t std::hash<HsBa::Slicer::OcctModel>::operator()(const HsBa::Slicer::OcctModel& model) const noexcept
{
	return std::hash<TopoDS_Shape>{}(model.shape_);
}