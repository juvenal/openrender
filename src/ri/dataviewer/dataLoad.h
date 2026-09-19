#ifndef DATALOAD_H
#define DATALOAD_H

class CDataView;

// One of the six visualizable file-content types, or a reason CDataDocument::open() refused
// to construct a document. Never determined from a filename/extension -- content only.
enum EDataFileType {
    DATA_UNKNOWN = 0,     // sniff not yet attempted / no file open
    DATA_NOT_A_DATA_FILE, // no magic number, and not a parseable debug-geometry dump either
    DATA_PHOTONMAP,
    DATA_IRRADIANCECACHE,
    DATA_GATHERCACHE,
    DATA_POINTCLOUD,
    DATA_BRICKMAP,
    DATA_DEBUGDUMP,
    DATA_BAD_VERSION,  // magic matched; VERSION_MAJOR/MINOR mismatch
    DATA_BAD_WORDSIZE, // magic matched; sizeof(int *) mismatch
};

// Side-effect-free: determines a file's data-structure type from content alone. Never touches
// CRenderer state, never installs a sink. Returns DATA_NOT_A_DATA_FILE for a RIB scene (this
// function makes no attempt at RIB parsing).
EDataFileType dataSniff(const char *fileName);

// Loads exactly one precomputed data-structure file outside of a render. Owns the CDataView it
// constructs for the CDataDocument's own lifetime (never via CRenderer::getPhotonMap/getCache/
// getTexture3d, which assert on mid-render-only state).
class CDataDocument {
    public:
        CDataDocument();
        ~CDataDocument();

        // On success, view() becomes non-NULL and the return value is one of the six
        // DATA_* content types. On failure, view() stays NULL and the return value is
        // DATA_NOT_A_DATA_FILE, DATA_BAD_VERSION, or DATA_BAD_WORDSIZE.
        EDataFileType open(const char *fileName);

        CDataView *view() const { return dataView; }

    private:
        CDataView *dataView;
        int declarationsInitialized;
};

#endif
