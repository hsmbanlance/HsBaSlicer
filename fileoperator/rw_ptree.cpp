#include "rw_ptree.hpp"

#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "base/encoding_convert.hpp"

namespace HsBa::Slicer::Config
{
	boost::property_tree::ptree from_ini(const std::string& path)
	{
		auto path_loc = utf8_to_local(path);
		boost::property_tree::ptree res;;
		boost::property_tree::read_ini(path_loc, res);
		return std::move(res);
	}
	boost::property_tree::ptree from_xml(const std::string& path)
	{
		auto path_loc = utf8_to_local(path);
		boost::property_tree::ptree res;
		boost::property_tree::read_xml(path_loc, res);
		return std::move(res);
	}
	boost::property_tree::ptree from_json(const std::string& path)
	{
		auto path_loc = utf8_to_local(path);
		boost::property_tree::ptree res;
		boost::property_tree::read_json(path_loc, res);
		return std::move(res);
	}

	void to_ini(const std::string& path, const boost::property_tree::ptree& ptree)
	{
		auto path_loc = utf8_to_local(path);
		boost::property_tree::write_ini(path_loc, ptree);
	}

	void to_xml(const std::string& path, const boost::property_tree::ptree& ptree)
	{
		auto path_loc = utf8_to_local(path);
		boost::property_tree::write_xml(path_loc, ptree);
	}

	void to_json(const std::string& path, const boost::property_tree::ptree& ptree)
	{
		auto path_loc = utf8_to_local(path);
		boost::property_tree::write_json(path_loc, ptree);
	}

}// namespace HsBa::Slicer::Config