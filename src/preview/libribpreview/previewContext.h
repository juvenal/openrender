#pragma once

#include "previewTypes.h"
#include "ribGeometryContext.h"

class CPreviewContext : public CRibGeometryContext {
public:
    CPreviewContext();
    ~CPreviewContext() override = default;

    void RiWorldBegin() override;
    void addObject(CObject *obj) override;

    PreviewScene result_;

private:
    void extractCamera();
    void updateBounds(const float3 &p);
};
