// [TEMP] Real serialized navmesh-query fixture. Whole production query/geometry/material TUs;
// six exact MapContainer/outer definitions. The old loader's chosen File/game/DP observers are
// reused unchanged. AIPathfinding/settings lifetimes are chosen; owners survive process exit
// because production QuadTreeManager destruction remains a frontier. No server is launched.
#include "map_polygon_runtime.h"
#define main navmesh_loader_fixture_main
#include "navmesh_loader_runtime.cpp"
#undef main
#include <cmath>
#include <limits>
#include <functional>

template<class Tag, typename Tag::type Member>
struct PrivateMember { friend typename Tag::type access(Tag) { return Member; } };
struct MapsTag {
    using type=std::vector<AIPathfinding::MapContainer*> AIPathfinding::*;
    friend type access(MapsTag);
};
template struct PrivateMember<MapsTag, &AIPathfinding::m_maps>;
static std::vector<AIPathfinding::MapContainer*>& maps(AIPathfinding& path) {
    return path.*access(MapsTag{});
}

namespace dice { namespace hfe { namespace ai {
// [TEMP] Native chosen lifetime: setup/search engines are outside this query fixture.
AIPathfinding::AIPathfinding() {}
AIPathfinding::~AIPathfinding() {}
IAISettings::IAISettings() {}
IAISettings::~IAISettings() {}
void* CommonSingletonServer::m_iaiSettings = nullptr;
} } }
#include "unused.inl"
#include "settings.inl"
MaterialManager* QuerySettings::getVehicleMaterialManager(int id) const {
    ++calls; vehicle=id;
    if(replacement) {
        // The receiver must be looked up after the virtual settings callback.
        std::vector<AIPathfinding::MapContainer*> fresh(1,replacement);
        maps(*path).swap(fresh);
    }
    return result;
}
static unsigned checks=0;
#define CHECK(x) do { ++checks; if(!(x)) { std::fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x); std::exit(90); } } while(0)
static const unsigned sentinel=0x13579BDFu;
static const unsigned invalid=0xFFFFFFFFu;
using Map=AIPathfinding::MapContainer;
static Map* load(const char* name) {
    Map* map=new Map(name,7);
    CHECK(map->m_id==7 && map->m_name==name);
    CHECK(map->m_properties && map->m_quadTreeManager);
    CHECK(map->m_properties->m_allowedHeightDiff==2.5f);
    map->m_quadTreeManager->loadQuadTrees(name);
    CHECK(map->m_quadTreeManager->getNumberOfIslands()==1);
    return map;
}
static void query(Map* map,MaterialManager* materials,Vec3 p,bool expected,unsigned poly=0,unsigned island=0) {
    unsigned actualPoly=sentinel, actualIsland=0x2468ACE0u;
    CHECK(map->getPolygon(p,actualPoly,actualIsland,materials)==expected);
    CHECK(actualPoly==(expected?poly:sentinel));
    CHECK(actualIsland==(expected?island:invalid));
}
static void append(Map* destination,Map* from) {
    destination->m_quadTreeManager->m_quadTrees.push_back(from->m_quadTreeManager->m_quadTrees[0]);
    from->m_quadTreeManager->m_quadTrees.clear();
}
int main(int argc,char** argv) {
    SetErrorMode(SEM_NOGPFAULTERRORBOX|SEM_FAILCRITICALERRORS);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    setDebugCallback(DebugTypeError,debugCallback,nullptr);
    if(argc!=3) unexpected("usage: MODE OUT");
    std::string mode=argv[1];
    levelDirectory=std::string(argv[2])+"/data"; game.m_levelPath=levelDirectory;
    CommonSingletonServer::m_iaiGame=static_cast<IAIGame*>(&game);
    CommonSingletonServer::m_iDPManager=static_cast<IDPManager*>(&drawPrimitives);
    MaterialManager materials;
    if(mode=="empty_manager") {
        Map* map=new Map("unused",3);
        query(map,nullptr,{0,0,0},false);
        CHECK(map->m_quadTreeManager->getNumberOfIslands()==0);
    } else if(mode=="empty_tree" || mode=="empty_leaf") {
        Map* map;
        if(mode=="empty_tree") {
            map=new Map("never-loaded",0);
            map->m_quadTreeManager->m_quadTrees.push_back(std::make_pair(new QuadTree(0),static_cast<ClusterGraph*>(nullptr)));
        } else map=load("empty_leaf");
        query(map,nullptr,{0,0,0},false);
    } else if(mode=="leaf_success") {
        Map* map=load("one");
        query(map,&materials,{0,0,0},true);
        query(map,&materials,{-2,0,2},true);
        unsigned p=invalid,i=invalid;
        CHECK(map->getPolygon({1,0,1},p,i,&materials));
        CHECK(p==0 && i==0);
    } else if(mode=="bounds") {
        Map* map=load("edge");
        for(Vec3 p: {Vec3{-4,0,-4},Vec3{-4,0,4},Vec3{4,0,-4},Vec3{4,0,4}})
            query(map,&materials,p,true);
        for(Vec3 p: {Vec3{-4.0001f,0,0},Vec3{4.0001f,0,0},Vec3{0,0,-4.0001f},Vec3{0,0,4.0001f}})
            query(map,nullptr,p,false);
        Map* smaller=load("one");
        query(smaller,&materials,{3.001f,0,0},false); // inside root bounds, outside polygon
        query(smaller,&materials,{3.0001f,0,0},true); // 6*0.0001 < 0.001 edge tolerance
    } else if(mode=="materials") {
        Map* map=load("blocked");
        query(map,&materials,{0,0,0},false);
        materials.setMaterialCost(0,-0.0f); query(map,&materials,{0,0,0},true);
        materials.setMaterialCost(0,std::numeric_limits<float>::quiet_NaN()); query(map,&materials,{0,0,0},true);
        materials.setMaterialCost(0,std::numeric_limits<float>::infinity()); query(map,&materials,{0,0,0},true);
    } else if(mode=="normal_filter") {
        query(load("downward"),nullptr,{0,0,0},false);
        query(load("zero_normal"),nullptr,{0,0,0},false);
    } else if(mode=="height_above") {
        Map* map=load("one");
        query(map,&materials,{0,2.5f,0},true);
        query(map,&materials,{0,std::nextafter(2.5f,3.f),0},false);
        map->m_properties->m_allowedHeightDiff=0.5f;
        query(map,&materials,{0,0.5f,0},true);
        query(map,&materials,{0,0.50001f,0},false);
        map->m_properties->m_allowedHeightDiff=-2.f;
        query(map,&materials,{0,2,0},true); // tolerance is squared, not sign-guarded
    } else if(mode=="height_below") {
        Map* map=load("one"); map->m_properties->m_allowedHeightDiff=100.f;
        query(map,&materials,{0,-1,0},true);
        query(map,&materials,{0,std::nextafter(-1.f,-2.f),0},false);
        map->m_properties->m_allowedHeightDiff=0.f;
        query(map,&materials,{0,-0.5f,0},true);
        query(map,&materials,{0,0,0},true);
    } else if(mode=="closest") {
        Map* map=load("closest");
        query(map,&materials,{0,0.4f,0},true,2);
        query(map,&materials,{0,-0.1f,0},true,1);
    } else if(mode=="tie") {
        query(load("tie"),&materials,{0,0,0},true,1);
    } else if(mode=="filtered_nearest") {
        query(load("filter"),&materials,{0,0,0},true,2);
    } else if(mode=="quadrants") {
        Map* map=load("quadrants");
        query(map,&materials,{-2,0,-2},true,0);
        query(map,&materials,{-2,0,2},true,1);
        query(map,&materials,{2,0,-2},true,2);
        query(map,&materials,{2,0,2},true,3);
    } else if(mode=="quadrant_boundary") {
        Map* map=load("quadrants");
        query(map,&materials,{0,0,-2},true,2);
        query(map,&materials,{-2,0,0},true,1);
        query(map,&materials,{0,0,0},true,3);
    } else if(mode=="missing_child") {
        Map* map=load("missing_child");
        query(map,nullptr,{-2,0,2},false);
        query(map,&materials,{2,0,2},true,3);
    } else if(mode=="island_iteration") {
        Map* map=load("blocked"); append(map,load("one"));
        query(map,&materials,{0,0,0},true,0,1);
        materials.setMaterialCost(1,-1); query(map,&materials,{0,0,0},false);
    } else if(mode=="first_island") {
        Map* map=load("one"); append(map,load("closest"));
        query(map,&materials,{0,0.4f,0},true,0,0); // first accepting island wins, not global nearest
    } else if(mode=="known_reuse") {
        Map* map=load("one"); append(map,load("one"));
        unsigned polygon=sentinel;
        unsigned island=map->m_quadTreeManager->findPolygon(
            {0,0,0},polygon,0,1,&materials,map->m_properties->m_allowedHeightDiff);
        CHECK(island==1 && polygon==0); // an accepted known pair bypasses the earlier island
        materials.setMaterialCost(1,std::numeric_limits<float>::quiet_NaN());
        polygon=sentinel;
        island=map->m_quadTreeManager->findPolygon(
            {0,0,0},polygon,0,1,&materials,map->m_properties->m_allowedHeightDiff);
        CHECK(island==0 && polygon==0); // known-id >= 0 rejects NaN; leaf rejection form accepts it
    } else if(mode=="alias_outputs") {
        Map* map=load("blocked"); append(map,load("quadrants"));
        unsigned same=sentinel;
        CHECK(map->getPolygon({2,0,2},same,same,&materials));
        CHECK(same==1); // polygon 3 overwritten by returned island 1
        same=sentinel; materials.setMaterialCost(1,-1);
        CHECK(!map->getPolygon({2,0,2},same,same,&materials)); CHECK(same==invalid);
    } else if(mode=="outer_invalid") {
        AIPathfinding* path=new AIPathfinding;
        CommonSingletonServer::m_iaiSettings=nullptr;
        for(int index: {-1,0,0x7fffffff}) {
            unsigned p=sentinel,i=sentinel;
            CHECK(!path->getPolygon({0,0,0},index,p,i,-44)); CHECK(p==sentinel && i==sentinel);
        }
        maps(*path).push_back(load("one"));
        unsigned p=sentinel,i=sentinel;
        CHECK(!path->getPolygon({0,0,0},1,p,i,0)); CHECK(p==sentinel && i==sentinel);
    } else if(mode=="outer_routing" || mode=="outer_reload") {
        AIPathfinding* path=new AIPathfinding;
        maps(*path).push_back(load(mode=="outer_reload"?"blocked":"quadrants"));
        QuerySettings settings; settings.result=&materials; settings.path=path;
        CommonSingletonServer::m_iaiSettings=&settings;
        if(mode=="outer_reload") settings.replacement=load("quadrants");
        unsigned p=sentinel,i=sentinel;
        CHECK(path->getPolygon({2,0,2},0,p,i,-12345));
        CHECK(p==3 && i==0); CHECK(settings.calls==1 && settings.vehicle==-12345);
    } else if(mode=="closest_valid_polygon") {
        Map* map=load("sample_target");
        unsigned island=sentinel,polygon=sentinel; Vec3 closest={91,92,93};

        CHECK(!map->findClosestValidPolygon(island,polygon,closest,
                                             {1,2,3},{9,8,7},0,&materials));
        CHECK(island==invalid && polygon==invalid);
        CHECK(closest.x==1 && closest.y==2 && closest.z==3);

        island=sentinel; polygon=sentinel; closest={91,92,93};
        CHECK(map->findClosestValidPolygon(island,polygon,closest,
                                            {0,0,0},{10,0,0},3,&materials));
        CHECK(island==0 && polygon==0);
        CHECK(closest.x==5 && closest.y==0 && closest.z==0);

        island=sentinel; polygon=sentinel; closest={91,92,93};
        CHECK(map->findClosestValidPolygon(island,polygon,closest,
                                            {0,0,0},{5,0,0},2,&materials));
        CHECK(island==0 && polygon==0 && closest.x==5);

        island=sentinel; polygon=sentinel; closest={91,92,93};
        CHECK(!map->findClosestValidPolygon(island,polygon,closest,
                                             {0,0,0},{2,0,0},3,&materials));
        CHECK(island==invalid && polygon==invalid);
        CHECK(closest.x==3 && closest.y==0 && closest.z==0); // one step beyond `to`

        island=sentinel; polygon=sentinel; closest={91,92,93};
        CHECK(!map->findClosestValidPolygon(island,polygon,closest,
                                             {0,0,0},{5,6,7},1,&materials));
        CHECK(island==invalid && polygon==invalid);
        CHECK(closest.x==5 && closest.y==6 && closest.z==7); // full step after one failed sample

        Map* quadrants=load("quadrants"); unsigned same=sentinel;
        CHECK(quadrants->findClosestValidPolygon(same,same,closest,
                                                  {2,0,2},{2,0,2},1,&materials));
        CHECK(same==0); // polygon 3 is written first, then island 0 overwrites the alias
    } else if(mode=="plane") {
        Plane3 plane={{0,1,0},-2}; Vec3 hit={91,92,93}; bool segment=false;
        CHECK(!plane.intersectLine({0,0,0},{1,0,1},hit,segment));
        CHECK(hit.x==91 && hit.y==92 && hit.z==93 && !segment);
        CHECK(plane.intersectLine({1,3,4},{1,1,4},hit,segment));
        CHECK(segment && hit.x==1 && hit.y==2 && hit.z==4);
        CHECK(plane.intersectLine({1,0,4},{1,1,4},hit,segment));
        CHECK(!segment && hit.y==2);
        CHECK(plane.intersectLine({1,3,4},{1,4,4},hit,segment));
        CHECK(!segment && hit.y==2);
        CHECK(plane.intersectLine({1,2,4},{1,3,4},hit,segment)); CHECK(segment && hit.y==2);
        CHECK(plane.intersectLine({1,1,4},{1,2,4},hit,segment)); CHECK(segment && hit.y==2);
        Vec3 a={2,3,4}; CHECK(plane.intersectLine(a,{2,1,4},a,segment)); CHECK(a.y==2 && a.x==2 && a.z==4);
    } else if(mode=="distance_limit") {
        Map* map=load("limit"); map->m_properties->m_allowedHeightDiff=20000;
        query(map,&materials,{0,0,0},false); // equal to initial best 1e8 is never selected
        query(map,&materials,{0,-1,0},true); // strictly less succeeds
    } else if(mode=="stock") {
        Map* map=load("stock");
        #include "stock_samples.inl"
        CHECK(sizeof(samples)/sizeof(samples[0])==32);
        for(const Vec3& p:samples) {
            unsigned polygon=invalid,island=invalid;
            CHECK(map->getPolygon(p,polygon,island,&materials)); CHECK(island==0 && polygon!=invalid);
            std::vector<const Vec3*> vertices;
            map->m_quadTreeManager->getIsland(island)->getVerticiesOfPolygon(vertices,polygon);
            CHECK(vertices.size()>=3);
        }
    } else unexpected("unknown mode");
    std::printf("PASS %s %u checks\n",mode.c_str(),checks); return 0;
}
