---
kind: business_term
name: Business Glossary
category: business_term
scope:
    - '**'
---

### spiral_mode
- Definition：工艺流水线中的花瓶/螺旋模式开关（整型配置项）。开启时跳过逐层填充与支撑，将外壁合并为一条随 Z 连续上升的单条路径：FDM 下产出无 `;LAYER:` 标记的连续挤出 G-code；WAAM 下产出一条连续上升焊道；TDP 下追加 `spiral/path.json` 连续路径数据供 Lua 导出脚本消费。默认值为 0，零回归。
- Aliases：螺旋模式、花瓶模式

### Custom 流水线
- Definition：由纯 Lua 脚本驱动的自定义流水线，通过 `custom_pipeline.proto` 定义配置/结果消息，经 `Msg2PipelineConfig` / `PipelineConfig2Msg` 转换后由 Dll 层的 `HsBaCustom*ProtoBytes` C ABI 暴露给外部语言调用。入口函数名可留空以回落默认 `run_pipeline`。
- Aliases：自定义流水线、lua_pipeline

### ParamStore 流水线
- Definition：用于保存和复用工艺参数的流水线，通过 AnyObject + SQLAdapter 设计，注册到 C 导出与 C++ 模块导出中，并提供写入和读取两种操作。
- Aliases：参数存储流水线、param_store

### AnyObject Mock
- Definition：AnyObject 机制上的 Mockit 风格测试桩子系统，仅在检测到常用测试宏或显式宏时启用。支持固定返回桩、抛异常桩、按实参谓词匹配的规则桩（规则优先于普通桩），以及 `Action` / `ArgPredicate` 工厂组合。线程安全，跨 SHARED 库边界各自维护实例。
- Aliases：MockRegistry、打桩

### LuaCommonTypes
- Definition：在 `LibHsBaSlicer` 聚合层注册的常用自定义类型集合，覆盖 Eigen 向量/矩阵/四元数、Clipper2 几何（Point2D/PolygonD/PolygonsD 及其整型变体），提供 `new_<Name>` / `cast_<Name>` 及 table↔AnyObject 双向转换，字段反射支持命名键 `{x=,y=,z=}` 与序列输入。
- Aliases：常用类型注册、common_any_object_types

### repowiki
- Definition：位于 `.qoder/repowiki/` 的中英双语 Wiki 源码目录（`en/` + `zh/`），是 GitHub Wiki 的单一事实来源；由 Qoder IDE 的 Repo Wiki 功能重新生成，再由 `publish-wiki.yml` 工作流发布到 `*.wiki.git` 仓库。
- Aliases：.qoder/repowiki、Wiki 源
