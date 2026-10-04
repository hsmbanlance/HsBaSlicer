/**
 * @file HsBaSlicerBridge.h
 * @brief HsBaSlicer iOS bridging header.
 *
 * Declares the pipeline-example entry function exported from the C++ static library,
 * called directly from Swift via the Bridging Header.
 *
 * Note: HsBaRunPipelineExamples() is not a production entry, for examples and testing only.
 */

#ifndef HsBaSlicerBridge_h
#define HsBaSlicerBridge_h

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Run the FDM / SLA / SLS pipeline examples.
     *
     * Internally calls initialize() and then runs the FDM, SLA, and SLS pipelines in order.
     * For example demonstration and functional testing only; not a real production entry.
     */
    void HsBaRunPipelineExamples(void);

#ifdef __cplusplus
}
#endif

#endif /* HsBaSlicerBridge_h */
