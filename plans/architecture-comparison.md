# VST3 Bridge Architecture Alternatives

## Overview

Given your requirements:
- ✅ GPU-accelerated plugins (OpenGL/Vulkan/DXVK)
- ✅ Plugin groups (multiple plugins per Wine process)
- ✅ Target: REAPER and Ardour
- ✅ Wine 11 (latest staging)
- ✅ Real-time GUI (minimal latency)

Here are the three viable approaches:

---

## Option 1: DMA-BUF/GBM Texture Sharing (Recommended for GPU)

**Concept**: Plugin renders to GPU texture in Wine → Share texture via DMA-BUF → Display in Linux DAW

```mermaid
flowchart TD
    subgraph Wine["Wine Process"]
        Plugin["VST3 Plugin"] -->|OpenGL/Vulkan| FBO["Framebuffer Object"]
        FBO -->|DMA-BUF export| Tex["dmabuf fd"]
    end
    
    subgraph Linux["Linux Native"]
        Tex -->|import| EGL["EGLImage"]
        EGL -->|render| X11["X11 Window<br/>in DAW"]
    end
```

**Complexity**: HIGH
- Need to intercept OpenGL/Vulkan contexts in Wine
- Handle DMA-BUF import/export
- Manage GPU memory synchronization
- Different code paths for each graphics API

**Pros**:
- ✅ Real-time (sub-millisecond latency)
- ✅ Zero-copy GPU-to-GPU transfer
- ✅ Works with all GPU APIs
- ✅ Most future-proof

**Cons**:
- ❌ Very complex implementation
- ❌ Requires deep Wine integration
- ❌ Debugging GPU issues is difficult
- ❌ May not work with proprietary NVIDIA drivers easily

---

## Option 2: PipeWire Video Capture (Balanced)

**Concept**: Create Wine window off-screen → PipeWire captures it → Stream to Linux side

```mermaid
flowchart TD
    subgraph Wine["Wine Process"]
        Plugin["VST3 Plugin"] -->|renders| Win["Off-screen Window"]
        Win -->|DMA-BUF| PW["PipeWire Node"]
    end
    
    subgraph Linux["Linux Native"]
        PW -->|consumes| Buf["Video Buffer"]
        Buf -->|display| X11["X11 Window<br/>in DAW"]
    end
```

**Complexity**: MEDIUM-HIGH
- Set up PipeWire stream from Wine window
- Configure video format negotiation
- Handle buffer synchronization

**Pros**:
- ✅ Works with all GPU APIs (handled by PipeWire)
- ✅ Standard Linux infrastructure
- ✅ Good performance (DMA-BUF under the hood)
- ✅ Handles compositor/ Wayland compatibility

**Cons**:
- ⚠️ 1-2 frames latency (16-33ms at 60fps)
- ⚠️ Requires PipeWire (standard on modern distros)
- ⚠️ More moving parts

---

## Option 3: X11 Composite Redirection (Simpler, Limited)

**Concept**: Use X11 Composite extension to redirect Wine window → Capture pixmap → Display

```mermaid
flowchart TD
    subgraph X11["X11 Server"]
        Wine["Wine X11 Window"] -->|Composite Redirect| Pixmap["Off-screen Pixmap"]
        Pixmap -->|XShmGetImage| Buffer["Shared Memory"]
        Buffer -->|XShmPutImage| DAW["DAW Window"]
    end
```

**Complexity**: MEDIUM
- Enable Composite redirect on Wine window
- Read pixmap via shared memory
- Display in DAW's X11 window

**Pros**:
- ✅ Simpler implementation
- ✅ No GPU-specific code
- ✅ Works on any X11 setup

**Cons**:
- ❌ **Does NOT work with GPU-accelerated rendering** (OpenGL/Vulkan bypass X11 pixmaps)
- ⚠️ CPU memory copy required
- ⚠️ Higher CPU usage
- ❌ Limited to GDI/Direct2D plugins only

---

## Option 4: Hybrid Approach (My Recommendation)

**Concept**: Detect rendering method → Use appropriate path

```mermaid
flowchart TD
    Plugin["VST3 Plugin"] -->|detects| Renderer["Rendering Backend"]
    
    Renderer -->|GDI/Direct2D| Path1["X11 Composite<br/>(Simple, fast)"]
    Renderer -->|OpenGL/Vulkan| Path2["DMA-BUF<br/>(Zero-copy)"]
    Renderer -->|DXVK| Path3["PipeWire<br/>(Robust)"]
    
    Path1 --> X11["X11 Display"]
    Path2 --> X11
    Path3 --> X11
```

**Implementation Strategy**:
1. **Phase 1**: Start with X11 Composite for GDI plugins (fastest to implement)
2. **Phase 2**: Add PipeWire capture for GPU plugins (broad compatibility)
3. **Phase 3**: Optimize with DMA-BUF for latency-critical scenarios

---

## Comparison Matrix

| Approach | Latency | GPU Support | Complexity | Maintenance |
|----------|---------|-------------|------------|-------------|
| DMA-BUF/GBM | <1ms | ✅ Full | Very High | Hard |
| PipeWire | ~16-33ms | ✅ Full | Medium-High | Medium |
| X11 Composite | ~16-33ms | ❌ GDI only | Medium | Easy |
| **Hybrid** | Variable | ✅ Full | Medium | Medium |

---

## My Recommendation

Given your requirements, I recommend starting with the **Hybrid Approach** focusing on:

### Phase 1 (MVP): X11 Composite + Wine 11
- Use off-screen window with Composite redirect
- Works for GDI/Direct2D plugins immediately
- Gets you a working solution fastest

### Phase 2: PipeWire for GPU Plugins  
- Add PipeWire capture for OpenGL/Vulkan/DXVK
- Broad compatibility, reasonable latency
- Handles the GPU plugins you need

### Phase 3: DMA-BUF Optimization (Optional)
- Only if Phase 2 latency isn't acceptable
- Optimize the most common paths

This gives you:
- ✅ Working solution quickly (Phase 1)
- ✅ GPU plugin support (Phase 2)
- ✅ Room for optimization (Phase 3)
- ✅ Manageable complexity
