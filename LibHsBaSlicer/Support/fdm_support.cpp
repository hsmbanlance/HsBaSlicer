#include "fdm_support.hpp"

#include "support/FdmSupport.hpp"
#include "support/SlaSupport.hpp"
#include "support/LuaSupport.hpp"

namespace HsBa::Slicer
{
HSBA_SLICER_LIB_API PolygonsD GenerateFdmSupport(const PolygonsD& current_layer, const PolygonsD& prev_layer,
                                                 float layer_height, const Support::FdmSupportConfig& config)
{
    // 根据配置选择支撑类型
    std::unique_ptr<Support::ISupport> support_gen;
    switch (config.support_pattern)
    {
    case 1:
        support_gen = std::make_unique<Support::FdmTreeSupport>();
        break;
    case 2:
        support_gen = std::make_unique<Support::FdmHoneycombSupport>();
        break;
    default:
        support_gen = std::make_unique<Support::FdmPlaneSupport>();
        break;
    }
    return support_gen->Generate(current_layer, prev_layer, layer_height, config);
}

HSBA_SLICER_LIB_API std::vector<PolygonsD> GenerateAllFdmSupport(const std::vector<PolygonsD>& layers,
                                                                 const Support::FdmSupportConfig& config)
{
    // 根据配置选择支撑类型
    std::unique_ptr<Support::ISupport> support_gen;
    switch (config.support_pattern)
    {
    case 1:
        support_gen = std::make_unique<Support::FdmTreeSupport>();
        break;
    case 2:
        support_gen = std::make_unique<Support::FdmHoneycombSupport>();
        break;
    default:
        support_gen = std::make_unique<Support::FdmPlaneSupport>();
        break;
    }
    return support_gen->GenerateAll(layers, config);
}

HSBA_SLICER_LIB_API std::vector<PolygonsD> GenerateAllSlaSupport(const std::vector<PolygonsD>& layers,
                                                                  const Support::SlaSupportConfig& config)
{
    Support::SlaSacrificialSupport sla_support;
    return sla_support.GenerateAll(layers, config);
}

HSBA_SLICER_LIB_API std::vector<PolygonsD> GenerateAllLuaSupport(const std::vector<PolygonsD>& layers,
                                                                  const Support::SupportConfig& config,
                                                                  std::string_view script,
                                                                  std::string_view functionName)
{
    Support::LuaSupport lua_support(script, functionName);
    return lua_support.GenerateAll(layers, config);
}

}  // namespace HsBa::Slicer
