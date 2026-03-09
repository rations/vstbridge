# VST3 Bridge - Implementation Status & Completion Roadmap

## Executive Summary

**Current Status**: ~10% Complete - Project skeleton is in place with compilation successful for the Linux library. Wine host component has ~40 remaining compilation errors. Real functionality is **NOT YET IMPLEMENTED** - this is a skeleton project that needs substantial work.

**Critical Warning**: This project currently **does not work** for running Windows VST3 plugins on Linux. The compilation fixes only establish the foundation. The actual audio processing, GUI capture, and event handling infrastructure exists but is non-functional.

---

## Architecture Overview (As Designed vs As Implemented)

### Design Goals
The VST3 Bridge aims to solve yabridge's Wine compatibility issues by using off-screen rendering:
1. **Off-screen GUI rendering**: Windows plugins render to hidden window, captured via GDI BitBlt
2. **Pixel buffer transfer**: RGBA/BGRA data transferred via shared memory
3. **Native X11 display**: Linux side displays captured pixels in native window
4. **Event forwarding**: Mouse/keyboard events forwarded via XCB → Windows messages

### Current Implementation Status
❌ **NOT FUNCTIONAL** - Architecture exists in code but lacks working implementations

---

## Component Implementation Status

### 1. Native Linux Library (`libvst3bridge.so`) - ✅ COMPILES
**Status**: Compiles successfully, but **contains major implementation gaps**

#### ✅ Completed:
- ✅ VST3 plugin entry point registration (`SMTG_EXPORT_SYMBOL`)
- ✅ Plugin factory class stub
- ✅ Plugin proxy stub with message handlers
- ✅ IPC client for socket communication
- ✅ X11 window creation
- ✅ X11 pixel capture (partial implementation)
- ✅ Audio shared memory manager (Pimpl pattern)
- ✅ GUI event structures

#### ❌ Major Gaps:
- ❌ **Audio Processing**: `PluginProxy::process()` receives messages but doesn't implement audio transfer
- ❌ **Parameter Changes**: Missing real-time parameter synchronization
- ❌ **GUI Display**: X11 capture exists but isn't wired to Wine host
- ❌ **State Management**: Plugin state load/save not implemented
- ❌ **Event Forwarding**: X11 events captured but not forwarded to Wine

**Files**:
- `src/native/vst3_entry.cpp` - Entry point (compiles)
- `src/native/plugin_factory.cpp` - Factory stub
- `src/native/plugin_proxy.cpp` - Proxy with unimplemented methods
- `src/native/x11_capture.cpp` - Partial pixel capture
- `src/native/x11_window.cpp` - Window creation (untested)
- `src/native/gui_handler.cpp` - GUI handler stub

---

### 2. Wine Host Process (`vst3bridge-host.exe`) - ❌ DOES NOT COMPILE
**Status**: ~40 compilation errors remain

#### ❌ Critical Missing Pieces:
- ❌ **Plugin Loading**: Invalid `void*` casts to VST3 interfaces
- ❌ **GDI Capture**: Framework exists but no actual pixel capture logic
- ❌ **Audio Processing**: No shared memory audio transfer
- ❌ **GUI Event Injection**: No Windows message injection
- ❌ **VST3 Call Proxying**: Call forwarding not implemented
- ❌ **Window Management**: Off-screen window creation incomplete
- ❌ **Parameter Handling**: No parameter change processing

**Files**:
- `src/wine-host/host_main.cpp` - Main loop with broken message handling
- `src/wine-host/vst3_host.cpp` - VST3 host stub
- `src/wine-host/gdi_capture.cpp` - Empty implementation
- `src/wine-host/window_manager.cpp` - Stub
- `src/wine-host/audio_processor.cpp` - Unimplemented
- `src/wine-host/gui_event_receiver.cpp` - Event handling stub
- `src/wine-host/plugin_instance.cpp` - Recently fixed (needs testing)

---

### 3. Protocol Layer - ⚠️ PARTIAL
**Status**: Message structures defined, protocol incomplete

#### ✅ Completed:
- ✅ Basic message types (Handshake, Ping, CreateInstance, etc.)
- ✅ Factory operations (ClassInfo, CreateInstance)
- ✅ Plugin lifecycle (Initialize, Terminate)
- ✅ Component operations (Bus management, State management)
- ✅ Audio processor messages (Process, SetProcessing, etc.)
- ✅ Parameter change messages
- ✅ View operations (GUI lifecycle)

#### ❌ Missing:
- ❌ **Message Serialization**: Binary protocol incomplete for complex structures
- ❌ **Error Handling**: No protocol-level error recovery
- ❌ **Version Negotiation**: Protocol versioning not enforced
- ❌ **Large Data Transfer**: Streaming for large plugin states

**Files**:
- `src/common/protocol.h` - All message structures defined

---

## Detailed Implementation Requirements

### Phase 1: Wine Host Completion (Critical Path)
**Complexity**: HIGH | **Time Estimate**: 2-3 weeks | **Priority**: CRITICAL

#### 1.1 VST3 Plugin Loading & Lifecycle
**Problem**: Current code uses `void*` for component/controller pointers without proper VST3 interface casting.

**Requirements**:
1. **Proper COM-style QueryInterface**: Implement `FUnknown` reference counting
2. **Interface Casting**: Use `static_cast` for IPluginBase, IComponent, IAudioProcessor
3. **Factory Querying**: Load plugin factory via exported entry point
4. **Instance Creation**: Create component and controller instances
5. **Initialization**: Call `IPluginBase::initialize()` with IHostApplication
6. **Connection**: Connect controller to component via IConnectionPoint

**Implementation Steps**:
```cpp
// In vst3_host.cpp:
// 1. Load DLL via LoadLibrary()
// 2. Get GetPluginFactory entry point
// 3. Query IPluginFactory interface
// 4. Iterate classes, find VST3 components
// 5. Create instance via factory->createInstance()
// 6. Query IPluginBase, call initialize()
// 7. Query IAudioProcessor for audio plugins
```

**Required Files**:
- Modify: `src/wine-host/vst3_host.cpp` (currently 1KB stub)
- Modify: `src/wine-host/plugin_instance.cpp` (needs real interface work)
- Create: `src/wine-host/host_application.cpp` (IHostApplication impl)

---

#### 1.2 Audio Processing Implementation
**Problem**: No actual audio data transfer between Linux and Wine.

**Requirements**:
1. **Shared Memory Audio Buffers**: Create POSIX shared memory segments
2. **Bus Configuration**: Query plugin bus arrangements via VST3 interfaces
3. **Process Call Proxying**:
   - Receive MsgRequestProcess from Linux
   - Map shared memory for input/output buffers
   - Prepare AudioBusBuffers for each bus
   - Prepare ProcessData with parameter changes, events
   - Call IAudioProcessor::process()
   - Return results via MsgResponseProcess
4. **Parameter Automation**: Transfer parameter changes both directions
5. **Real-time Constraints**: Lock-free ring buffers for audio

**Implementation Details**:
- Use `shm_open()` + `mmap()` for shared memory
- Memory layout: `[input_bus_0][input_bus_1]...[output_bus_0]...`
- Synchronization via socket messages (not mutexes for RT safety)
- Audio format: 32-bit float (64-bit optional)

**Files**:
- Modify: `src/wine-host/audio_processor.cpp` (currently 10KB with stubs)
- Modify: `src/wine-host/audio_shm_host.cpp` (needs real SHM implementation)
- Modify: `src/native/plugin_proxy.cpp` - Add process() implementation

**VST3 Interfaces Required**:
```cpp
// Query from component:
IAudioProcessor* audioProc = static_cast<IAudioProcessor*>(component);
audioProc->setupProcessing(processSetup);
audioProc->setProcessing(true);

// Process loop:
ProcessData data;
data.numInputs = ...;
data.numOutputs = ...;
data.inputs = inputBusBuffers;
data.outputs = outputBusBuffers;
data.processContext = &context;
audioProc->process(data);
```

---

#### 1.3 GUI Capture & Display
**Problem**: GDI capture framework exists but no actual pixel transfer.

**Requirements**:
1. **Off-screen Window Creation**:
   - Create window at (-10000, -10000)
   - Set appropriate size from plugin
   - Use WS_POPUP | WS_CLIPSIBLINGS style
2. **Window Message Pump**: Run message loop for GUI updates
3. **GDI BitBlt Capture**:
   - Get window DC via GetDC()
   - Create compatible bitmap
   - BitBlt from window to bitmap
   - Extract pixel data (BGRA format)
4. **Shared Memory Transfer**:
   - Create shared memory: `vst3bridge-gui-XXXX`
   - Map to Linux side
   - Atomic frame counters for synchronization
5. **Linux Side Display**:
   - XShm for efficient display (already started)
   - Resize handling with nearest-neighbor scaling
6. **Event Injection**:
   - Map Linux X11 events to Windows messages
   - Send WM_MOUSEMOVE, WM_LBUTTONDOWN, etc.
   - Keyboard events via WM_KEYDOWN/UP

**Implementation**:
```cpp
// In gdi_capture.cpp:
HDC hdcScreen = GetDC(NULL);
HDC hdcMem = CreateCompatibleDC(hdcScreen);
HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
SelectObject(hdcMem, hBitmap);

// Capture
BitBlt(hdcMem, 0, 0, width, height, GetDC(hwnd), 0, 0, SRCCOPY);

// Get pixel data
BITMAPINFO bi = {0};
bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
bi.bmiHeader.biWidth = width;
bi.bmiHeader.biHeight = -height; // Top-down
bi.bmiHeader.biPlanes = 1;
bi.bmiHeader.biBitCount = 32;
GetDIBits(hdcMem, hBitmap, 0, height, pixelBuffer, &bi, DIB_RGB_COLORS);
```

**Files**:
- Modify: `src/wine-host/gdi_capture.cpp` (add capture logic)
- Modify: `src/wine-host/window_manager.cpp` (window creation/lifecycle)
- Modify: `src/native/gui_handler.cpp` (Linux-side display)
- Modify: `src/wine-host/gui_event_receiver.cpp` (event injection)

**Performance Requirements**:
- Capture at >= 30fps for smooth GUI
- Shared memory name: `/vst3bridge-gui-{instance_id}`
- Frame size: width * height * 4 bytes (BGRA)
- 1920x1080 GUI = ~8MB per frame

---

#### 1.4 Parameter & State Management
**Problem**: No parameter change handling or plugin state persistence.

**Requirements**:
1. **Parameter Querying**:
   - IEditController::getParameterCount()
   - IEditController::getParameterInfo()
   - IEditController::getParamNormalized()
2. **Parameter Changes**:
   - Real-time: IParameterChanges queue (from Linux audio thread)
   - UI: Direct IEditController::setParamNormalized()
3. **State Management**:
   - IComponent::getState() for save
   - IComponent::setState() for load
   - IBStream implementation for data streaming
4. **Serialization**: Protocol for transferring state data >64KB

**VST3 Flow**:
```cpp
// Parameter change from DAW:
// Linux → Socket → Wine → IEditController::setParamNormalized()

// Audio-processing parameter changes:
// IParameterChanges with Queue changes
// Process called with input parameter changes
```

**Files**:
- Modify: `src/native/plugin_proxy.cpp` (parameter change sending)
- Modify: `src/wine-host/parameter_changes.cpp` (parameter routing)
- Modify: `src/wine-host/plugin_instance.cpp` (state methods)

---

#### 1.5 Event & MIDI Handling
**Problem**: No event forwarding from Linux to Wine.

**Requirements**:
1. **Event List Transfer**:
   - IEventList for note events, pitch bend, modulation
   - Serialize events via protocol
2. **Event Injection**:
   - Linux to Wine event conversion
   - ProcessData with event lists
3. **MIDI Support**: Process MIDI events as VST3 events

**Files**:
- Modify: `src/wine-host/gui_event_receiver.cpp` (event handling)
- Modify: `src/native/plugin_proxy.cpp` (event sending)

---

### Phase 2: Linux Library Implementation
**Complexity**: MEDIUM-HIGH | **Time Estimate**: 2-3 weeks | **Priority**: HIGH

#### 2.1 VST3 Host Integration
**Problem**: Native library missing DAW interaction logic.

**Requirements**:
1. **Factory Discovery**: DAW calls entry point, get IPluginFactory
2. **Instance Creation**: CreatePluginInstance for each plugin
3. **Component/Controller**: Proper separation of concerns
4. **Audio Callback**: Process audio blocks from DAW
5. **GUI Lifecycle**: CreateView, attach to parent window

**DAW → Bridge Flow**:
```
DAW loads .so → GetPluginFactory() → CreateInstance()  
→ Initialize() → CreateView() → Process audio → Terminate()
```

**VST3 Interfaces to Implement**:
- `IPluginFactory` - Entry point for DAW
- `IComponent` - Audio processing interface
- `IAudioProcessor` - Process method
- `IEditController` - GUI and parameters
- `IPlugView` - GUI interface

**Files**:
- Modify: `src/native/vst3_entry.cpp` (entry point)
- Modify: `src/native/plugin_factory.cpp` (factory implementation)
- Modify: `src/native/vst3_plugin.cpp` (component/controller wrapper)
- Modify: `src/native/plugin_proxy.cpp` (process method)

---

#### 2.2 GUI Capture Integration
**Problem**: X11 capture exists but not integrated with Wine host.

**Requirements**:
1. **Shared Memory Setup**: Create GUI SHM segment
2. **Frame Reception**: Listen for MsgFrameUpdate from Wine
3. **Display Update**: XShmPutImage to window
4. **Resize Handling**: Window resize → new capture size
5. **Input Events**: Capture X11 events, send to Wine

**Implementation**:
```cpp
// In gui_handler.cpp:
// 1. Upon CreateView:
//    - Create shared memory for GUI
//    - Send MsgCreateView to Wine
//    - Wait for frame updates
//
// 2. Event loop:
//    - On MsgFrameUpdate: XShmPutImage()
//    - On X11 event: Send MsgInputEvent to Wine
```

**Files**:
- Modify: `src/native/gui_handler.cpp` (complete implementation)
- Modify: `src/native/x11_window.cpp` (event handling)
- Modify: `src/native/x11_capture.cpp` (integrate with protocol)

---

#### 2.3 Audio Synchronization
**Problem**: Audio processing paths exist but not functional.

**Requirements**:
1. **Block Processing**: Handle DAW audio buffer sizes
2. **Bus Management**: Query plugin buses, configure input/output counts
3. **Parameter Changes**: Transfer parameter changes WITH audio block
4. **Latency/Tail**: GetLatencySamples(), getTailSamples()
5. **Process Context**: Fill ProcessContext with timing info

**Considerations**:
- DAW may call process() with varying buffer sizes
- Real-time safety: No locking, lock-free queues
- Threading: Audio thread ≠ UI thread
- Denormal noise prevention

**Files**:
- Modify: `src/native/plugin_proxy.cpp` (process implementation)
- Modify: `src/native/audio_shm.cpp` (buffer management)
- Modify: `src/native/parameter_changes.cpp` (create this)

---

#### 2.4 State Management
**Problem**: Plugin state persistence not implemented.

**Requirements**:
1. **Save State**:
   - DAW calls getState()
   - Send MsgRequestGetState to Wine
   - Wine calls IComponent::getState()
   - Transfer IBStream data via protocol
   - Return blob to DAW
2. **Load State**:
   - DAW calls setState()
   - Send MsgRequestSetState with blob
   - Wine calls IComponent::setState()

**Complex Data Transfer**:
- State data may be large (1MB+)
- Need streaming protocol for multi-packet transfer
- Hash verification for data integrity

**Files**:
- Modify: `src/native/plugin_proxy.cpp` (getState/setState)
- Modify: `src/wine-host/plugin_instance.cpp` (state methods)
- Add streaming protocol extension

---

### Phase 3: Testing & Quality Assurance
**Complexity**: MEDIUM | **Time Estimate**: 1-2 weeks

#### 3.1 Test Suite
**Requirements**:
1. **Unit Tests**:
   - Protocol serialization/deserialization
   - Shared memory management
   - Message passing
   - VST3 interface mocking
2. **Integration Tests**:
   - Load various test plugins
   - Audio processing validation
   - GUI functionality
   - Parameter automation
3. **Real Plugin Testing**:
   - Native Instruments plugins
   - WAVES plugins
   - FabFilter plugins
   - Free VST3 plugins

**Tools**:
- Google Test (gtest) for unit testing
- Dummy VST3 plugin for testing
- Reaper or Ardour for integration testing

---

#### 3.2 Performance Optimization
**Requirements**:
1. **Audio Latency**: Measure round-trip latency
2. **GUI Framerate**: Target 60fps capture/display
3. **Memory Usage**: Minimize copies (zero-copy where possible)
4. **CPU Usage**: Profile hot paths (BitBlt, process())

**Optimization Targets**:
- Shared memory: Single copy per block
- GUI: Double-buffer shared memory (frame flipping)
- Audio: Lock-free queues, no allocations in process()
- Protocol: Buffer pooling for messages

---

### Phase 4: Production Polish
**Complexity**: LOW-MEDIUM | **Time Estimate**: 1 week

#### 4.1 Configuration & Packaging
**Requirements**:
1. **Configuration File**: 
   - Wine prefix selection
   - Plugin-specific overrides
   - Logging level, debug mode
   - GUI scaling factors
   - Audio buffer size
2. **Wine Prefix Management**:
   - Auto-detect or specify WINEPREFIX
   - Support multiple prefixes per plugin
3. **Desktop Integration**:
   - .desktop files for DAW integration
   - Plugin path configuration
   - Package as .deb/.rpm

**Files to Create**:
- `vst3bridge.conf` (ini-style config)
- `vst3bridge-launcher` script

---

#### 4.2 Error Handling & Diagnostics
**Requirements**:
1. **Comprehensive Logging**:
   - Debug messages for all operations
   - Optional verbose logging
   - Log file rotation
2. **Error Recovery**:
   - Wine process crash detection
   - Automatic restart with backoff
   - Graceful degradation (audio-only mode)
3. **Diagnostic Tools**:
   - `vst3bridge-diag` utility
   - Wine version check
   - Plugin compatibility checker

**Files**:
- Add logging throughout codebase
- Create diagnostic utility

---

## Known Technical Challenges

### Wine-Specific Issues
1. **GDI32 limitations**: Some plugins use Direct2D/Direct3D (needs fallback)
2. **Window messages**: Off-screen windows may not receive all messages
3. **Threading**: VST3 threading model vs Wine threading
4. **COM marshalling**: Cross-process COM calls problematic

### Performance Considerations
1. **BitBlt overhead**: 60fps capture of high-res GUIs is expensive
2. **Shared memory size**: Large buffers for audio + GUI
3. **Socket latency**: Unix domain sockets fast but not zero-cost
4. **Wine overhead**: Wine itself adds 5-15% CPU overhead

### Compatibility Matrix
- **Works**: Standard GDI-based plugins (most VST3 plugins)
- **May Fail**: Direct2D/Direct3D accelerated GUIs
- **Untested**: Plugins using video overlays, multiple windows
- **Known Issues**: Modal dialogs, file pickers (may need special handling)

---

## Development Roadmap (Accelerated)

### Week 1-2: Critical Fixes
**Objective**: Get Wine host compiling with basic audio processing

**Day 1-2**: Fix remaining Wine host compilation (40 errors)
- Implement missing message structures in protocol.h
- Fix factory casting in host_main.cpp
- Create proper PluginInstance implementation

**Day 3-4**: Basic audio processing
- Implement IComponent::initialize() with real IHostApplication
- Implement setBusArrangements() with proper VST3 calls
- Implement setProcessing() with IAudioProcessor
- Test with minimal audio buffer transfer

**Day 5-10**: Shared memory audio flow
- Create POSIX SHM segments in Linux
- Map SHM in Wine process
- Implement process() call forwarding
- Transfer audio buffers end-to-end

### Week 3-4: GUI Infrastructure
**Objective**: Off-screen capture working

**Day 11-13**: Window management
- Create off-screen windows in Wine
- Implement window message pump
- Basic GDI capture (BitBlt prototype)

**Day 14-17**: Shared memory GUI
- Create GUI shared memory segments
- Implement pixel transfer (BGRA)
- Linux-side display with XShm

**Day 18-21**: Event forwarding
- Capture X11 input events
- Map to Windows messages
- Inject events to Wine window

### Week 5-6: Integration & Polish
**Objective**: Production-ready basic functionality

**Day 22-24**: Parameter and state management
- Implement parameter changes in real-time
- Add plugin state save/load
- Test with multiple plugins

**Day 25-28**: Error handling & logging
- Add comprehensive error handling
- Wine crash detection & restart
- Logging infrastructure

**Day 29-30**: Performance optimization
- Profile and optimize hot paths
- Zero-copy audio paths
- Buffer pooling

### Week 7+: Testing & Release
- Integration testing with real DAWs
- Performance benchmarks
- Package creation
- Documentation

---

## Technical Debt & Code Quality

### Current Issues
1. **C-style casts**: Should use static_cast/dynamic_cast
2. **Missing error checking**: Many functions don't validate results
3. **Hardcoded constants**: Buffer sizes, limits scattered in code
4. **No RAII**: Manual resource management in places
5. **Thread safety**: Lock-free patterns not consistently applied

### Best Practices to Apply
1. **Use smart pointers**: std::unique_ptr, std::shared_ptr
2. **Exception safety**: Strong exception guarantee where possible
3. **Const correctness**: Mark methods const where appropriate
4. **Documentation**: Doxygen comments for all public APIs
5. **Testing**: Unit tests for protocol, SHM, critical paths

---

## Resource Requirements

### Hardware
- **CPU**: x86_64 with SSE2 (already required)
- **RAM**: 4GB minimum (for Wine + VST3 plugins)
- **GPU**: Not required (GDI software rendering works)

### Software Dependencies
- **Linux**: Kernel 3.17+ (for memfd_create if used)
- **Wine**: 6.0+ (latest stable recommended)
- **X11**: With SHM extension (most systems)
- **C++17**: GCC 7+ or Clang 5+
- **Build**: Meson 0.58+, Ninja

### Development Environment
- **Recommended**: Devuan 6 Excalibur with sysvinit
- **Wine Dev**: wine-devel or wine-staging package
- **VST3 SDK**: Bundled with project

---

## Conclusion

**Honest Assessment**: This is a 2-3 month project to reach production quality. The current state is 10% complete with compilation-only fixes. While the architecture is sound and well-designed, **every major component requires substantial work**:

- Wine-side VST3 interfacing: 0% (only stubs)
- Audio processing: 10% (structures exist, no transfer)
- GUI capture/display: 15% (framework exists, no capture/display)
- Event handling: 0% (not started)
- Parameter/state: 10% (messages exist, no implementation)

**To make this work for users with modern Wine** (the stated goal), you need to complete the Wine host implementation first, then integrate with the Linux library. The approach is sound (off-screen rendering solves the X11 embedding problem), but the execution is in very early stages.

**Next immediate steps**:
1. Fix remaining Wine host compilation (highest priority)
2. Implement basic plugin loading (instantiate VST3)
3. Get simple audio passthrough working (generate sine wave)
4. Add GUI capture for basic window
5. Iterate from there

The project architecture is excellent - but it's an architecture document plus stubs, not a working bridge yet.
