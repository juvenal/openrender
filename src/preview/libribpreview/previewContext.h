#pragma once

#include "previewTypes.h"
#include "rendererContext.h"

class CPreviewContext : public CRendererContext {
public:
    CPreviewContext();
    ~CPreviewContext() override = default;

    void RiDisplayV(const char *name, const char *type, const char *mode,
                    int n, const char *tokens[], const void *params[]) override;
    void RiWorldBegin() override;
    void RiWorldEnd() override;
    void addObject(CObject *obj) override;

    PreviewScene result_;

private:
    void extractCamera();
    void updateBounds(const float3 &p);
};
