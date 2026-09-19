#pragma once
#include "dataScene.h"
#include "ri/dataviewer/dataView.h"

// Adapts CDataView's static sink protocol (dataView.h) into a DataScene, applying the
// deterministic even-stride decimation cap decided in spec clarification (research.md §6).
class CDataSceneSink : public CPrimitiveSink {
    public:
        explicit CDataSceneSink(DataScene &scene);

        void triangles(int n, const float *P, const float *C) override;
        void triangleMesh(int n, const int *indices, const float *P, const float *C) override;
        void lines(int n, const float *P, const float *C) override;
        void points(int n, const float *P, const float *C) override;
        void disks(int n, const float *P, const float *dP, const float *N, const float *C) override;

    private:
        DataScene &scene;
};

// Fills in bounds/camera/documentType/channel/detail/draw-mode from `view`, draws it through a
// fresh CDataSceneSink into `scene`, and applies the decimation cap. Disc records accumulate in
// `scene.disks` pre-expansion, then diskExpand.h expands the surviving discs into
// `scene.triVerts`/`triCols`. Called by ribdata_open()/ribdata_key().
void buildDataScene(CDataView *view, RibDataType documentType, DataScene &scene);
