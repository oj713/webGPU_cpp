#pragma once
#include <webgpu/webgpu.h>

/**
 * Utility function to get a WebGPU adapter (adapts device for device hardware capabilities) so that
 *  WGPUAdapter adapter = requestAdapterSync(options);
 * is roughly equivalent to
 *  const adapter = await navigator.gpu.requestAdapter(options);
 */
WGPUAdapter requestAdapterSync(WGPUInstance instance, WGPURequestAdapterOptions const * options);

/** 
 * Utility function to get a WebGPU device, so that
 *  WGPUDevice device = requestDeviceSync(adapter, options);
 * is roughly equivalent to
 *  const device = await adapter.requestDevice(descriptor);
 * similar to requestAdapterSync
 */
WGPUDevice requestDeviceSync(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor);

/** 
 * Inspects the Limits, Features, and Properties of an adapter
 */
void inspectAdapter(WGPUAdapter adapter);

/** 
 * Displays device information
 */
void inspectDevice(WGPUDevice device);