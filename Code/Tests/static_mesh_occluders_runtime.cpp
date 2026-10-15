// [TEMP] [CHOSEN, not recovered]
// Tests production loadOccluders against extracted stock .occ files using an isolated zeroed template buffer.
// Supplies heap, logging, and stream collaborators; other force-linked paths must remain uncalled.
// Checks counts, normalized planes, anchor equations, and edge/point references.
// Usage: static_mesh_occluders_runtime.exe <occ-directory>; zero exit means all files passed.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <windows.h>

#include "../RendDX9Abi/HostFileAbi.h"
#include "../BF2/Geom/StaticMeshTemplate.h"


// The four externals loadOccluders reaches. Real signatures, fixture bodies.

namespace renddx9abi {

void* engineAlloc(unsigned int size) { return ::malloc(size ? size : 1u); }
void  engineFree(void* p)            { ::free(p); }

int g_logCount = 0;
void logf(const char* fmt, ...)
{
    ++g_logCount;
    va_list args;
    va_start(args, fmt);
    ::printf("      [log] ");
    ::vprintf(fmt, args);
    ::printf("\n");
    va_end(args);
}

} // namespace renddx9abi


// A HostStream over a byte buffer. Only +0x1C (getAvailableBytes) and +0x10 (read) are used.

namespace {

class MemoryStream : public ::renddx9abi::HostStream
{
public:
    MemoryStream(const unsigned char* data, unsigned int size)
        : m_data(data), m_size(size), m_pos(0) {}

    virtual void         addRef()  {}
    virtual int          getRef()  { return 1; }
    virtual int          release() { return 1; }
    virtual void*        queryInterface(unsigned int) const { return 0; }

    virtual unsigned int read(void* out, unsigned int n)
    {
        if (n > m_size - m_pos)
            n = m_size - m_pos;
        ::memcpy(out, m_data + m_pos, n);
        m_pos += n;
        return n;
    }
    virtual unsigned int write(const void*, unsigned int) { return 0; }
    virtual int          skip(unsigned int)               { return 0; }
    virtual int          getAvailableBytes()              { return static_cast<int>(m_size - m_pos); }
    virtual ::renddx9abi::HostStream* clone()             { return 0; }
    virtual bool         canRead()                        { return true; }
    virtual bool         canWrite()                       { return false; }
    virtual void         flushWrite()                     {}

private:
    const unsigned char* m_data;
    unsigned int         m_size;
    unsigned int         m_pos;
};

int g_failures = 0;

void fail(const char* file, const char* what)
{
    ::printf("  FAIL  %-44s %s\n", file, what);
    ++g_failures;
}

// Walks the records loadOccluders built through their typed StaticMeshOccluder* members.
void checkGroups(const char* name,
                 const dice::hfe::geom::StaticMeshTemplate::OccluderGroupVector& groups)
{
    using dice::hfe::geom::StaticMeshOccluderGroup;
    using dice::hfe::geom::StaticMeshOccluderPlane;

    const unsigned int groupCount = static_cast<unsigned int>(groups.size());
    if (groupCount == 0)
    {
        fail(name, "no occluder group was produced");
        return;
    }

    unsigned int totalPlanes = 0;
    for (unsigned int g = 0; g < groupCount; ++g)
    {
        const StaticMeshOccluderGroup* const group = &groups[g];

        const unsigned int planeCount = static_cast<unsigned int>(group->planes.size());
        const unsigned int pointCount = static_cast<unsigned int>(group->points.size());
        const unsigned int edgeCount  = static_cast<unsigned int>(group->edges.size());
        totalPlanes += planeCount;

        if (planeCount == 0) { fail(name, "group has no planes"); continue; }
        if (pointCount == 0) { fail(name, "group has no points"); continue; }
        if (edgeCount == 0)  { fail(name, "group has no edges");  continue; }

        // Every edge endpoint must address a real point.
        for (unsigned int e = 0; e < edgeCount; ++e)
        {
            const unsigned char* const edge = group->edges[e].points;
            if (edge[0] >= pointCount || edge[1] >= pointCount)
                fail(name, "edge endpoint out of range");
        }

        for (unsigned int p = 0; p < planeCount; ++p)
        {
            const StaticMeshOccluderPlane& plane = group->planes[p];
            const float* const eq = plane.plane;
            const std::vector<unsigned short>& refs = plane.edgeReferences;

            const unsigned int refCount = static_cast<unsigned int>(refs.size());
            if (refCount != 4)
            {
                fail(name, "plane does not carry exactly four edge refs");
                continue;
            }

            // Every ref must address a real edge; the edge index is the reference's low byte.
            for (unsigned int r = 0; r < 4; ++r)
            {
                if ((refs[r] & 0xFFu) >= edgeCount)
                    fail(name, "plane edge ref out of range");
            }

            // The stored normal is normalised by the body.
            const double len = ::sqrt(static_cast<double>(
                eq[0] * eq[0] + eq[1] * eq[1] + eq[2] * eq[2]));
            if (::fabs(len - 1.0) > 1e-4)
                fail(name, "plane normal is not unit length");

            // `d` is measured through edges[ref0].v0 - the anchor the body documents. If the
            // pointer arithmetic in the plane pass were wrong, this would not close.
            const unsigned char* const edge0 = group->edges[refs[0] & 0xFFu].points;
            const float* const anchor = &group->points[edge0[0]].x;
            const double residual =
                eq[0] * anchor[0] + eq[1] * anchor[1] + eq[2] * anchor[2] + eq[3];
            // The mesh coordinates run to a few tens of units, so scale the tolerance with them.
            double scale = 1.0;
            for (unsigned int k = 0; k < 3; ++k)
                if (::fabs(static_cast<double>(anchor[k])) > scale)
                    scale = ::fabs(static_cast<double>(anchor[k]));
            if (::fabs(residual) > 1e-4 * scale)
                fail(name, "dot(n, anchor) + d is not zero");
        }
    }

    ::printf("  ok    %-44s groups=%u planes=%u\n", name, groupCount, totalPlanes);
}

void checkFile(const char* name, const unsigned char* data, unsigned int size)
{
    // Zeroed storage, cast to the template; loadOccluders touches m_occluderGroups only. Zero bytes
    // are an empty std::vector under this build's _ITERATOR_DEBUG_LEVEL=0 STL.
    static unsigned char storage[sizeof(dice::hfe::geom::StaticMeshTemplate)];
    ::memset(storage, 0, sizeof(storage));
    dice::hfe::geom::StaticMeshTemplate* const tmpl =
        reinterpret_cast<dice::hfe::geom::StaticMeshTemplate*>(storage);

    MemoryStream stream(data, size);
    tmpl->loadOccluders(&stream);

    checkGroups(name, tmpl->m_occluderGroups);
    // Release this file's records before the storage is zeroed for the next one.
    dice::hfe::geom::StaticMeshTemplate::OccluderGroupVector().swap(tmpl->m_occluderGroups);
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        ::printf("usage: static_mesh_occluders_runtime.exe <dir-of-.occ-files>\n");
        return 2;
    }

    char pattern[MAX_PATH];
    ::_snprintf(pattern, sizeof(pattern) - 1, "%s\\*.occ", argv[1]);
    pattern[sizeof(pattern) - 1] = '\0';

    WIN32_FIND_DATAA found;
    HANDLE handle = ::FindFirstFileA(pattern, &found);
    if (handle == INVALID_HANDLE_VALUE)
    {
        ::printf("no .occ files under %s\n", argv[1]);
        return 2;
    }

    int files = 0;
    do
    {
        char path[MAX_PATH];
        ::_snprintf(path, sizeof(path) - 1, "%s\\%s", argv[1], found.cFileName);
        path[sizeof(path) - 1] = '\0';

        FILE* f = ::fopen(path, "rb");
        if (!f)
            continue;
        ::fseek(f, 0, SEEK_END);
        const long size = ::ftell(f);
        ::fseek(f, 0, SEEK_SET);
        unsigned char* const data = static_cast<unsigned char*>(::malloc(size > 0 ? size : 1));
        const size_t got = ::fread(data, 1, static_cast<size_t>(size), f);
        ::fclose(f);

        checkFile(found.cFileName, data, static_cast<unsigned int>(got));
        ::free(data);
        ++files;
    }
    while (::FindNextFileA(handle, &found));
    ::FindClose(handle);

    ::printf("\n%d file(s), %d failure(s), %d EDGE ERROR log line(s)\n",
             files, g_failures, ::renddx9abi::g_logCount);
    if (::renddx9abi::g_logCount != 0)
        ::printf("NOTE: an EDGE ERROR on stock content would mean the edge-orientation reading is "
                 "backwards - see addOccluderEdge.\n");
    return (g_failures == 0 && ::renddx9abi::g_logCount == 0) ? 0 : 1;
}
