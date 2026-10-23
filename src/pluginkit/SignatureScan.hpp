#pragma once

// SignatureScan — ARM64 binary pattern scanning for function address resolution.
// Used as a fallback when direct dlsym() cannot find the symbol names
// (which change between Minecraft Bedrock updates).

namespace dynamicam {

// Call once from RenderHooks::initHooks() if dlsym failed.
void initSignatureScanning();

// Get the cached libminecraftpe.so handle (for PerspectiveSystem accessors)
void *getMCPEHandle();

} // namespace dynamicam
