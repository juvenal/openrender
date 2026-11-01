/**
 * Project: openRender
 *
 * File: rendererShadingServices.cpp
 *
 * Description:
 *   Hosts the CRendererServicesImpl singleton and registers it with
 *   CShadingContext::setDefaultServices() at renderer startup. This file
 *   stays in src/ri/ so it can freely include renderer.h and
 *   rendererServicesImpl.h without creating a circular dependency with
 *   src/libshader/shading/.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva@v2-labs.press>
 *
 * Copyright (c) 2025 - 2026, Juvenal A. Silva Jr.
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <cstddef>
#include <cstring>

#include "rendererServicesImpl.h"
#include "ri_config.h"
#include "object.h"
#include "shader.h"
#include "shaderParameterHooks.h"
#include "shaderParameters.h"
#include "shading.h"
#include "shaderRefs.h"

// One instance per process; CRendererServicesImpl is stateless.
static CRendererServicesImpl g_rendererServicesImpl;

// Called by CRenderer::beginRenderer() to make the renderer services
// available to all CShadingContext instances for the whole session, and to
// install the shader-parameter hooks Graphics State calls.
void rendererShadingServicesInit() {
    CShadingContext::setDefaultServices(&g_rendererServicesImpl);

    // Graphics State asks Shading, through two function pointers, which parameters the bound
    // shaders need and which parameter a name refers to (spec 025). Installed here, not in
    // rendererDeclarations.cpp: ribVector compiles that file and does not link libshader_shading.
    installShaderParameterHooks(&libshader::shaderParameterHooks());
}

// Called last by CRenderer::endRenderer(): lookups after RiEnd() get the
// no-services result.
void rendererShadingServicesShutdown() {
    installShaderParameterHooks(nullptr);
    CShadingContext::setDefaultServices(nullptr);
}

// getMatrix(): the renderer answers every coordinate-system lookup. CRenderer::findCoordinateSystem() names the
// system and the kind; the object, shader and light systems come from the handles Shading passes, and the camera
// and current systems are the identity (Shading's own diagnostics and fallbacks stay in Shading). The time is
// ignored: no motion-blurred coordinate system exists to look up (spec 026, FR-008).
openrender::shading::MatrixAnswer CRendererServicesImpl::getMatrix(const openrender::shading::MatrixRequest &request) const {
    using namespace openrender::shading;
    MatrixAnswer answer;
    const float *from = nullptr;
    const float *to = nullptr;
    ECoordinateSystem kind = COORDINATE_CUSTOM;

    if (!CRenderer::findCoordinateSystem(request.name, from, to, kind)) {
        answer.status = EQueryStatus::NotFound;
        return answer;
    }

    switch (kind) {
        case COORDINATE_OBJECT:
            if (!request.object) {
                answer.status = EQueryStatus::NoObject;
                answer.kind = kind;
                return answer;
            }
            from = static_cast<const CObject *>(request.object.ptr)->xform->from;
            to = static_cast<const CObject *>(request.object.ptr)->xform->to;
            break;
        case COORDINATE_SHADER:
            assert(request.shader);
            from = static_cast<const CShaderInstance *>(request.shader.ptr)->xform->from;
            to = static_cast<const CShaderInstance *>(request.shader.ptr)->xform->to;
            break;
        case COORDINATE_LIGHT:
            assert(request.light);
            from = static_cast<const CShaderInstance *>(request.light.ptr)->xform->from;
            to = static_cast<const CShaderInstance *>(request.light.ptr)->xform->to;
            break;
        case COORDINATE_CAMERA:
        case COORDINATE_CURRENT:
            from = identityMatrix;
            to = identityMatrix;
            break;
        default:
            // world, NDC, raster, screen, custom and the colour names: as returned
            break;
    }

    answer.status = EQueryStatus::Found;
    answer.kind = kind;
    answer.from = from;
    answer.to = to;
    return answer;
}

///////////////////////////////////////////////////////////////////////
// getAttribute(): attribute(), option() and rendererinfo() (spec 026, FR-005)
//
// The name tables are CShadingContext::attributes(), options() and rendererInfo() as they were in Shading,
// moved here: the same names, casts and order of checks. A computed answer is written into
// answer.inlineValue; a stored one is returned by pointer into the renderer's own storage. Shading
// copies it (and refuses a TypeMismatch).
///////////////////////////////////////////////////////////////////////
namespace {

    // Option names that are defined and responded
    const char *const optionsFormat = "Format";
    const char *const optionsDeviceFrame = "Frame";
    const char *const optionsDeviceResolution = "DeviceResolution";
    const char *const optionsFrameAspectRatio = "FrameAspectRatio";
    const char *const optionsCropWindow = "CropWindow";
    const char *const optionsDepthOfField = "DepthOfField";
    const char *const optionsShutter = "Shutter";
    const char *const optionsClipping = "Clipping";
    const char *const optionsBucketSize = "BucketSize";
    const char *const optionsColorQuantizer = "ColorQuantizer";
    const char *const optionsDepthQuantizer = "DepthQuantizer";
    const char *const optionsPixelFilter = "PixelFilter";
    const char *const optionsGamma = "Gamma";
    const char *const optionsMaxRayDepth = "MaxRayDepth";
    const char *const optionsRelativeDetail = "RelativeDetail";
    const char *const optionsPixelSamples = "PixelSamples";
    const char *const optionsSamplingSampler = "sampling:sampler"; // spec 039
    const char *const optionsSamplingSeed = "sampling:seed";

    // Attributes that are defined and responded
    const char *const attributesShadingRate = "ShadingRate";
    const char *const attributesSides = "Sides";
    const char *const attributesMatte = "matte";
    const char *const attributesMotionfactor = "GeometricApproximation:motionfactor";
    const char *const attributesDisplacementBnd = "displacementbound:sphere";
    const char *const attributesDisplacementSys = "displacementbound:coordinatesystem";
    const char *const attributesName = "identifier:name";

    const char *const attributesTraceBias = "trace:bias";
    const char *const attributesTraceMaxDiffuse = "trace:maxdiffusedepth";
    const char *const attributesTraceMaxSpecular = "trace:maxspeculardepth";

    const char *const attributesUser = "user:";

    // Rendererinfo requests
    const char *const rendererinfoRenderer = "renderer";
    const char *const rendererinfoVersion = "version";

    // The renderer's name, a string with static storage: the answer points at this slot
    char *const s_rendererName = const_cast<char *>(OPENRENDER_PROJECT_NAME);

    static_assert(offsetof(openrender::shading::AttributeValue, inlineValue) % alignof(const char *) == 0,
                  "a computed string answer is stored as pointers in inlineValue");

    // A computed answer of n floats
    inline void setFloats(openrender::shading::AttributeValue &a, int n, float v0, float v1 = 0, float v2 = 0, float v3 = 0) {
        a.status = openrender::shading::EQueryStatus::Found;
        a.type = TYPE_FLOAT;
        a.count = n;
        a.inlineValue[0] = v0;
        a.inlineValue[1] = v1;
        a.inlineValue[2] = v2;
        a.inlineValue[3] = v3;
        a.data = a.inlineValue;
    }

    // A string stored in the renderer: the answer points at its slot
    inline void setStringSlot(openrender::shading::AttributeValue &a, char *const *slot) {
        a.status = openrender::shading::EQueryStatus::Found;
        a.type = TYPE_STRING;
        a.count = 1;
        a.data = slot;
    }

    // A user attribute or option: the variable's own storage
    inline void setUserVariable(openrender::shading::AttributeValue &a, const CVariable *var) {
        a.status = openrender::shading::EQueryStatus::Found;
        a.type = var->type;
        a.count = var->numFloats;
        a.data = var->defaultValue;
    }

    // FR-026: the stored value cannot be copied safely into a destination of the expected type
    inline bool copyIsUnsafe(const openrender::shading::AttributeValue &a, openrender::shading::QueryType expected) {
        using openrender::shading::QueryType;
        const bool storedIsString = (a.type == TYPE_STRING);
        switch (expected) {
            case QueryType::String:
                return !storedIsString;
            case QueryType::Triple:
                return storedIsString || a.count > 3;
            case QueryType::Matrix:
                return storedIsString || a.count > 16;
            case QueryType::Float:
            default:
                return storedIsString;
        }
    }

} // namespace

void CRendererServicesImpl::getAttribute(const openrender::shading::AttributeRequest &request, openrender::shading::AttributeValue &answer) const {
    using namespace openrender::shading;
    const char *name = request.name;

    answer.status = EQueryStatus::NotFound;

    switch (request.scope) {
        case AttributeScope::Attribute:
        {
            // No object (the imager): nothing to read (FR-027)
            if (!request.object)
                return;
            const CAttributes *currentAttributes = static_cast<const CObject *>(request.object.ptr)->attributes;

            if (strcmp(name, attributesShadingRate) == 0) {
                setFloats(answer, 1, (float)currentAttributes->shadingRate);
            }
            else if (strcmp(name, attributesSides) == 0) {
                setFloats(answer, 1, (float)(currentAttributes->flags & ATTRIBUTES_FLAGS_DOUBLE_SIDED ? 2 : 1));
            }
            else if (strcmp(name, attributesMatte) == 0) {
                setFloats(answer, 1, (float)((currentAttributes->flags & ATTRIBUTES_FLAGS_MATTE) != 0));
            }
            else if (strcmp(name, attributesMotionfactor) == 0) {
                setFloats(answer, 1, (float)currentAttributes->motionFactor);
            }
            else if (strcmp(name, attributesDisplacementBnd) == 0) {
                setFloats(answer, 1, (float)currentAttributes->maxDisplacement);
            }
            else if (strcmp(name, attributesDisplacementSys) == 0) {
                setStringSlot(answer, &currentAttributes->maxDisplacementSpace);
            }
            else if (strcmp(name, attributesName) == 0) {
                setStringSlot(answer, &currentAttributes->name);
            }
            // Additional attributes
            else if (strcmp(name, attributesTraceBias) == 0) {
                setFloats(answer, 1, (float)currentAttributes->bias);
            }
            else if (strcmp(name, attributesTraceMaxDiffuse) == 0) {
                setFloats(answer, 1, (float)currentAttributes->maxDiffuseDepth);
            }
            else if (strcmp(name, attributesTraceMaxSpecular) == 0) {
                setFloats(answer, 1, (float)currentAttributes->maxSpecularDepth);
            }
            // User attributes
            else if (strncmp(name, attributesUser, strlen(attributesUser)) == 0) {
                CVariable *var;

                if (currentAttributes->userAttributes.lookup(name + strlen(attributesUser), var) == TRUE)
                    setUserVariable(answer, var);
            }
            break;
        }

        case AttributeScope::Option:
        {
            if (strcmp(name, optionsFormat) == 0) {
                setFloats(answer, 3, (float)CRenderer::xres, (float)CRenderer::yres, 1.f);
            }
            else if (strcmp(name, optionsDeviceFrame) == 0) {
                setFloats(answer, 1, (float)CRenderer::frame);
            }
            else if (strcmp(name, optionsDeviceResolution) == 0) {
                setFloats(answer, 3, (float)CRenderer::xres, (float)CRenderer::yres, 1.f);
            }
            else if (strcmp(name, optionsFrameAspectRatio) == 0) {
                setFloats(answer, 1, CRenderer::frameAR);
            }
            else if (strcmp(name, optionsCropWindow) == 0) {
                setFloats(answer, 4, CRenderer::cropLeft, CRenderer::cropTop, CRenderer::cropRight, CRenderer::cropBottom);
            }
            else if (strcmp(name, optionsDepthOfField) == 0) {
                setFloats(answer, 3, CRenderer::fstop, CRenderer::focallength, CRenderer::focaldistance);
            }
            else if (strcmp(name, optionsShutter) == 0) {
                setFloats(answer, 2, CRenderer::shutterOpen, CRenderer::shutterClose);
            }
            else if (strcmp(name, optionsClipping) == 0) {
                setFloats(answer, 2, CRenderer::clipMin, CRenderer::clipMax);
            }
            else if (strcmp(name, optionsBucketSize) == 0) {
                setFloats(answer, 2, CRenderer::bucketWidth, CRenderer::bucketHeight);
            }
            else if (strcmp(name, optionsColorQuantizer) == 0) {
                const float *q = CRenderer::colorQuantizer;
                setFloats(answer, 4, q[0], q[1], q[2], q[3]);
            }
            else if (strcmp(name, optionsDepthQuantizer) == 0) {
                const float *q = CRenderer::depthQuantizer;
                setFloats(answer, 4, q[0], q[1], q[2], q[3]);
            }
            else if (strcmp(name, optionsPixelFilter) == 0) {
                setFloats(answer, 2, CRenderer::pixelFilterWidth, CRenderer::pixelFilterHeight);
            }
            else if (strcmp(name, optionsGamma) == 0) {
                setFloats(answer, 2, CRenderer::gamma, CRenderer::gain);
            }
            else if (strcmp(name, optionsMaxRayDepth) == 0) {
                setFloats(answer, 1, (float)CRenderer::maxRayDepth);
            }
            else if (strcmp(name, optionsRelativeDetail) == 0) {
                setFloats(answer, 1, CRenderer::relativeDetail);
            }
            else if (strcmp(name, optionsPixelSamples) == 0) {
                setFloats(answer, 2, (float)CRenderer::pixelXsamples, (float)CRenderer::pixelYsamples);
            }
            else if (strcmp(name, optionsSamplingSampler) == 0) {
                // the sampler's name has static storage; the answer holds a pointer to it
                const char *sampler = openrender::sampling::samplerName(CRenderer::samplingBackend);
                memcpy(answer.inlineValue, &sampler, sizeof(sampler));
                answer.status = EQueryStatus::Found;
                answer.type = TYPE_STRING;
                answer.count = 1;
                answer.data = answer.inlineValue;
            }
            else if (strcmp(name, optionsSamplingSeed) == 0) {
                setFloats(answer, 1, (float)(int)CRenderer::samplingSeed);
            }
            else if (strncmp(name, attributesUser, strlen(attributesUser)) == 0) {
                CVariable *var;
                if (CRenderer::userOptions != NULL && CRenderer::userOptions->lookup(name + strlen(attributesUser), var) != FALSE)
                    setUserVariable(answer, var);
            }
            break;
        }

        case AttributeScope::RendererInfo:
        {
            if (strcmp(name, rendererinfoRenderer) == 0) {
                setStringSlot(answer, &s_rendererName);
            }
            else if (strcmp(name, rendererinfoVersion) == 0) {
                setFloats(answer, 4, (float)VERSION_MAJOR, (float)VERSION_MINOR, (float)VERSION_PATCH, (float)0);
            }
            break;
        }
    }

    if (answer.status == EQueryStatus::Found && copyIsUnsafe(answer, request.expected))
        answer.status = EQueryStatus::TypeMismatch;
}

///////////////////////////////////////////////////////////////////////
// getMessage(): surface(), displacement(), atmosphere(), incident(), opposite(), lightsource() (spec 026, FR-006)
//
// The instance is the one bound to the object's attributes (or the light handle for the Light source); the answer
// is its getParameter(), called as Shading called it. For the five object sources the stored shape of the
// parameter is looked up first, without writing, so that a value that cannot be copied safely into a destination of
// the expected type (FR-026) is answered TypeMismatch before anything is written into request.dest. The Light source
// is exempt (B4): lightsource() keeps each backend's own output.
///////////////////////////////////////////////////////////////////////
openrender::shading::MessageAnswer CRendererServicesImpl::getMessage(const openrender::shading::MessageRequest &request) const {
    using namespace openrender::shading;
    MessageAnswer answer;
    answer.status = EQueryStatus::NotFound;

    CShaderInstance *instance = nullptr;
    if (request.source == MessageSource::Light) {
        instance = const_cast<CShaderInstance *>(static_cast<const CShaderInstance *>(request.light.ptr));
    }
    else {
        // No object (the imager): nothing to read (FR-027)
        if (!request.object)
            return answer;
        const CAttributes *currentAttributes = static_cast<const CObject *>(request.object.ptr)->attributes;
        switch (request.source) {
            case MessageSource::Surface:
                if (currentAttributes->surface())
                    instance = shaderInstanceOf(currentAttributes->surface());
                break;
            case MessageSource::Displacement:
                if (currentAttributes->displacement())
                    instance = shaderInstanceOf(currentAttributes->displacement());
                break;
            case MessageSource::Atmosphere:
                if (currentAttributes->atmosphere())
                    instance = shaderInstanceOf(currentAttributes->atmosphere());
                break;
            case MessageSource::Interior:
                if (currentAttributes->interior())
                    instance = shaderInstanceOf(currentAttributes->interior());
                break;
            case MessageSource::Exterior:
                if (currentAttributes->exterior())
                    instance = shaderInstanceOf(currentAttributes->exterior());
                break;
            default:
                break;
        }
    }
    if (instance == nullptr)
        return answer;

    if (request.source != MessageSource::Light) {
        EVariableType type = TYPE_FLOAT;
        int numFloats = 0;
        if (instance->getParameterShape(request.name, type, numFloats)) {
            const bool storedIsString = (type == TYPE_STRING);
            bool unsafe;
            switch (request.expected) {
                case QueryType::String:
                    unsafe = !storedIsString;
                    break;
                case QueryType::Triple:
                    unsafe = storedIsString || numFloats > 3;
                    break;
                case QueryType::Matrix:
                    unsafe = storedIsString || numFloats > 16;
                    break;
                case QueryType::Float:
                default:
                    unsafe = storedIsString;
                    break;
            }
            if (unsafe) {
                answer.status = EQueryStatus::TypeMismatch;
                return answer;
            }
        }
    }

    CVariable *variable = nullptr;
    int globalIndex = -1;
    const int found = instance->getParameter(request.name, request.dest, request.wantVariable ? &variable : nullptr, request.wantVariable ? &globalIndex : nullptr);
    if (found != FALSE) {
        answer.status = EQueryStatus::Found;
        answer.variable = variable;
        answer.globalIndex = request.wantVariable ? globalIndex : -1;
    }
    return answer;
}
