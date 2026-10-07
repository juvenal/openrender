// Declared locally: the dependency exists only at link level.
int traceRay(int depth);

int lookupOcclusion(int depth) {
    return traceRay(depth);
}
