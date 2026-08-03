#pragma once

#include <JuceHeader.h>
#include "MelatoninBlur/melatonin/shadows.h"
#include <vector>
#include <cstdint>
#include <functional>

class SpectrogramComponent;
class SharedResources;

/**
    OpenGL heightfield for the spectrogram (expanded / Scope).
    Framed Melatonin window + nested GL host (orbit / pan / zoom).
    Floating (expanded): move via frame chrome, resize via corner grip.
    Docked (Scope strip/tiled): fills the pane, no free move/resize.
*/
class Spectrogram3DComponent : public juce::Component,
                               private juce::Timer
{
public:
    enum class MeshQuality { low = 0, medium, high };
    enum class ChromeMode { floating, docked };

    Spectrogram3DComponent();
    ~Spectrogram3DComponent() override;

    void setDataSource (SpectrogramComponent* source) noexcept { dataSource = source; }
    void setThemeColors (SharedResources* r) noexcept;
    void setActive (bool shouldBeActive) noexcept;
    bool isActive() const noexcept { return active; }

    void setMeshQuality (MeshQuality q) noexcept;
    MeshQuality getMeshQuality() const noexcept { return meshQuality; }

    void setMultisamplingEnabled (bool shouldEnable) noexcept;
    bool isMultisamplingEnabled() const noexcept { return msaaEnabled; }

    /**
        Soft background: offscreen GL → Image → paint compositing over the EQ
        (nested OpenGL HWND transparency is not available on Windows/Direct2D).
    */
    void setTransparentBackground (bool shouldEnable) noexcept;
    bool isTransparentBackground() const noexcept { return transparentBackground; }

    void setMeshHeight (float heightWorld) noexcept;
    float getMeshHeight() const noexcept { return meshHeight; }
    static constexpr float kDefaultMeshHeight = 0.55f;
    static constexpr float kMinMeshHeight = 0.15f;
    static constexpr float kMaxMeshHeight = 1.40f;

    void setChromeMode (ChromeMode mode) noexcept;
    ChromeMode getChromeMode() const noexcept { return chromeMode; }

    void resetCamera() noexcept;
    void saveAsDefaultView() noexcept;
    void invalidateMesh() noexcept;
    /** Rebuild vertex colours from the current mesh + 3D colour LUT (ramp edits). */
    void recolourMesh() noexcept;

    /** @deprecated Use getMeshHeight() — kept as alias for call sites expecting a constant. */
    static constexpr float kMeshHeight = kDefaultMeshHeight;

    /**
        Y-up turntable camera: yaw around world Y, pitch = elevation.
        No roll — the ground plane always stays downward.
    */
    struct CameraState
    {
        float yawDeg = -40.0f;
        float pitchDeg = 35.0f;   // elevation above floor horizon (0=edge-on, 90=top-down)
        float distance = 3.0f;
        float panX = 0.0f;        // look-at X (time)
        float panY = kDefaultMeshHeight * 0.35f;
        float panZ = 0.0f;        // look-at Z (freq)
    };

    void setDefaultCameraState (const CameraState& state) noexcept;
    CameraState getDefaultCameraState() const noexcept { return defaultCamera; }
    CameraState getCameraState() const noexcept { return camera; }
    static CameraState getFactoryCameraState() noexcept;

    std::function<void()> onEscape;
    std::function<void()> onUserResized;
    std::function<void()> onUserMoved;
    std::function<void()> onDefaultViewChanged;

    juce::Rectangle<int> getFrameBounds() const noexcept { return getBounds(); }
    int getShadowPad() const noexcept;
    static int getShadowPadForMode (ChromeMode mode) noexcept;
    void setResizeLimits (int maxW, int maxH) noexcept;
    void setMovementBounds (juce::Rectangle<int> parentLocalBounds) noexcept;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;

private:
    static constexpr int kShadowPadFloating = 14;
    static constexpr int kShadowPadDocked = 2;
    static constexpr float kCornerRadius = 12.0f;
    static constexpr int kGlInset = 3;

    struct Vertex { float x, y, z, r, g, b; };

    struct FreqLabel
    {
        float hz = 0.0f;
        juce::String text;
        float u0 = 0, v0 = 0, u1 = 0, v1 = 0; // atlas UVs
        float worldZ = 0.0f;
    };

    class GlHost : public juce::Component,
                   public juce::OpenGLRenderer
    {
    public:
        explicit GlHost (Spectrogram3DComponent& owner);
        ~GlHost() override;

        void setActive (bool shouldBeActive) noexcept;
        void requestAttachAsync();
        void applyPixelFormat();
        void applyBackgroundTransparency() noexcept;
        void reattachWithCurrentFormat();
        void triggerRedraw() { if (openGLContext.isAttached()) openGLContext.triggerRepaint(); }
        bool isGlReady() const noexcept { return glReady; }
        bool hasContextFailed() const noexcept { return contextFailed; }
        void markSoftContentDirty() noexcept { softContentDirty = true; }

        void newOpenGLContextCreated() override;
        void renderOpenGL() override;
        void openGLContextClosing() override;

        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
        void mouseDoubleClick (const juce::MouseEvent& e) override;
        bool keyPressed (const juce::KeyPress& key) override;

    private:
        void attachNow();
        void createShaders();
        void destroyShaders();
        void uploadMeshIfNeeded();
        void ensureFloorGeometry();
        void rebuildFloorGeometry();
        void ensureLabelAtlas();
        void drawSoftTint();
        void drawGroundAndGrid();
        void drawMesh();
        void drawFrequencyLabels();
        void setCornerUniforms (juce::OpenGLShaderProgram& program) const;
        juce::Matrix3D<float> getProjectionMatrix() const;
        juce::Matrix3D<float> getViewMatrix() const;
        bool projectWorldToNdc (float wx, float wy, float wz,
                                float& ndcX, float& ndcY, float& ndcZ) const;
        juce::Rectangle<int> getViewPixelBounds() const noexcept;
        void ensureSoftFrameBuffer (int width, int height);
        void renderSoftComposite();
        void readbackSoftImage (int width, int height);

        Spectrogram3DComponent& owner;
        juce::OpenGLContext openGLContext;
        bool attachPending = false;
        bool contextFailed = false;
        bool glReady = false;
        bool softContentDirty = true;

        std::unique_ptr<juce::OpenGLShaderProgram> colourShader;
        std::unique_ptr<juce::OpenGLShaderProgram::Attribute> colourPositionAttrib;
        std::unique_ptr<juce::OpenGLShaderProgram::Attribute> colourColourAttrib;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourProjectionUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourViewUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourResolutionUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourCornerUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourClearUniform;

        std::unique_ptr<juce::OpenGLShaderProgram> labelShader;
        std::unique_ptr<juce::OpenGLShaderProgram::Attribute> labelPositionAttrib;
        std::unique_ptr<juce::OpenGLShaderProgram::Attribute> labelTexAttrib;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> labelTexUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> labelResolutionUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> labelCornerUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> labelClearUniform;

        std::unique_ptr<juce::OpenGLShaderProgram> tintShader;
        std::unique_ptr<juce::OpenGLShaderProgram::Attribute> tintPositionAttrib;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> tintColourUniform;
        unsigned int tintVbo = 0;

        unsigned int meshVbo = 0, meshIbo = 0;
        unsigned int floorVbo = 0;
        unsigned int labelVbo = 0;
        int meshIndexCount = 0;
        int floorVertexCount = 0;
        bool meshNeedsUpload = false;

        juce::OpenGLTexture labelAtlas;
        juce::OpenGLFrameBuffer softFbo;
        unsigned int softDepthRbo = 0;
        bool labelAtlasReady = false;
        int softFboW = 0;
        int softFboH = 0;
        double floorGridSr = 0.0;
        bool floorGridLog = true;
        bool floorGridSoftBg = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlHost)
    };

    /** Transparent hit target for orbit/pan/zoom when the GL HWND is hidden in soft mode. */
    class HitLayer : public juce::Component
    {
    public:
        explicit HitLayer (Spectrogram3DComponent& owner);
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
        void mouseDoubleClick (const juce::MouseEvent& e) override;
        bool keyPressed (const juce::KeyPress& key) override;

    private:
        Spectrogram3DComponent& owner;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HitLayer)
    };

    void timerCallback() override;
    void seedDefaultOrientation() noexcept;
    void clampCamera() noexcept;
    float lookAtY() const noexcept { return meshHeight * 0.35f; }
    /** Y-up turntable view: T(pullback) * Rx(+pitch) * Ry(yaw) * T(-lookAt). */
    juce::Matrix3D<float> getTurntableViewMatrix() const noexcept;
    juce::Colour getClearColour() const noexcept;
    void applyBackgroundTransparency() noexcept;
    void layoutPresentation() noexcept;
    juce::Rectangle<int> getInnerFrameLocal() const noexcept;
    juce::Rectangle<int> getGlViewLocal() const noexcept;
    void showContextMenu (juce::Point<int> screenPos);
    bool isInMoveChrome (juce::Point<int> localPos) const noexcept;
    void applyChromeMode() noexcept;
    void markSoftContentDirty() noexcept;

    void meshSizeForQuality (int& outW, int& outH) const noexcept;
    void fillMeshColumn (int meshCol, const float* histCol, int histH);
    void seedMeshFromHistory (const std::vector<float>& history, int histW, int histH);
    void appendMeshColumnsFromHistory (const std::vector<float>& history, int histW, int histH, int numNew);
    void ensureIndexBuffer (int w, int h);
    void rebuildVerticesFromMeshDb (float brightness, float minDb, float maxDb);
    void updateMeshFromSource();
    void rebuildFreqLabels (double sampleRate, bool logFreq);
    static float worldZForFreq (float hz, double sampleRate, bool logFreq) noexcept;
    static juce::String formatGridHz (float hz);

    void handleMouseDown (const juce::MouseEvent& e);
    void handleMouseDrag (const juce::MouseEvent& e);
    void handleMouseUp (const juce::MouseEvent&);
    void handleMouseWheel (const juce::MouseWheelDetails& wheel);
    void handleDoubleClick();

    SpectrogramComponent* dataSource = nullptr;
    SharedResources* theme = nullptr;
    std::unique_ptr<GlHost> glHost;
    std::unique_ptr<HitLayer> hitLayer;
    bool active = false;
    bool msaaEnabled = false;
    bool transparentBackground = false;
    ChromeMode chromeMode = ChromeMode::floating;
    MeshQuality meshQuality = MeshQuality::medium;
    float meshHeight = kDefaultMeshHeight;

    melatonin::DropShadow panelShadow {
        { juce::Colours::black.withAlpha (0.55f), 16, { 0, 6 }, 0 }
    };

    std::vector<Vertex> cpuVertices;
    std::vector<uint32_t> cpuIndices;
    juce::CriticalSection meshLock;
    std::vector<float> meshDb;
    int meshW = 0;
    int meshH = 0;
    uint64_t lastHistorySerial = 0;
    float lastBrightness = -1.0f;
    float lastMinDb = 0.0f;
    float lastMaxDb = 0.0f;
    bool indicesValid = false;
    bool meshNeedsUpload = false;

    std::vector<FreqLabel> freqLabels;
    juce::Image labelAtlasImage;
    bool labelAtlasDirty = true;

    juce::Image softCompositeImage;
    juce::CriticalSection softImageLock;

    CameraState camera;
    CameraState defaultCamera;
    juce::Point<float> lastDrag {};
    enum class DragMode { none, orbit, pan, dolly };
    DragMode dragMode = DragMode::none;
    /** Elevation above the horizon (0 = edge-on to the floor, 90 = top-down). */
    static constexpr float kMinPitchDeg = 5.0f;
    static constexpr float kMaxPitchDeg = 89.0f;

    juce::ComponentDragger moveDragger;
    juce::ComponentBoundsConstrainer constrainer;
    bool movingByChrome = false;

    std::unique_ptr<juce::ResizableCornerComponent> resizer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Spectrogram3DComponent)
};
