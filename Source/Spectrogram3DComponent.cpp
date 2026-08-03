#include "Spectrogram3DComponent.h"
#include "SpectrogramComponent.h"
#include "Menu/SharedResources.h"
#include "ComboBoxLookAndFeel.h"
#include <cstring>

namespace
{
    constexpr const char* kColourVertexShader = R"(
        #version 150
        in vec3 position;
        in vec3 colour;
        uniform mat4 projectionMatrix;
        uniform mat4 viewMatrix;
        out vec3 vColour;
        void main()
        {
            vColour = colour;
            gl_Position = projectionMatrix * viewMatrix * vec4 (position, 1.0);
        }
    )";

    constexpr const char* kColourFragmentShader = R"(
        #version 150
        in vec3 vColour;
        out vec4 fragColour;
        uniform vec2 uResolution;
        uniform float uCornerRadius;
        uniform vec4 uClearColour;
        void main()
        {
            vec2 halfSize = uResolution * 0.5;
            vec2 p = gl_FragCoord.xy - halfSize;
            vec2 q = abs(p) - halfSize + vec2(uCornerRadius);
            float d = min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - uCornerRadius;
            if (d > 0.0)
            {
                fragColour = uClearColour;
                return;
            }
            fragColour = vec4 (vColour, 1.0);
        }
    )";

    constexpr const char* kLabelVertexShader = R"(
        #version 150
        in vec2 position;
        in vec2 texCoord;
        out vec2 vTex;
        void main()
        {
            vTex = texCoord;
            gl_Position = vec4 (position, 0.0, 1.0);
        }
    )";

    constexpr const char* kLabelFragmentShader = R"(
        #version 150
        in vec2 vTex;
        out vec4 fragColour;
        uniform sampler2D uTex;
        uniform vec2 uResolution;
        uniform float uCornerRadius;
        uniform vec4 uClearColour;
        void main()
        {
            vec2 halfSize = uResolution * 0.5;
            vec2 p = gl_FragCoord.xy - halfSize;
            vec2 q = abs(p) - halfSize + vec2(uCornerRadius);
            float d = min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - uCornerRadius;
            if (d > 0.0)
            {
                fragColour = uClearColour;
                return;
            }
            vec4 t = texture (uTex, vTex);
            if (t.a < 0.08)
                discard;
            fragColour = t;
        }
    )";

    constexpr const char* kBlitVertexShader = R"(
        #version 150
        in vec2 position;
        in vec2 texCoord;
        out vec2 vTex;
        void main()
        {
            vTex = texCoord;
            gl_Position = vec4 (position, 0.0, 1.0);
        }
    )";

    constexpr const char* kBlitFragmentShader = R"(
        #version 150
        in vec2 vTex;
        out vec4 fragColour;
        uniform sampler2D uTex;
        uniform vec4 uTint;
        void main()
        {
            vec4 bg = texture (uTex, vTex);
            fragColour = vec4 (mix (bg.rgb, uTint.rgb, uTint.a), 1.0);
        }
    )";

    static constexpr float kMajorHz[] = {
        20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f
    };
    static constexpr float kMinorHz[] = {
        30.0f, 40.0f, 60.0f, 70.0f, 80.0f, 90.0f,
        150.0f, 300.0f, 400.0f, 600.0f, 700.0f, 800.0f, 900.0f,
        1500.0f, 3000.0f, 4000.0f, 6000.0f, 7000.0f, 8000.0f, 9000.0f, 15000.0f
    };
}

//==============================================================================
Spectrogram3DComponent::GlHost::GlHost (Spectrogram3DComponent& o)
    : owner (o)
{
    // Soft BG composites a backdrop texture in GL — the native HWND stays opaque.
    setOpaque (true);
    setVisible (false);
    setInterceptsMouseClicks (true, true);
    setWantsKeyboardFocus (true);

    openGLContext.setOpenGLVersionRequired (juce::OpenGLContext::openGL3_2);
    openGLContext.setRenderer (this);
    openGLContext.setComponentPaintingEnabled (false);
    applyPixelFormat();
}

Spectrogram3DComponent::GlHost::~GlHost()
{
    openGLContext.detach();
}

void Spectrogram3DComponent::GlHost::applyPixelFormat()
{
    openGLContext.setMultisamplingEnabled (owner.msaaEnabled);
    juce::OpenGLPixelFormat pf (8, 8, 24, 8);
    pf.multisamplingLevel = owner.msaaEnabled ? (uint8_t) 4 : (uint8_t) 0;
    openGLContext.setPixelFormat (pf);
}

void Spectrogram3DComponent::GlHost::setActive (bool shouldBeActive) noexcept
{
    setVisible (shouldBeActive);
    if (shouldBeActive)
        requestAttachAsync();
}

void Spectrogram3DComponent::GlHost::requestAttachAsync()
{
    if (openGLContext.isAttached() || attachPending)
        return;

    attachPending = true;
    juce::Component::SafePointer<GlHost> safe (this);
    juce::MessageManager::callAsync ([safe]
    {
        if (safe == nullptr)
            return;
        safe->attachPending = false;
        if (! safe->isVisible())
            return;
        safe->attachNow();
    });
}

void Spectrogram3DComponent::GlHost::attachNow()
{
    if (! isVisible() || openGLContext.isAttached())
        return;

    if (getPeer() == nullptr || getWidth() < 2 || getHeight() < 2)
    {
        requestAttachAsync();
        return;
    }

    applyPixelFormat();
    openGLContext.attachTo (*this);
    openGLContext.triggerRepaint();
}

void Spectrogram3DComponent::GlHost::reattachWithCurrentFormat()
{
    applyPixelFormat();
    if (openGLContext.isAttached())
        openGLContext.detach();
    if (isVisible())
        requestAttachAsync();
}

void Spectrogram3DComponent::GlHost::createShaders()
{
    colourShader = std::make_unique<juce::OpenGLShaderProgram> (openGLContext);
    if (! colourShader->addVertexShader (kColourVertexShader)
        || ! colourShader->addFragmentShader (kColourFragmentShader)
        || ! colourShader->link())
    {
        DBG ("Spectrogram3D colour shader: " + colourShader->getLastError());
        colourShader.reset();
        contextFailed = true;
        return;
    }

    colourPositionAttrib = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*colourShader, "position");
    colourColourAttrib = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*colourShader, "colour");
    colourProjectionUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "projectionMatrix");
    colourViewUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "viewMatrix");
    colourResolutionUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uResolution");
    colourCornerUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uCornerRadius");
    colourClearUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uClearColour");

    labelShader = std::make_unique<juce::OpenGLShaderProgram> (openGLContext);
    if (! labelShader->addVertexShader (kLabelVertexShader)
        || ! labelShader->addFragmentShader (kLabelFragmentShader)
        || ! labelShader->link())
    {
        DBG ("Spectrogram3D label shader: " + labelShader->getLastError());
        labelShader.reset();
    }
    else
    {
        labelPositionAttrib = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*labelShader, "position");
        labelTexAttrib = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*labelShader, "texCoord");
        labelTexUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*labelShader, "uTex");
        labelResolutionUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*labelShader, "uResolution");
        labelCornerUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*labelShader, "uCornerRadius");
        labelClearUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*labelShader, "uClearColour");
    }

    blitShader = std::make_unique<juce::OpenGLShaderProgram> (openGLContext);
    if (! blitShader->addVertexShader (kBlitVertexShader)
        || ! blitShader->addFragmentShader (kBlitFragmentShader)
        || ! blitShader->link())
    {
        DBG ("Spectrogram3D blit shader: " + blitShader->getLastError());
        blitShader.reset();
    }
    else
    {
        blitPositionAttrib = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*blitShader, "position");
        blitTexAttrib = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*blitShader, "texCoord");
        blitTexUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*blitShader, "uTex");
        blitTintUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*blitShader, "uTint");
    }

    contextFailed = (colourShader == nullptr);
}

void Spectrogram3DComponent::GlHost::destroyShaders()
{
    colourClearUniform.reset();
    colourCornerUniform.reset();
    colourResolutionUniform.reset();
    colourViewUniform.reset();
    colourProjectionUniform.reset();
    colourColourAttrib.reset();
    colourPositionAttrib.reset();
    colourShader.reset();

    labelClearUniform.reset();
    labelCornerUniform.reset();
    labelResolutionUniform.reset();
    labelTexUniform.reset();
    labelTexAttrib.reset();
    labelPositionAttrib.reset();
    labelShader.reset();

    blitTintUniform.reset();
    blitTexUniform.reset();
    blitTexAttrib.reset();
    blitPositionAttrib.reset();
    blitShader.reset();
}

void Spectrogram3DComponent::GlHost::newOpenGLContextCreated()
{
    meshVbo = meshIbo = floorVbo = labelVbo = blitVbo = 0;
    floorVertexCount = 0;
    floorGridSr = 0.0;
    labelAtlasReady = false;
    backdropTexReady = false;
    createShaders();
    juce::gl::glGenBuffers (1, &meshVbo);
    juce::gl::glGenBuffers (1, &meshIbo);
    juce::gl::glGenBuffers (1, &labelVbo);
    juce::gl::glGenBuffers (1, &blitVbo);
    glReady = (colourShader != nullptr && meshVbo != 0 && meshIbo != 0);
    meshNeedsUpload = true;
    owner.meshNeedsUpload = true;
    owner.backdropNeedsUpload = true;
}

void Spectrogram3DComponent::GlHost::openGLContextClosing()
{
    labelAtlas.release();
    labelAtlasReady = false;
    backdropTex.release();
    backdropTexReady = false;
    if (meshVbo != 0) { juce::gl::glDeleteBuffers (1, &meshVbo); meshVbo = 0; }
    if (meshIbo != 0) { juce::gl::glDeleteBuffers (1, &meshIbo); meshIbo = 0; }
    if (floorVbo != 0) { juce::gl::glDeleteBuffers (1, &floorVbo); floorVbo = 0; }
    if (labelVbo != 0) { juce::gl::glDeleteBuffers (1, &labelVbo); labelVbo = 0; }
    if (blitVbo != 0) { juce::gl::glDeleteBuffers (1, &blitVbo); blitVbo = 0; }
    destroyShaders();
    glReady = false;
    meshIndexCount = 0;
    floorVertexCount = 0;
}

void Spectrogram3DComponent::GlHost::setCornerUniforms (juce::OpenGLShaderProgram&) const
{
    const float scale = (float) openGLContext.getRenderingScale();
    const float resX = (float) getWidth() * scale;
    const float resY = (float) getHeight() * scale;
    const float corner = juce::jmax (1.0f, (kCornerRadius - (float) kGlInset) * scale);
    const auto clear = owner.getClearColour();

    if (colourResolutionUniform != nullptr)
        colourResolutionUniform->set (resX, resY);
    if (colourCornerUniform != nullptr)
        colourCornerUniform->set (corner);
    // Soft BG: keep rounded-corner outside transparent so the sampled backdrop shows through.
    const float cr = owner.transparentBackground ? 0.0f : clear.getFloatRed();
    const float cg = owner.transparentBackground ? 0.0f : clear.getFloatGreen();
    const float cb = owner.transparentBackground ? 0.0f : clear.getFloatBlue();
    const float ca = owner.transparentBackground ? 0.0f : 1.0f;

    if (colourClearUniform != nullptr)
        colourClearUniform->set (cr, cg, cb, ca);

    if (labelResolutionUniform != nullptr)
        labelResolutionUniform->set (resX, resY);
    if (labelCornerUniform != nullptr)
        labelCornerUniform->set (corner);
    if (labelClearUniform != nullptr)
        labelClearUniform->set (cr, cg, cb, ca);
}

void Spectrogram3DComponent::GlHost::uploadMeshIfNeeded()
{
    std::vector<Vertex> verts;
    std::vector<uint32_t> inds;
    {
        const juce::ScopedLock sl (owner.meshLock);
        if (! owner.meshNeedsUpload && ! meshNeedsUpload)
            return;
        verts = owner.cpuVertices;
        inds = owner.cpuIndices;
        owner.meshNeedsUpload = false;
        meshNeedsUpload = false;
    }

    if (verts.empty() || inds.empty() || meshVbo == 0 || meshIbo == 0)
        return;

    juce::gl::glBindBuffer (juce::gl::GL_ARRAY_BUFFER, meshVbo);
    juce::gl::glBufferData (juce::gl::GL_ARRAY_BUFFER,
                            (GLsizeiptr) (verts.size() * sizeof (Vertex)),
                            verts.data(), juce::gl::GL_DYNAMIC_DRAW);
    juce::gl::glBindBuffer (juce::gl::GL_ELEMENT_ARRAY_BUFFER, meshIbo);
    juce::gl::glBufferData (juce::gl::GL_ELEMENT_ARRAY_BUFFER,
                            (GLsizeiptr) (inds.size() * sizeof (uint32_t)),
                            inds.data(), juce::gl::GL_DYNAMIC_DRAW);
    meshIndexCount = (int) inds.size();
}

void Spectrogram3DComponent::GlHost::ensureFloorGeometry()
{
    const double sr = (owner.dataSource != nullptr)
                          ? juce::jmax (1.0, owner.dataSource->getDisplaySampleRate())
                          : 48000.0;
    const bool logFreq = (owner.dataSource != nullptr) ? owner.dataSource->isLogFrequencyAxis() : true;
    const bool soft = owner.transparentBackground;

    if (floorVbo != 0 && sr == floorGridSr && logFreq == floorGridLog && soft == floorGridSoftBg)
        return;

    floorGridSr = sr;
    floorGridLog = logFreq;
    floorGridSoftBg = soft;
    owner.rebuildFreqLabels (sr, logFreq);
    rebuildFloorGeometry();
    labelAtlasReady = false;
}

void Spectrogram3DComponent::GlHost::rebuildFloorGeometry()
{
    constexpr float groundY = -0.012f;
    constexpr float gridY = -0.010f;
    std::vector<Vertex> lines;
    lines.reserve (160);

    auto pushLine = [&lines] (float x0, float y0, float z0, float x1, float y1, float z1, float r, float g, float b)
    {
        lines.push_back ({ x0, y0, z0, r, g, b });
        lines.push_back ({ x1, y1, z1, r, g, b });
    };

    // Opaque ground plane (two triangles). Skipped for soft BG so the backdrop shows through.
    if (! owner.transparentBackground)
    {
        const float gr = 0.06f, gg = 0.07f, gb = 0.09f;
        lines.push_back ({ -1.0f, groundY, -1.0f, gr, gg, gb });
        lines.push_back ({  1.0f, groundY, -1.0f, gr, gg, gb });
        lines.push_back ({  1.0f, groundY,  1.0f, gr, gg, gb });
        lines.push_back ({ -1.0f, groundY, -1.0f, gr, gg, gb });
        lines.push_back ({  1.0f, groundY,  1.0f, gr, gg, gb });
        lines.push_back ({ -1.0f, groundY,  1.0f, gr, gg, gb });
    }

    const float nyquist = (float) (floorGridSr * 0.5);
    const float maxHz = juce::jmin (SpectrogramComponent::kMaxDisplayHz, nyquist * 0.999f);

    for (float hz : kMinorHz)
    {
        if (hz < SpectrogramComponent::kMinDisplayHz || hz > maxHz)
            continue;
        const float z = Spectrogram3DComponent::worldZForFreq (hz, floorGridSr, floorGridLog);
        pushLine (-1.0f, gridY, z, 1.0f, gridY, z, 0.28f, 0.30f, 0.34f);
    }
    for (float hz : kMajorHz)
    {
        if (hz < SpectrogramComponent::kMinDisplayHz || hz > maxHz)
            continue;
        const float z = Spectrogram3DComponent::worldZForFreq (hz, floorGridSr, floorGridLog);
        pushLine (-1.0f, gridY, z, 1.0f, gridY, z, 0.62f, 0.65f, 0.70f);
        // Playhead tick
        pushLine (1.0f, gridY, z, 1.0f, 0.07f, z, 0.85f, 0.85f, 0.88f);
    }

    constexpr int timeDiv = 8;
    for (int i = 0; i <= timeDiv; ++i)
    {
        const float x = (float) i / (float) timeDiv * 2.0f - 1.0f;
        const float a = (i == timeDiv) ? 0.75f : 0.32f;
        pushLine (x, gridY, -1.0f, x, gridY, 1.0f, a, a, a + 0.02f);
    }

    pushLine (-1.0f, gridY, -1.0f, 1.0f, gridY, -1.0f, 0.7f, 0.72f, 0.75f);
    pushLine (1.0f, gridY, -1.0f, 1.0f, gridY, 1.0f, 0.7f, 0.72f, 0.75f);
    pushLine (1.0f, gridY, 1.0f, -1.0f, gridY, 1.0f, 0.7f, 0.72f, 0.75f);
    pushLine (-1.0f, gridY, 1.0f, -1.0f, gridY, -1.0f, 0.7f, 0.72f, 0.75f);

    floorVertexCount = (int) lines.size();
    if (floorVbo == 0)
        juce::gl::glGenBuffers (1, &floorVbo);

    juce::gl::glBindBuffer (juce::gl::GL_ARRAY_BUFFER, floorVbo);
    juce::gl::glBufferData (juce::gl::GL_ARRAY_BUFFER,
                            (GLsizeiptr) (lines.size() * sizeof (Vertex)),
                            lines.data(), juce::gl::GL_DYNAMIC_DRAW);
    juce::gl::glBindBuffer (juce::gl::GL_ARRAY_BUFFER, 0);
}

void Spectrogram3DComponent::GlHost::ensureLabelAtlas()
{
    if (labelAtlasReady || labelShader == nullptr)
        return;

    if (owner.labelAtlasDirty || ! owner.labelAtlasImage.isValid())
    {
        constexpr int cellW = 64;
        constexpr int cellH = 28;
        const int n = (int) owner.freqLabels.size();
        if (n <= 0)
            return;

        const int cols = 4;
        const int rows = (n + cols - 1) / cols;
        juce::Image img (juce::Image::ARGB, cols * cellW, rows * cellH, true);
        juce::Graphics g (img);
        g.setColour (juce::Colours::transparentBlack);
        g.fillAll();
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));

        for (int i = 0; i < n; ++i)
        {
            const int col = i % cols;
            const int row = i / cols;
            auto cell = juce::Rectangle<int> (col * cellW, row * cellH, cellW, cellH);
            g.setColour (juce::Colours::black.withAlpha (0.75f));
            g.drawText (owner.freqLabels[(size_t) i].text, cell.translated (1, 1),
                        juce::Justification::centred, false);
            g.setColour (juce::Colours::white.withAlpha (0.92f));
            g.drawText (owner.freqLabels[(size_t) i].text, cell,
                        juce::Justification::centred, false);

            const float u0 = (float) (col * cellW) / (float) img.getWidth();
            const float v0 = 1.0f - (float) ((row + 1) * cellH) / (float) img.getHeight();
            const float u1 = (float) ((col + 1) * cellW) / (float) img.getWidth();
            const float v1 = 1.0f - (float) (row * cellH) / (float) img.getHeight();
            owner.freqLabels[(size_t) i].u0 = u0;
            owner.freqLabels[(size_t) i].v0 = v0;
            owner.freqLabels[(size_t) i].u1 = u1;
            owner.freqLabels[(size_t) i].v1 = v1;
        }

        owner.labelAtlasImage = std::move (img);
        owner.labelAtlasDirty = false;
    }

    labelAtlas.loadImage (owner.labelAtlasImage);

    // Recompute UVs against the actual (possibly POT-padded) texture size.
    const float tw = (float) juce::jmax (1, labelAtlas.getWidth());
    const float th = (float) juce::jmax (1, labelAtlas.getHeight());
    constexpr int cellW = 64;
    constexpr int cellH = 28;
    constexpr int cols = 4;
    for (int i = 0; i < (int) owner.freqLabels.size(); ++i)
    {
        const int col = i % cols;
        const int row = i / cols;
        auto& lb = owner.freqLabels[(size_t) i];
        lb.u0 = (float) (col * cellW) / tw;
        lb.u1 = (float) ((col + 1) * cellW) / tw;
        lb.v1 = 1.0f - (float) (row * cellH) / th;
        lb.v0 = 1.0f - (float) ((row + 1) * cellH) / th;
    }

    labelAtlasReady = true;
}

juce::Matrix3D<float> Spectrogram3DComponent::GlHost::getProjectionMatrix() const
{
    constexpr float nearPlane = 0.05f;
    constexpr float farPlane = 80.0f;
    constexpr float fovHalfWAtNear = 1.0f / 1.5f;
    const float aspect = (float) getHeight() / (float) juce::jmax (1, getWidth());
    const float w = nearPlane * fovHalfWAtNear;
    const float h = w * aspect;
    return juce::Matrix3D<float>::fromFrustum (-w, w, -h, h, nearPlane, farPlane);
}

juce::Matrix3D<float> Spectrogram3DComponent::GlHost::getViewMatrix() const
{
    return owner.getTurntableViewMatrix();
}

bool Spectrogram3DComponent::GlHost::projectWorldToNdc (float wx, float wy, float wz,
                                                       float& ndcX, float& ndcY) const
{
    const auto proj = getProjectionMatrix();
    const auto view = getViewMatrix();
    float v[4] = { wx, wy, wz, 1.0f };
    float eye[4] = {
        view.mat[0] * v[0] + view.mat[4] * v[1] + view.mat[8]  * v[2] + view.mat[12] * v[3],
        view.mat[1] * v[0] + view.mat[5] * v[1] + view.mat[9]  * v[2] + view.mat[13] * v[3],
        view.mat[2] * v[0] + view.mat[6] * v[1] + view.mat[10] * v[2] + view.mat[14] * v[3],
        view.mat[3] * v[0] + view.mat[7] * v[1] + view.mat[11] * v[2] + view.mat[15] * v[3]
    };
    float clip[4] = {
        proj.mat[0] * eye[0] + proj.mat[4] * eye[1] + proj.mat[8]  * eye[2] + proj.mat[12] * eye[3],
        proj.mat[1] * eye[0] + proj.mat[5] * eye[1] + proj.mat[9]  * eye[2] + proj.mat[13] * eye[3],
        proj.mat[2] * eye[0] + proj.mat[6] * eye[1] + proj.mat[10] * eye[2] + proj.mat[14] * eye[3],
        proj.mat[3] * eye[0] + proj.mat[7] * eye[1] + proj.mat[11] * eye[2] + proj.mat[15] * eye[3]
    };
    if (std::abs (clip[3]) < 1.0e-6f)
        return false;
    ndcX = clip[0] / clip[3];
    ndcY = clip[1] / clip[3];
    return ndcX > -1.2f && ndcX < 1.2f && ndcY > -1.2f && ndcY < 1.2f;
}

void Spectrogram3DComponent::GlHost::uploadBackdropIfNeeded()
{
    juce::Image img;
    {
        const juce::ScopedLock sl (owner.backdropLock);
        if (! owner.backdropNeedsUpload || ! owner.backdropImage.isValid())
            return;
        img = owner.backdropImage;
        owner.backdropNeedsUpload = false;
    }

    backdropTex.loadImage (img);
    backdropTexReady = true;
}

void Spectrogram3DComponent::GlHost::drawBackdropAndTint()
{
    using namespace juce::gl;
    uploadBackdropIfNeeded();
    if (blitShader == nullptr || blitVbo == 0 || ! backdropTexReady)
        return;

    // Fullscreen NDC quad (triangle strip), Y flipped for OpenGL texture convention.
    const float verts[] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 0.0f
    };

    blitShader->use();
    if (blitTexUniform != nullptr)
        blitTexUniform->set (0);
    const auto tint = owner.getClearColour();
    if (blitTintUniform != nullptr)
        blitTintUniform->set (tint.getFloatRed(), tint.getFloatGreen(),
                              tint.getFloatBlue(), tint.getFloatAlpha());

    backdropTex.bind();
    glActiveTexture (GL_TEXTURE0);
    glDisable (GL_DEPTH_TEST);
    glDepthMask (GL_FALSE);
    glDisable (GL_BLEND);
    glDisable (GL_CULL_FACE);

    glBindBuffer (GL_ARRAY_BUFFER, blitVbo);
    glBufferData (GL_ARRAY_BUFFER, sizeof (verts), verts, GL_STREAM_DRAW);

    const GLsizei stride = (GLsizei) (4 * sizeof (float));
    if (blitPositionAttrib != nullptr && blitPositionAttrib->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) blitPositionAttrib->attributeID);
        glVertexAttribPointer ((GLuint) blitPositionAttrib->attributeID, 2, GL_FLOAT, GL_FALSE, stride, nullptr);
    }
    if (blitTexAttrib != nullptr && blitTexAttrib->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) blitTexAttrib->attributeID);
        glVertexAttribPointer ((GLuint) blitTexAttrib->attributeID, 2, GL_FLOAT, GL_FALSE,
                               stride, (const void*) (sizeof (float) * 2));
    }

    glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);

    if (blitPositionAttrib != nullptr && blitPositionAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) blitPositionAttrib->attributeID);
    if (blitTexAttrib != nullptr && blitTexAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) blitTexAttrib->attributeID);

    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glDepthMask (GL_TRUE);
    glEnable (GL_DEPTH_TEST);
}

void Spectrogram3DComponent::GlHost::drawGroundAndGrid()
{
    using namespace juce::gl;
    if (floorVbo == 0 || floorVertexCount <= 0 || colourShader == nullptr)
        return;

    colourShader->use();
    setCornerUniforms (*colourShader);
    if (colourProjectionUniform != nullptr)
        colourProjectionUniform->setMatrix4 (getProjectionMatrix().mat, 1, false);
    if (colourViewUniform != nullptr)
        colourViewUniform->setMatrix4 (getViewMatrix().mat, 1, false);

    glDisable (GL_CULL_FACE);
    glEnable (GL_DEPTH_TEST);
    glDepthMask (GL_TRUE);
    glBindBuffer (GL_ARRAY_BUFFER, floorVbo);

    const GLsizei stride = (GLsizei) sizeof (Vertex);
    if (colourPositionAttrib != nullptr && colourPositionAttrib->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) colourPositionAttrib->attributeID);
        glVertexAttribPointer ((GLuint) colourPositionAttrib->attributeID, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    }
    if (colourColourAttrib != nullptr && colourColourAttrib->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) colourColourAttrib->attributeID);
        glVertexAttribPointer ((GLuint) colourColourAttrib->attributeID, 3, GL_FLOAT, GL_FALSE,
                               stride, (const void*) (sizeof (float) * 3));
    }

    const int groundVerts = owner.transparentBackground ? 0 : 6;
    if (groundVerts > 0)
        glDrawArrays (GL_TRIANGLES, 0, groundVerts);
    if (floorVertexCount > groundVerts)
        glDrawArrays (GL_LINES, groundVerts, floorVertexCount - groundVerts);

    if (colourPositionAttrib != nullptr && colourPositionAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) colourPositionAttrib->attributeID);
    if (colourColourAttrib != nullptr && colourColourAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) colourColourAttrib->attributeID);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
}

void Spectrogram3DComponent::GlHost::drawMesh()
{
    using namespace juce::gl;
    if (meshIndexCount <= 0 || colourShader == nullptr)
        return;

    colourShader->use();
    setCornerUniforms (*colourShader);
    if (colourProjectionUniform != nullptr)
        colourProjectionUniform->setMatrix4 (getProjectionMatrix().mat, 1, false);
    if (colourViewUniform != nullptr)
        colourViewUniform->setMatrix4 (getViewMatrix().mat, 1, false);

    // Same solid fill as before polish — no height discard, no backface cull
    // (those punched holes in quiet bins / grazing angles).
    glEnable (GL_DEPTH_TEST);
    glDepthMask (GL_TRUE);
    glDisable (GL_CULL_FACE);
    glDisable (GL_BLEND);
    glEnable (GL_POLYGON_OFFSET_FILL);
    glPolygonOffset (1.0f, 1.0f);

    glBindBuffer (GL_ARRAY_BUFFER, meshVbo);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, meshIbo);

    const GLsizei stride = (GLsizei) sizeof (Vertex);
    if (colourPositionAttrib != nullptr && colourPositionAttrib->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) colourPositionAttrib->attributeID);
        glVertexAttribPointer ((GLuint) colourPositionAttrib->attributeID, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    }
    if (colourColourAttrib != nullptr && colourColourAttrib->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) colourColourAttrib->attributeID);
        glVertexAttribPointer ((GLuint) colourColourAttrib->attributeID, 3, GL_FLOAT, GL_FALSE,
                               stride, (const void*) (sizeof (float) * 3));
    }

    glDrawElements (GL_TRIANGLES, meshIndexCount, GL_UNSIGNED_INT, nullptr);

    if (colourPositionAttrib != nullptr && colourPositionAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) colourPositionAttrib->attributeID);
    if (colourColourAttrib != nullptr && colourColourAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) colourColourAttrib->attributeID);

    glDisable (GL_POLYGON_OFFSET_FILL);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
}

void Spectrogram3DComponent::GlHost::drawFrequencyLabels()
{
    using namespace juce::gl;
    if (labelShader == nullptr || ! labelAtlasReady || owner.freqLabels.empty() || labelVbo == 0)
        return;

    struct V { float x, y, u, v; };
    std::vector<V> quads;
    quads.reserve (owner.freqLabels.size() * 6);

    const float scale = (float) openGLContext.getRenderingScale();
    const float pxW = 52.0f * scale / (float) juce::jmax (1, getWidth());
    const float pxH = 22.0f * scale / (float) juce::jmax (1, getHeight());
    // Convert pixel half-size to NDC
    const float halfW = pxW;
    const float halfH = pxH;

    for (const auto& lb : owner.freqLabels)
    {
        float ndcX = 0.0f, ndcY = 0.0f;
        if (! projectWorldToNdc (1.08f, 0.09f, lb.worldZ, ndcX, ndcY))
            continue;

        const float x0 = ndcX + 0.01f;
        const float x1 = x0 + halfW * 2.0f;
        const float y0 = ndcY - halfH;
        const float y1 = ndcY + halfH;

        quads.push_back ({ x0, y0, lb.u0, lb.v0 });
        quads.push_back ({ x1, y0, lb.u1, lb.v0 });
        quads.push_back ({ x1, y1, lb.u1, lb.v1 });
        quads.push_back ({ x0, y0, lb.u0, lb.v0 });
        quads.push_back ({ x1, y1, lb.u1, lb.v1 });
        quads.push_back ({ x0, y1, lb.u0, lb.v1 });
    }

    if (quads.empty())
        return;

    glBindBuffer (GL_ARRAY_BUFFER, labelVbo);
    glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (quads.size() * sizeof (V)), quads.data(), GL_DYNAMIC_DRAW);

    labelShader->use();
    setCornerUniforms (*labelShader);
    if (labelTexUniform != nullptr)
        labelTexUniform->set (0);
    labelAtlas.bind();
    glActiveTexture (GL_TEXTURE0);

    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable (GL_DEPTH_TEST);
    glDisable (GL_CULL_FACE);

    const GLsizei stride = (GLsizei) sizeof (V);
    if (labelPositionAttrib != nullptr && labelPositionAttrib->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) labelPositionAttrib->attributeID);
        glVertexAttribPointer ((GLuint) labelPositionAttrib->attributeID, 2, GL_FLOAT, GL_FALSE, stride, nullptr);
    }
    if (labelTexAttrib != nullptr && labelTexAttrib->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) labelTexAttrib->attributeID);
        glVertexAttribPointer ((GLuint) labelTexAttrib->attributeID, 2, GL_FLOAT, GL_FALSE,
                               stride, (const void*) (sizeof (float) * 2));
    }

    glDrawArrays (GL_TRIANGLES, 0, (GLsizei) quads.size());

    if (labelPositionAttrib != nullptr && labelPositionAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) labelPositionAttrib->attributeID);
    if (labelTexAttrib != nullptr && labelTexAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) labelTexAttrib->attributeID);

    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glDisable (GL_BLEND);
    glEnable (GL_DEPTH_TEST);
}

void Spectrogram3DComponent::GlHost::renderOpenGL()
{
    using namespace juce::gl;

    if (owner.transparentBackground)
    {
        uploadBackdropIfNeeded();
        glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    else
    {
        juce::OpenGLHelpers::clear (owner.getClearColour());
        glClear (GL_DEPTH_BUFFER_BIT);
    }

    if (! glReady || colourShader == nullptr || getWidth() < 2 || getHeight() < 2)
        return;

    if (owner.msaaEnabled)
        glEnable (GL_MULTISAMPLE);

    uploadMeshIfNeeded();
    ensureFloorGeometry();
    ensureLabelAtlas();

    const float scale = (float) openGLContext.getRenderingScale();
    glViewport (0, 0,
                juce::roundToInt ((float) getWidth() * scale),
                juce::roundToInt ((float) getHeight() * scale));

    if (owner.transparentBackground)
        drawBackdropAndTint();

    drawGroundAndGrid();
    drawMesh();
    drawFrequencyLabels();
}

void Spectrogram3DComponent::GlHost::mouseDown (const juce::MouseEvent& e)
{
    if (! hasKeyboardFocus (true))
        grabKeyboardFocus();
    owner.handleMouseDown (e.getEventRelativeTo (&owner));
}

void Spectrogram3DComponent::GlHost::mouseDrag (const juce::MouseEvent& e)
{
    owner.handleMouseDrag (e.getEventRelativeTo (&owner));
}

void Spectrogram3DComponent::GlHost::mouseUp (const juce::MouseEvent& e)
{
    owner.handleMouseUp (e);
}

void Spectrogram3DComponent::GlHost::mouseWheelMove (const juce::MouseEvent&,
                                                    const juce::MouseWheelDetails& wheel)
{
    owner.handleMouseWheel (wheel);
}

void Spectrogram3DComponent::GlHost::mouseDoubleClick (const juce::MouseEvent&)
{
    owner.handleDoubleClick();
}

bool Spectrogram3DComponent::GlHost::keyPressed (const juce::KeyPress& key)
{
    return owner.keyPressed (key);
}

//==============================================================================
Spectrogram3DComponent::Spectrogram3DComponent()
{
    setOpaque (false);
    setVisible (false);
    setInterceptsMouseClicks (true, true);
    setWantsKeyboardFocus (true);

    glHost = std::make_unique<GlHost> (*this);
    addChildComponent (*glHost);

    constrainer.setMinimumSize (220 + kShadowPadFloating * 2, 160 + kShadowPadFloating * 2);
    constrainer.setMaximumSize (4000, 3000);
    resizer = std::make_unique<juce::ResizableCornerComponent> (this, &constrainer);
    addAndMakeVisible (*resizer);
    resizer->setAlwaysOnTop (true);

    defaultCamera = getFactoryCameraState();
    camera = defaultCamera;
    applyChromeMode();
}

Spectrogram3DComponent::~Spectrogram3DComponent()
{
    stopTimer();
    resizer.reset();
    glHost.reset();
}

Spectrogram3DComponent::CameraState Spectrogram3DComponent::getFactoryCameraState() noexcept
{
    // ¾ view from above: pitch = elevation above the floor horizon (not a tilted orbit axis).
    // Distance places the eye roughly 3× mesh-height above the peaks.
    constexpr float pitchDeg = 35.0f;
    constexpr float lookY = kDefaultMeshHeight * 0.35f;
    constexpr float eyeHeightAbovePeaks = 3.0f * kDefaultMeshHeight;
    const float eyeY = kDefaultMeshHeight + eyeHeightAbovePeaks;
    const float pitchRad = juce::degreesToRadians (pitchDeg);
    const float distance = (eyeY - lookY) / juce::jmax (0.25f, std::sin (pitchRad));
    return { -40.0f, pitchDeg, distance, 0.0f, lookY, 0.0f };
}

void Spectrogram3DComponent::setThemeColors (SharedResources* r) noexcept
{
    theme = r;
    repaint();
}

void Spectrogram3DComponent::setResizeLimits (int maxW, int maxH) noexcept
{
    const int pad = getShadowPad() * 2;
    constrainer.setMinimumSize (220 + pad, 160 + pad);
    constrainer.setMaximumSize (juce::jmax (220 + pad, maxW),
                                juce::jmax (160 + pad, maxH));
}

void Spectrogram3DComponent::setMovementBounds (juce::Rectangle<int> parentLocalBounds) noexcept
{
    constrainer.setMinimumOnscreenAmounts (24, 24, 24, 24);
    // Keep the framed window inside the available content area.
    if (auto* parent = getParentComponent())
    {
        juce::ignoreUnused (parent);
        constrainer.setSizeLimits (constrainer.getMinimumWidth(),
                                   constrainer.getMinimumHeight(),
                                   juce::jmax (constrainer.getMinimumWidth(), parentLocalBounds.getWidth()),
                                   juce::jmax (constrainer.getMinimumHeight(), parentLocalBounds.getHeight()));
    }
}

int Spectrogram3DComponent::getShadowPad() const noexcept
{
    return getShadowPadForMode (chromeMode);
}

int Spectrogram3DComponent::getShadowPadForMode (ChromeMode mode) noexcept
{
    return mode == ChromeMode::docked ? kShadowPadDocked : kShadowPadFloating;
}

void Spectrogram3DComponent::setChromeMode (ChromeMode mode) noexcept
{
    if (chromeMode == mode)
        return;
    chromeMode = mode;
    applyChromeMode();
    resized();
    repaint();
}

void Spectrogram3DComponent::applyChromeMode() noexcept
{
    const bool floating = (chromeMode == ChromeMode::floating);
    if (resizer != nullptr)
        resizer->setVisible (floating && active);
}

void Spectrogram3DComponent::setMeshHeight (float heightWorld) noexcept
{
    const float h = juce::jlimit (kMinMeshHeight, kMaxMeshHeight, heightWorld);
    if (std::abs (h - meshHeight) < 1.0e-4f)
        return;
    meshHeight = h;
    camera.panY = lookAtY();
    defaultCamera.panY = lookAtY();
    invalidateMesh();
    if (active)
        updateMeshFromSource();
    if (glHost != nullptr)
        glHost->triggerRedraw();
    repaint();
}

void Spectrogram3DComponent::setActive (bool shouldBeActive) noexcept
{
    if (active == shouldBeActive)
        return;

    active = shouldBeActive;
    setAlwaysOnTop (false);
    setVisible (active);
    applyChromeMode();

    if (glHost != nullptr)
        glHost->setActive (active);

    if (active)
    {
        clampCamera();
        startTimerHz (30);
        updateMeshFromSource();
        if (transparentBackground)
            refreshBackdropSnapshot();
    }
    else
    {
        stopTimer();
    }

    repaint();
}

void Spectrogram3DComponent::setMeshQuality (MeshQuality q) noexcept
{
    if (meshQuality == q)
        return;
    meshQuality = q;
    invalidateMesh();
    if (active)
        updateMeshFromSource();
}

void Spectrogram3DComponent::setMultisamplingEnabled (bool shouldEnable) noexcept
{
    if (msaaEnabled == shouldEnable)
        return;

    msaaEnabled = shouldEnable;
    if (glHost != nullptr)
        glHost->reattachWithCurrentFormat();
    repaint();
}

void Spectrogram3DComponent::setTransparentBackground (bool shouldEnable) noexcept
{
    if (transparentBackground == shouldEnable)
        return;

    transparentBackground = shouldEnable;
    applyBackgroundTransparency();
    if (glHost != nullptr)
        glHost->triggerRedraw();
    repaint();
}

void Spectrogram3DComponent::applyBackgroundTransparency() noexcept
{
    setOpaque (! transparentBackground);
    if (glHost != nullptr)
        glHost->applyBackgroundTransparency();
}

void Spectrogram3DComponent::GlHost::applyBackgroundTransparency() noexcept
{
    // Backdrop is sampled on the message thread and drawn in renderOpenGL.
    setOpaque (true);
    openGLContext.setComponentPaintingEnabled (false);
}

void Spectrogram3DComponent::invalidateMesh() noexcept
{
    meshDb.clear();
    meshW = 0;
    meshH = 0;
    lastHistorySerial = 0;
    indicesValid = false;
    lastBrightness = -1.0f;
}

void Spectrogram3DComponent::recolourMesh() noexcept
{
    if (meshW < 2 || meshH < 2 || meshDb.empty() || lastBrightness < 0.0f)
        return;
    rebuildVerticesFromMeshDb (lastBrightness, lastMinDb, lastMaxDb);
    if (glHost != nullptr)
        glHost->triggerRedraw();
}

void Spectrogram3DComponent::resetCamera() noexcept
{
    seedDefaultOrientation();
    if (glHost != nullptr)
        glHost->triggerRedraw();
    repaint();
}

void Spectrogram3DComponent::saveAsDefaultView() noexcept
{
    clampCamera();
    defaultCamera = camera;
    if (onDefaultViewChanged != nullptr)
        onDefaultViewChanged();
}

void Spectrogram3DComponent::setDefaultCameraState (const CameraState& state) noexcept
{
    defaultCamera = state;
    camera = defaultCamera;
    clampCamera();
    defaultCamera = camera;
    if (glHost != nullptr)
        glHost->triggerRedraw();
    repaint();
}

void Spectrogram3DComponent::seedDefaultOrientation() noexcept
{
    camera = defaultCamera;
    clampCamera();
}

void Spectrogram3DComponent::clampCamera() noexcept
{
    // Pitch = elevation above the floor horizon. 0° ≈ edge-on, 90° = top-down.
    // Never allow negative elevation (that would put the camera under the floor).
    camera.pitchDeg = juce::jlimit (kMinPitchDeg, kMaxPitchDeg, camera.pitchDeg);
    camera.distance = juce::jlimit (0.35f, 14.0f, camera.distance);
    camera.panY = lookAtY();
}

juce::Matrix3D<float> Spectrogram3DComponent::getTurntableViewMatrix() const noexcept
{
    // Same composition style as JUCE's OpenGLDemo (T * R), constrained to a Y-up turntable:
    //   1) move look-at to origin
    //   2) yaw around world +Y  (spin)
    //   3) pitch around world +X (elevation) — keeps the floor horizontal
    //   4) pull back along view -Z
    //
    // World: +Y up, floor = XZ, mesh height = +Y. No custom lookAt / quaternion.
    const float yaw = juce::degreesToRadians (camera.yawDeg);
    const float pitch = juce::degreesToRadians (camera.pitchDeg);

    const auto toOrigin = juce::Matrix3D<float>::fromTranslation (
        { -camera.panX, -camera.panY, -camera.panZ });
    const auto rotYaw = juce::Matrix3D<float>::rotation ({ 0.0f, yaw, 0.0f });
    // Positive pitch elevates the camera above the floor (negative would go underneath).
    const auto rotPitch = juce::Matrix3D<float>::rotation ({ pitch, 0.0f, 0.0f });
    const auto pullBack = juce::Matrix3D<float>::fromTranslation (
        { 0.0f, 0.0f, -camera.distance });

    return pullBack * rotPitch * rotYaw * toOrigin;
}

juce::Colour Spectrogram3DComponent::getClearColour() const noexcept
{
    // Match expanded Osc/Gon soft fill (~90/255) when transparent background is on.
    constexpr float kSoftAlpha = 90.0f / 255.0f;
    if (theme != nullptr)
    {
        const auto base = transparentBackground ? theme->sharedColors.oscBackground
                                                : theme->sharedColors.pluginBackground.darker (0.15f);
        return transparentBackground ? base.withAlpha (kSoftAlpha) : base;
    }
    return transparentBackground ? juce::Colour::fromFloatRGBA (0.06f, 0.07f, 0.09f, kSoftAlpha)
                                 : juce::Colour (0xff12151a);
}

juce::Rectangle<int> Spectrogram3DComponent::getInnerFrameLocal() const noexcept
{
    return getLocalBounds().reduced (getShadowPad());
}

bool Spectrogram3DComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onEscape != nullptr)
            onEscape();
        return true;
    }

    if (key == juce::KeyPress ('f') || key == juce::KeyPress ('F'))
    {
        resetCamera();
        return true;
    }

    return false;
}

void Spectrogram3DComponent::resized()
{
    const auto inner = getInnerFrameLocal();
    const int pad = getShadowPad();
    if (glHost != nullptr)
    {
        glHost->setBounds (inner.reduced (chromeMode == ChromeMode::docked ? 1 : kGlInset));
        if (active)
            glHost->requestAttachAsync();
    }

    if (resizer != nullptr)
    {
        // Keep grip entirely in the shadow chrome so the GL HWND cannot cover it.
        resizer->setBounds (getWidth() - pad, getHeight() - pad, pad, pad);
        resizer->setVisible (chromeMode == ChromeMode::floating && active);
    }

    if (glHost != nullptr)
        glHost->triggerRedraw();

    if (onUserResized != nullptr && resizer != nullptr && resizer->isMouseButtonDown())
        onUserResized();
}

void Spectrogram3DComponent::paint (juce::Graphics& g)
{
    const auto inner = getInnerFrameLocal().toFloat();
    const float radius = chromeMode == ChromeMode::docked ? 4.0f : kCornerRadius;
    juce::Path panel;
    panel.addRoundedRectangle (inner, radius);

    if (chromeMode == ChromeMode::floating
        && (theme == nullptr || ! theme->disableGlowShadowEffects))
        panelShadow.render (g, panel);

    // Soft BG: frame chrome only — GL draws the sampled backdrop + tint.
    if (! transparentBackground)
    {
        g.setColour (getClearColour());
        g.fillPath (panel);
    }
    else
    {
        g.setColour (getClearColour().withMultipliedAlpha (0.35f));
        g.strokePath (panel, juce::PathStrokeType (chromeMode == ChromeMode::docked ? 1.0f : 1.2f));
    }

    g.setColour (juce::Colours::white.withAlpha (chromeMode == ChromeMode::docked ? 0.12f : 0.22f));
    g.strokePath (panel, juce::PathStrokeType (chromeMode == ChromeMode::docked ? 1.0f : 1.2f));
    if (chromeMode == ChromeMode::floating)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.strokePath (panel, juce::PathStrokeType (1.0f),
                      juce::AffineTransform::translation (0.0f, 1.0f));
    }

    if (active && glHost != nullptr && (glHost->hasContextFailed() || ! glHost->isGlReady()))
    {
        g.setColour (juce::Colours::whitesmoke.withAlpha (0.7f));
        g.setFont (12.0f);
        g.drawFittedText (glHost->hasContextFailed() ? "3D spectrogram unavailable"
                                                     : "Initialising 3D spectrogram...",
                          inner.toNearestInt().reduced (8),
                          juce::Justification::centred, 2);
    }
}

void Spectrogram3DComponent::timerCallback()
{
    if (! active)
        return;

    if (transparentBackground)
    {
        // Snapshot every other tick — EQ graph updates often enough at ~15 Hz.
        if ((++backdropSkipCounter & 1) != 0)
            refreshBackdropSnapshot();
    }

    updateMeshFromSource();
    if (glHost != nullptr)
    {
        glHost->triggerRedraw();
        if (active)
            glHost->requestAttachAsync();
    }
}

void Spectrogram3DComponent::refreshBackdropSnapshot()
{
    auto* parent = getParentComponent();
    if (parent == nullptr || glHost == nullptr || ! glHost->isShowing())
        return;

    const auto areaInParent = parent->getLocalArea (glHost.get(), glHost->getLocalBounds());
    if (areaInParent.getWidth() < 2 || areaInParent.getHeight() < 2)
        return;

    float snapScale = 1.0f;
    if (auto* display = juce::Desktop::getInstance().getDisplays()
                            .getDisplayForRect (parent->localAreaToGlobal (areaInParent)))
        snapScale = juce::jlimit (1.0f, 2.0f, (float) display->scale);

    // Hide the framed 3D window so the snapshot captures the EQ graph / Scope panes beneath.
    const bool wasVisible = isVisible();
    setVisible (false);
    auto snap = parent->createComponentSnapshot (areaInParent, true, snapScale);
    setVisible (wasVisible);

    if (! snap.isValid())
        return;

    const juce::ScopedLock sl (backdropLock);
    backdropImage = std::move (snap);
    backdropNeedsUpload = true;
}

bool Spectrogram3DComponent::isInMoveChrome (juce::Point<int> localPos) const noexcept
{
    if (chromeMode != ChromeMode::floating || ! active)
        return false;
    if (glHost != nullptr && glHost->getBounds().contains (localPos))
        return false;
    if (resizer != nullptr && resizer->getBounds().contains (localPos))
        return false;
    return getLocalBounds().contains (localPos);
}

void Spectrogram3DComponent::mouseDown (const juce::MouseEvent& e)
{
    movingByChrome = isInMoveChrome (e.getPosition()) && e.mods.isLeftButtonDown()
                     && ! e.mods.isPopupMenu();
    if (movingByChrome)
        moveDragger.startDraggingComponent (this, e);
}

void Spectrogram3DComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (! movingByChrome)
        return;
    moveDragger.dragComponent (this, e, &constrainer);
    if (onUserMoved != nullptr)
        onUserMoved();
}

void Spectrogram3DComponent::mouseUp (const juce::MouseEvent&)
{
    movingByChrome = false;
}

void Spectrogram3DComponent::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (isInMoveChrome (e.getPosition()) ? juce::MouseCursor::DraggingHandCursor
                                                     : juce::MouseCursor::NormalCursor);
}

//==============================================================================
void Spectrogram3DComponent::meshSizeForQuality (int& outW, int& outH) const noexcept
{
    switch (meshQuality)
    {
        case MeshQuality::low:    outW = 64;  outH = 32;  break;
        case MeshQuality::high:   outW = 192; outH = 96;  break;
        case MeshQuality::medium:
        default:                  outW = 128; outH = 64;  break;
    }
}

void Spectrogram3DComponent::fillMeshColumn (int meshCol, const float* histCol, int histH)
{
    if (histCol == nullptr || meshH <= 1 || histH <= 1
        || meshCol < 0 || meshCol >= meshW)
        return;

    for (int z = 0; z < meshH; ++z)
    {
        const int srcRow = (z * (histH - 1)) / (meshH - 1);
        const int row = histH - 1 - srcRow;
        meshDb[(size_t) meshCol * (size_t) meshH + (size_t) z] = histCol[row];
    }
}

void Spectrogram3DComponent::seedMeshFromHistory (const std::vector<float>& history, int histW, int histH)
{
    meshDb.assign ((size_t) meshW * (size_t) meshH, -120.0f);
    const int cols = juce::jmin (meshW, histW);
    const int srcStart = histW - cols;
    for (int i = 0; i < cols; ++i)
        fillMeshColumn (meshW - cols + i,
                        history.data() + (size_t) (srcStart + i) * (size_t) histH, histH);
}

void Spectrogram3DComponent::appendMeshColumnsFromHistory (const std::vector<float>& history,
                                                           int histW, int histH, int numNew)
{
    numNew = juce::jlimit (1, meshW, numNew);
    if (meshW > numNew)
    {
        std::memmove (meshDb.data(),
                      meshDb.data() + (size_t) numNew * (size_t) meshH,
                      (size_t) (meshW - numNew) * (size_t) meshH * sizeof (float));
    }

    const int srcStart = juce::jmax (0, histW - numNew);
    for (int i = 0; i < numNew; ++i)
    {
        const int srcCol = juce::jmin (histW - 1, srcStart + i);
        fillMeshColumn (meshW - numNew + i,
                        history.data() + (size_t) srcCol * (size_t) histH, histH);
    }
}

void Spectrogram3DComponent::ensureIndexBuffer (int w, int h)
{
    if (indicesValid && meshW == w && meshH == h && ! cpuIndices.empty())
        return;

    std::vector<uint32_t> inds;
    inds.reserve ((size_t) (w - 1) * (size_t) (h - 1) * 6);
    for (int z = 0; z < h - 1; ++z)
    {
        for (int x = 0; x < w - 1; ++x)
        {
            const uint32_t i0 = (uint32_t) (z * w + x);
            const uint32_t i1 = i0 + 1;
            const uint32_t i2 = i0 + (uint32_t) w;
            const uint32_t i3 = i2 + 1;
            inds.push_back (i0); inds.push_back (i2); inds.push_back (i1);
            inds.push_back (i1); inds.push_back (i2); inds.push_back (i3);
        }
    }

    const juce::ScopedLock sl (meshLock);
    cpuIndices.swap (inds);
    indicesValid = true;
}

void Spectrogram3DComponent::rebuildVerticesFromMeshDb (float brightness, float minDb, float maxDb)
{
    if (dataSource == nullptr || meshW < 2 || meshH < 2 || meshDb.empty())
        return;

    dataSource->refreshColourLutFor3D();
    ensureIndexBuffer (meshW, meshH);

    std::vector<Vertex> verts ((size_t) meshW * (size_t) meshH);
    const float denom = juce::jmax (1.0f, maxDb - minDb);

    for (int z = 0; z < meshH; ++z)
    {
        const float v = (float) z / (float) (meshH - 1);
        for (int x = 0; x < meshW; ++x)
        {
            const float u = (float) x / (float) (meshW - 1);
            const float db = meshDb[(size_t) x * (size_t) meshH + (size_t) z];
            const float norm = juce::jlimit (0.0f, 1.0f, (db - minDb) / denom);
            const auto c = dataSource->colourFromHistoryDb3D (db, brightness, minDb, maxDb);

            auto& vtx = verts[(size_t) z * (size_t) meshW + (size_t) x];
            vtx.x = u * 2.0f - 1.0f;
            vtx.y = norm * meshHeight;
            vtx.z = v * 2.0f - 1.0f;
            vtx.r = c.getFloatRed();
            vtx.g = c.getFloatGreen();
            vtx.b = c.getFloatBlue();
        }
    }

    const juce::ScopedLock sl (meshLock);
    cpuVertices.swap (verts);
    meshNeedsUpload = true;
}

void Spectrogram3DComponent::updateMeshFromSource()
{
    if (dataSource == nullptr || ! dataSource->isSpectrogramEnabled())
        return;

    std::vector<float> history;
    int histW = 0, histH = 0;
    float brightness = 1.0f, minDb = -90.0f, maxDb = -6.0f;
    dataSource->getHistorySnapshot (history, histW, histH, brightness, minDb, maxDb);
    if (histW < 2 || histH < 2 || history.empty())
        return;

    int wantW = 0, wantH = 0;
    meshSizeForQuality (wantW, wantH);
    wantW = juce::jmin (wantW, histW);
    wantH = juce::jmin (wantH, histH);

    const uint64_t serial = dataSource->getHistoryColumnSerial();
    const bool sizeChanged = (wantW != meshW || wantH != meshH || meshDb.empty());
    const bool historyReset = (serial < lastHistorySerial);
    const bool lookChanged = (brightness != lastBrightness || minDb != lastMinDb || maxDb != lastMaxDb);

    if (sizeChanged || historyReset)
    {
        meshW = wantW;
        meshH = wantH;
        indicesValid = false;
        seedMeshFromHistory (history, histW, histH);
        lastHistorySerial = serial;
        lastBrightness = brightness;
        lastMinDb = minDb;
        lastMaxDb = maxDb;
        rebuildVerticesFromMeshDb (brightness, minDb, maxDb);
        return;
    }

    if (serial > lastHistorySerial)
    {
        const int delta = (int) juce::jmin<uint64_t> ((uint64_t) meshW, serial - lastHistorySerial);
        appendMeshColumnsFromHistory (history, histW, histH, delta);
        lastHistorySerial = serial;
        lastBrightness = brightness;
        lastMinDb = minDb;
        lastMaxDb = maxDb;
        rebuildVerticesFromMeshDb (brightness, minDb, maxDb);
        return;
    }

    if (lookChanged)
    {
        lastBrightness = brightness;
        lastMinDb = minDb;
        lastMaxDb = maxDb;
        rebuildVerticesFromMeshDb (brightness, minDb, maxDb);
    }
}

float Spectrogram3DComponent::worldZForFreq (float hz, double sampleRate, bool logFreq) noexcept
{
    const float nyquist = (float) (juce::jmax (1.0, sampleRate) * 0.5);
    const float maxHz = juce::jmin (SpectrogramComponent::kMaxDisplayHz, nyquist * 0.999f);
    const float minHz = SpectrogramComponent::kMinDisplayHz;
    if (maxHz <= minHz + 1.0f)
        return 0.0f;

    hz = juce::jlimit (minHz, maxHz, hz);
    float t = 0.0f;
    if (logFreq)
        t = std::log (hz / minHz) / std::log (maxHz / minHz);
    else
        t = (hz - minHz) / (maxHz - minHz);
    return t * 2.0f - 1.0f;
}

juce::String Spectrogram3DComponent::formatGridHz (float hz)
{
    if (hz >= 1000.0f)
    {
        const float k = hz / 1000.0f;
        if (std::abs (k - std::round (k)) < 0.05f)
            return juce::String ((int) std::round (k)) + "k";
        return juce::String (k, 1) + "k";
    }
    return juce::String ((int) std::round (hz));
}

void Spectrogram3DComponent::rebuildFreqLabels (double sampleRate, bool logFreq)
{
    freqLabels.clear();
    const float nyquist = (float) (juce::jmax (1.0, sampleRate) * 0.5);
    const float maxHz = juce::jmin (SpectrogramComponent::kMaxDisplayHz, nyquist * 0.999f);

    for (float hz : kMajorHz)
    {
        if (hz < SpectrogramComponent::kMinDisplayHz || hz > maxHz)
            continue;
        FreqLabel lb;
        lb.hz = hz;
        lb.text = formatGridHz (hz);
        lb.worldZ = worldZForFreq (hz, sampleRate, logFreq);
        freqLabels.push_back (std::move (lb));
    }
    labelAtlasDirty = true;
}

//==============================================================================
void Spectrogram3DComponent::showContextMenu (juce::Point<int> screenPos)
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    menu.addItem (1, "Save as Default View");
    menu.addItem (2, "Reset Camera (F)");
    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 })
                            .withMousePosition(),
        [safe = juce::Component::SafePointer<Spectrogram3DComponent> (this)] (int result)
        {
            if (safe == nullptr || result <= 0)
                return;
            if (result == 1)
                safe->saveAsDefaultView();
            else if (result == 2)
                safe->resetCamera();
        });
}

void Spectrogram3DComponent::handleMouseDown (const juce::MouseEvent& e)
{
    lastDrag = e.position;

    if (e.mods.isPopupMenu())
    {
        dragMode = DragMode::none;
        showContextMenu (e.getScreenPosition());
        return;
    }

    // Turntable controls (no free tumble / roll):
    //  LMB drag        = orbit (yaw / elevation)
    //  Shift / MMB     = pan on the ground plane
    //  Alt / Ctrl+LMB  = dolly (distance)
    //  Wheel           = zoom / dolly
    if (e.mods.isMiddleButtonDown() || e.mods.isShiftDown())
        dragMode = DragMode::pan;
    else if (e.mods.isLeftButtonDown() && (e.mods.isAltDown() || e.mods.isCtrlDown() || e.mods.isCommandDown()))
        dragMode = DragMode::dolly;
    else if (e.mods.isLeftButtonDown())
        dragMode = DragMode::orbit;
    else
        dragMode = DragMode::none;
}

void Spectrogram3DComponent::handleMouseDrag (const juce::MouseEvent& e)
{
    if (dragMode == DragMode::none)
        return;

    const auto d = e.position - lastDrag;
    lastDrag = e.position;

    if (dragMode == DragMode::orbit)
    {
        // Yaw around world +Y; pitch = elevation only (floor stays down).
        camera.yawDeg -= d.x * 0.35f;
        camera.pitchDeg += d.y * 0.35f;
        clampCamera();
    }
    else if (dragMode == DragMode::pan)
    {
        // Slide the look-at across the floor (XZ), relative to current yaw.
        const float yaw = juce::degreesToRadians (camera.yawDeg);
        const float scale = 0.0025f * camera.distance;
        const float rightX = std::cos (yaw);
        const float rightZ = -std::sin (yaw);
        const float fwdX = -std::sin (yaw);
        const float fwdZ = -std::cos (yaw);
        // Reversed vs trackball "grab the floor": drag follows the mouse.
        camera.panX -= (rightX * d.x + fwdX * (-d.y)) * scale;
        camera.panZ -= (rightZ * d.x + fwdZ * (-d.y)) * scale;
        clampCamera();
    }
    else if (dragMode == DragMode::dolly)
    {
        camera.distance *= (1.0f + d.y * 0.005f);
        clampCamera();
    }

    if (glHost != nullptr)
        glHost->triggerRedraw();
}

void Spectrogram3DComponent::handleMouseUp (const juce::MouseEvent&)
{
    dragMode = DragMode::none;
}

void Spectrogram3DComponent::handleMouseWheel (const juce::MouseWheelDetails& wheel)
{
    camera.distance *= (1.0f - wheel.deltaY * 0.15f);
    clampCamera();
    if (glHost != nullptr)
        glHost->triggerRedraw();
}

void Spectrogram3DComponent::handleDoubleClick()
{
    resetCamera();
}
