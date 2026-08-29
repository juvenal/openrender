// Shared helpers for the RiBlobby unit tests (spec
// 015-blobby-implicit-surfaces). Provides the minimal test harness, a
// builder that assembles well-formed and deliberately malformed code
// arrays, a diagnostic recorder so "was this rejected, and did it say
// why?" is assertable, and implementation-independent mesh invariants
// (edge manifoldness, Euler characteristic, distance to an analytic
// surface).
//
// Mirrors tests/unit/csg/csgTestUtils.h, which spec 013 established.
#ifndef BLOBBY_TEST_UTILS_H
#define BLOBBY_TEST_UTILS_H

#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "blobbyField.h"
#include "blobbyPolygonize.h"
#include "riInterface.h"

namespace blobbytest {

///////////////////////////////////////////////////////////////////////
// Diagnostics
//
// error() dispatches through renderMan->RiError(), and renderMan is NULL
// outside a renderer context, so every test that can provoke a diagnostic
// must install one of these first. It doubles as the recorder the
// rejection tests assert against.
///////////////////////////////////////////////////////////////////////
inline std::vector<std::string> &messages() {
	static std::vector<std::string>	log;
	return log;
}

inline void recordError(int,int,const char *message) {
	messages().push_back(message == NULL ? "" : message);
}

// Installs the recorder and clears the log. Safe to call repeatedly; the
// CRiInterface is created once and leaked deliberately, since it must
// outlive every test in the process.
inline void beginCapture() {
	static CRiInterface	*iface	=	NULL;

	if (iface == NULL)	iface	=	new CRiInterface;

	renderMan	=	iface;
	renderMan->RiErrorHandler(recordError);
	messages().clear();
}

inline int sawDiagnostic() {
	return (int) messages().size();
}

// TRUE when some recorded diagnostic contains `needle`. Rejection tests
// use this to check the message names the offending thing, not merely
// that something was printed (FR-029, FR-031).
inline bool diagnosticMentions(const char *needle) {
	for (size_t i=0;i<messages().size();i++) {
		if (messages()[i].find(needle) != std::string::npos)	return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////////////
// Code-array builder
///////////////////////////////////////////////////////////////////////
class CBuilder {
	public:
		std::vector<int>			code;
		std::vector<float>			floats;
		std::vector<const char *>	strings;

		// Walks the code array counting instructions and primitive fields.
		// Deliberately defensive: several tests build deliberately
		// malformed arrays (a negative operand count, an operand count
		// that overruns the array), and the *builder* must not be the
		// thing that hangs on them -- that would mask whether the
		// validator handles them.
		void walk(int *instructionCount,int *leafCount) const {
			int		n		=	0;
			int		leaves	=	0;
			size_t	i		=	0;

			while (i < code.size()) {
				const int	op		=	code[i];
				size_t		advance;

				if (op >= 1000) {
					advance	=	(op == 1003) ? 3 : 2;
					leaves++;
				} else if (op >= 6)		advance	=	2;
				else if (op >= 4)		advance	=	3;
				else if (op >= 0) {
					if (i+1 >= code.size())	break;
					if (code[i+1] < 0)		break;
					advance	=	(size_t) 2 + (size_t) code[i+1];
				} else					advance	=	2;

				i	+=	advance;
				n++;
			}

			if (instructionCount != NULL)	*instructionCount	=	n;
			if (leafCount != NULL)			*leafCount			=	leaves;
		}

		// Number of instructions emitted so far; the next instruction's
		// result reference.
		int next() const {
			int	n;
			walk(&n,NULL);
			return n;
		}

		int constant(float v) {
			const int	idx	=	(int) floats.size();
			floats.push_back(v);
			const int	me	=	next();
			code.push_back(1000);
			code.push_back(idx);
			return me;
		}

		// Ellipsoid from an explicit 4x4, laid out exactly as RIB gives it.
		int ellipsoid(const float *m) {
			const int	idx	=	(int) floats.size();
			for (int i=0;i<16;i++)	floats.push_back(m[i]);
			const int	me	=	next();
			code.push_back(1001);
			code.push_back(idx);
			return me;
		}

		// Unit sphere translated to (x,y,z) and uniformly scaled by
		// `radius` -- the shape every published example uses.
		int sphere(float x,float y,float z,float radius=1.0f) {
			float	m[16]	=	{radius,0,0,0,  0,radius,0,0,  0,0,radius,0,  x,y,z,1};
			return ellipsoid(m);
		}

		int segment(const float *p0,const float *p1,float radius,const float *m=NULL) {
			const int	idx	=	(int) floats.size();
			for (int i=0;i<3;i++)	floats.push_back(p0[i]);
			for (int i=0;i<3;i++)	floats.push_back(p1[i]);
			floats.push_back(radius);
			if (m != NULL) {
				for (int i=0;i<16;i++)	floats.push_back(m[i]);
			} else {
				const float	identity[16]	=	{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
				for (int i=0;i<16;i++)	floats.push_back(identity[i]);
			}
			const int	me	=	next();
			code.push_back(1002);
			code.push_back(idx);
			return me;
		}

		int repeller(const char *file,float A,float B,float C,float D) {
			const int	sidx	=	(int) strings.size();
			strings.push_back(file);
			const int	fidx	=	(int) floats.size();
			floats.push_back(A);
			floats.push_back(B);
			floats.push_back(C);
			floats.push_back(D);
			const int	me	=	next();
			code.push_back(1003);
			code.push_back(sidx);
			code.push_back(fidx);
			return me;
		}

		int nary(int opcode,const std::vector<int> &operands) {
			const int	me	=	next();
			code.push_back(opcode);
			code.push_back((int) operands.size());
			for (size_t i=0;i<operands.size();i++)	code.push_back(operands[i]);
			return me;
		}

		int add(const std::vector<int> &o)		{ return nary(0,o); }
		int multiply(const std::vector<int> &o)	{ return nary(1,o); }
		int maximum(const std::vector<int> &o)	{ return nary(2,o); }
		int minimum(const std::vector<int> &o)	{ return nary(3,o); }

		int binary(int opcode,int a,int b) {
			const int	me	=	next();
			code.push_back(opcode);
			code.push_back(a);
			code.push_back(b);
			return me;
		}

		int unary(int opcode,int a) {
			const int	me	=	next();
			code.push_back(opcode);
			code.push_back(a);
			return me;
		}

		int negate(int a)	{ return unary(6,a); }
		int identity(int a)	{ return unary(7,a); }

		// nleaf < 0 means "declare the truthful count", which is what
		// well-formed input does.
		CBlobbyProgram *build(int nleaf=-1,EBlobbyOpcodeOrder order=BLOBBY_ORDER_RISPEC) const {
			int	leaves	=	nleaf;

			if (leaves < 0)	walk(NULL,&leaves);

			return new CBlobbyProgram(leaves,
									  (int) code.size(),
									  code.empty() ? NULL : &code[0],
									  (int) floats.size(),
									  floats.empty() ? NULL : &floats[0],
									  (int) strings.size(),
									  strings.empty() ? NULL : &strings[0],
									  order);
		}
};

///////////////////////////////////////////////////////////////////////
// Mesh invariants
///////////////////////////////////////////////////////////////////////

// Vertices are shared through the polygonizer's edge cache, so a
// watertight mesh has every undirected edge used by exactly two
// triangles. Returns the number of edges that are not.
inline int countNonManifoldEdges(const CBlobbyMesh *mesh) {
	std::map<std::pair<int,int>,int>	uses;
	int								bad	=	0;

	for (int t=0;t<mesh->numTriangles;t++) {
		for (int e=0;e<3;e++) {
			int	a	=	mesh->triangles[t*3+e];
			int	b	=	mesh->triangles[t*3+(e+1)%3];

			if (a > b) { const int tmp = a; a = b; b = tmp; }
			uses[std::make_pair(a,b)]++;
		}
	}

	for (std::map<std::pair<int,int>,int>::const_iterator it=uses.begin();it!=uses.end();++it) {
		if (it->second != 2)	bad++;
	}

	return bad;
}

inline int countEdges(const CBlobbyMesh *mesh) {
	std::set<std::pair<int,int> >	edges;

	for (int t=0;t<mesh->numTriangles;t++) {
		for (int e=0;e<3;e++) {
			int	a	=	mesh->triangles[t*3+e];
			int	b	=	mesh->triangles[t*3+(e+1)%3];

			if (a > b) { const int tmp = a; a = b; b = tmp; }
			edges.insert(std::make_pair(a,b));
		}
	}

	return (int) edges.size();
}

// V - E + F. A closed surface of genus g gives 2 - 2g; a sphere gives 2.
// Counts only vertices actually referenced, since the polygonizer may
// cache a vertex on an edge no emitted triangle ends up using.
inline int eulerCharacteristic(const CBlobbyMesh *mesh) {
	std::set<int>	used;

	for (int i=0;i<mesh->numTriangles*3;i++)	used.insert(mesh->triangles[i]);

	return (int) used.size() - countEdges(mesh) + mesh->numTriangles;
}

// Connected components of the mesh, over triangle adjacency through
// shared vertices. This is the geometric counterpart of the threshold's
// field-value bracket (SC-003).
inline int countComponents(const CBlobbyMesh *mesh) {
	std::map<int,std::vector<int> >	adjacency;

	for (int t=0;t<mesh->numTriangles;t++) {
		for (int e=0;e<3;e++)	adjacency[mesh->triangles[t*3+e]].push_back(t);
	}

	std::vector<int>	seen(mesh->numTriangles,0);
	int					comps	=	0;

	for (int t=0;t<mesh->numTriangles;t++) {
		if (seen[t])	continue;

		comps++;
		std::vector<int>	stack;
		stack.push_back(t);
		seen[t]	=	1;

		while (!stack.empty()) {
			const int	cur	=	stack.back();
			stack.pop_back();

			for (int e=0;e<3;e++) {
				const std::vector<int>	&nbrs	=	adjacency[mesh->triangles[cur*3+e]];

				for (size_t k=0;k<nbrs.size();k++) {
					if (!seen[nbrs[k]]) { seen[nbrs[k]] = 1; stack.push_back(nbrs[k]); }
				}
			}
		}
	}

	return comps;
}

// Largest deviation of any vertex from the sphere of radius `radius`
// centred at (cx,cy,cz).
inline double maxSphereDeviation(const CBlobbyMesh *mesh,float cx,float cy,float cz,float radius) {
	double	worst	=	0.0;

	for (int i=0;i<mesh->numVertices;i++) {
		const double	dx	=	mesh->P[i*3+0]-cx;
		const double	dy	=	mesh->P[i*3+1]-cy;
		const double	dz	=	mesh->P[i*3+2]-cz;
		const double	d	=	sqrt(dx*dx+dy*dy+dz*dz);

		if (fabs(d-radius) > worst)	worst	=	fabs(d-radius);
	}

	return worst;
}

// Largest angle, in radians, between a vertex normal and the outward
// radial direction of the sphere it should lie on.
inline double maxSphereNormalError(const CBlobbyMesh *mesh,float cx,float cy,float cz) {
	double	worst	=	0.0;

	for (int i=0;i<mesh->numVertices;i++) {
		double	d[3]	=	{mesh->P[i*3+0]-cx, mesh->P[i*3+1]-cy, mesh->P[i*3+2]-cz};
		double	dl		=	sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);

		if (dl < 1e-9)	continue;

		double	n[3]	=	{mesh->N[i*3+0], mesh->N[i*3+1], mesh->N[i*3+2]};
		double	nl		=	sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);

		if (nl < 1e-9)	continue;

		double	c	=	(d[0]*n[0]+d[1]*n[1]+d[2]*n[2])/(dl*nl);

		if (c >  1.0)	c =  1.0;
		if (c < -1.0)	c = -1.0;

		// The field decreases outward, so the gradient points inward:
		// compare against the radial direction up to sign.
		const double	ang	=	acos(fabs(c));

		if (ang > worst)	worst	=	ang;
	}

	return worst;
}

// Distance from a point to the segment [a,b].
inline double distanceToSegment(const float *p,const float *a,const float *b) {
	double	ab[3]	=	{(double)b[0]-a[0], (double)b[1]-a[1], (double)b[2]-a[2]};
	double	ap[3]	=	{(double)p[0]-a[0], (double)p[1]-a[1], (double)p[2]-a[2]};
	double	len2	=	ab[0]*ab[0]+ab[1]*ab[1]+ab[2]*ab[2];
	double	t		=	(len2 > 0) ? (ap[0]*ab[0]+ap[1]*ab[1]+ap[2]*ab[2])/len2 : 0.0;

	if (t < 0)	t = 0;
	if (t > 1)	t = 1;

	double	d[3]	=	{ap[0]-t*ab[0], ap[1]-t*ab[1], ap[2]-t*ab[2]};

	return sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
}

// Largest deviation of any vertex from the capsule of radius `radius`
// about the segment [a,b].
inline double maxCapsuleDeviation(const CBlobbyMesh *mesh,const float *a,const float *b,float radius) {
	double	worst	=	0.0;

	for (int i=0;i<mesh->numVertices;i++) {
		const double	d	=	distanceToSegment(mesh->P+i*3,a,b);

		if (fabs(d-radius) > worst)	worst	=	fabs(d-radius);
	}

	return worst;
}

// The radius at which a lone bump field crosses the threshold:
// (1-r^2)^3 = T. Every absolute-radius assertion is stated in terms of
// this rather than a literal, so the threshold's derivation stays the
// single source of truth.
inline double lonelyBlobRadius(double threshold=BLOBBY_THRESHOLD) {
	return sqrt(1.0 - pow(threshold,1.0/3.0));
}

}  // namespace blobbytest

///////////////////////////////////////////////////////////////////////
// Test harness (same shape as tests/unit/csg/)
///////////////////////////////////////////////////////////////////////
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name)                              \
    void test_##name();                         \
    void run_test_##name() {                    \
        printf("Running test: %s ... ", #name); \
        fflush(stdout);                         \
        int failedBefore = tests_failed;        \
        test_##name();                          \
        if (tests_failed == failedBefore) {     \
            tests_passed++;                     \
            printf("PASSED\n");                 \
        }                                       \
    }                                           \
    void test_##name()

#define ASSERT(condition)                                          \
    do {                                                           \
        if (!(condition)) {                                        \
            printf("\nAssertion failed: %s\nFile: %s, Line: %d\n", \
                   #condition, __FILE__, __LINE__);                \
            tests_failed++;                                        \
            return;                                                \
        }                                                          \
    } while (0)

#define REPORT(suite)                                                       \
    do {                                                                    \
        printf("\n=== %s: %d passed, %d failed ===\n", suite, tests_passed, \
               tests_failed);                                               \
    } while (0)

#endif
