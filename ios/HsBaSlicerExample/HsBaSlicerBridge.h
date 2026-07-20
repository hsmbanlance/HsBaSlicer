/**
 * @file HsBaSlicerBridge.h
 * @brief HsBaSlicer iOS 桥接头文件
 *
 * 声明从 C++ 静态库导出的流水线示例入口函数，
 * 供 Swift 通过 Bridging Header 直接调用。
 *
 * 注意：HsBaRunPipelineExamples() 为非生产入口，仅供示例和测试使用。
 */

#ifndef HsBaSlicerBridge_h
#define HsBaSlicerBridge_h

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 运行 FDM / SLA / SLS 三种工艺流水线示例。
 *
 * 内部调用 initialize() 后依次执行 FDM、SLA、SLS 流水线。
 * 仅供示例演示和功能测试，非生产环境实际入口。
 */
void HsBaRunPipelineExamples(void);

#ifdef __cplusplus
}
#endif

#endif /* HsBaSlicerBridge_h */
