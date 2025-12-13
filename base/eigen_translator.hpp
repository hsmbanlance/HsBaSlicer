#pragma once
#ifndef HSBA_SLICER_EIGEN_TRANSLATOR_HPP
#define HSBA_SLICER_EIGEN_TRANSLATOR_HPP

#include <string>

#include <Eigen/Core>

#include "base_interface.hpp"

namespace HsBa::Slicer::Utils
{
	/**
	 * @brief translate Eigen::Vector2f which used in boost::ptree
	 */
	class EigenVector2fTranslator:public ITranslator<Eigen::Vector2f>
	{
	public:
		std::string put_value(const Eigen::Vector2f& v) override;
		Eigen::Vector2f get_value(const std::string& str) override;
	};
	/**
	 * @brief translate Eigen::Vector2i which used in boost::ptree
	 */
	class EigenVector2iTranslator:public ITranslator<Eigen::Vector2i>
	{
	public:
		std::string put_value(const Eigen::Vector2i& v) override;
		Eigen::Vector2i get_value(const std::string& str) override;
	};
	/**
	 * @brief translate Eigen::Vector2d which used in boost::ptree
	 */
	class EigenVector2dTranslator:public ITranslator<Eigen::Vector2d>
	{
	public:
		std::string put_value(const Eigen::Vector2d& v) override;
		Eigen::Vector2d get_value(const std::string& str) override;
	};
	/**
	 * @brief translate Eigen::Vector2d which used in boost::ptree
	 */
	class EigenVector3fTranslator:public ITranslator<Eigen::Vector3f>
	{
	public:
		std::string put_value(const Eigen::Vector3f& v) override;
		Eigen::Vector3f get_value(const std::string& str) override;
	};
	/**
	 * @brief translate Eigen::Vector3i which used in boost::ptree
	 */
	class EigenVector3iTranslator:public ITranslator<Eigen::Vector3i>
	{
	public:
		std::string put_value(const Eigen::Vector3i& v) override;
		Eigen::Vector3i get_value(const std::string& str) override;
	};
	/**
	 * @brief translate Eigen::Vector3d which used in boost::ptree
	 */
	class EigenVector3dTranslator:public ITranslator<Eigen::Vector3d>
	{
	public:
		std::string put_value(const Eigen::Vector3d& v) override;
		Eigen::Vector3d get_value(const std::string& str) override;
	};
	/**
	 * @brief translate Eigen::Vector4f which used in boost::ptree
	 */
	class EigenVector4fTranslator:public ITranslator<Eigen::Vector4f>
	{
	public:
		std::string put_value(const Eigen::Vector4f& v) override;
		Eigen::Vector4f get_value(const std::string& str) override;
	};
	/**
	 * @brief translate Eigen::Vector4i which used in boost::ptree
	 */
	class EigenVector4iTranslator:public ITranslator<Eigen::Vector4i>
	{
	public:
		std::string put_value(const Eigen::Vector4i& v) override;
		Eigen::Vector4i get_value(const std::string& str) override;
	};
	/**
	 * @brief translate Eigen::Vector4d which used in boost::ptree
	 */
	class EigenVector4dTranslator:public ITranslator<Eigen::Vector4d>
	{
	public:
		std::string put_value(const Eigen::Vector4d& v) override;
		Eigen::Vector4d get_value(const std::string& str) override;
	};
	/**
	 * @brief translate Eigen::VectorXf which used in boost::ptree
	 */
	class EigenVectorXfTranslator:public ITranslator<Eigen::VectorXf>
	{
	public:
		std::string put_value(const Eigen::VectorXf& v) override;
		Eigen::VectorXf get_value(const std::string& str) override;
	};
}// namespace HsBa::Slicer::Utils

#endif // !HSBA_SLICER_EIGEN_TRANSLATOR_HPP
