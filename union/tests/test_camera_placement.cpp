// Synthetic collision scenes exercise the actual portable placement solver.
// No Gothic assets, executable or SDK ABI are needed by these tests.
#include "../hud/CameraPlacement.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

using namespace ParryHotter;

namespace {
struct Box { CameraPoint low, high; };

CameraRayHit Intersect(CameraPoint from, CameraPoint to, const Box& box) {
    const float starts[]{from.x, from.y, from.z};
    const float ends[]{to.x, to.y, to.z};
    const float lows[]{box.low.x, box.low.y, box.low.z};
    const float highs[]{box.high.x, box.high.y, box.high.z};
    float enter = 0.0f, leave = 1.0f;
    for (int axis=0; axis<3; ++axis) {
        const float delta = ends[axis]-starts[axis];
        if (std::fabs(delta)<0.000001f) {
            if (starts[axis]<lows[axis] || starts[axis]>highs[axis]) return {};
            continue;
        }
        float a=(lows[axis]-starts[axis])/delta;
        float b=(highs[axis]-starts[axis])/delta;
        if (a>b) std::swap(a,b);
        enter=std::max(enter,a);
        leave=std::min(leave,b);
        if (enter>leave) return {};
    }
    return {true, enter};
}

struct Scene {
    std::vector<Box> boxes;
    CameraRayHit operator()(CameraPoint from, CameraPoint to) const {
        CameraRayHit result;
        for (const auto& box:boxes) {
            const auto hit=Intersect(from,to,box);
            if (hit.hit && (!result.hit || hit.fraction<result.fraction)) result=hit;
        }
        return result;
    }
};

bool Close(float a,float b) { return std::fabs(a-b)<0.01f; }
CameraPlacementInput Input(float seconds=1.0f/60.0f) {
    return {true,{0.0f,180.0f,0.0f},0.0f,0.0f,seconds,18.0f};
}

void CheckPose(const Scene& scene,const CameraPlacementOutput& out,float radius=18.0f) {
    // The nine actual sample paths at yaw=pitch=0 must remain on the clear
    // side of every obstacle. The near-plane corners can hit a wall even
    // while the center ray misses it entirely.
    const float diagonal=0.70710678118f;
    const float samples[][2]={{0,0},{1,0},{-1,0},{0,1},{0,-1},
        {diagonal,diagonal},{diagonal,-diagonal},{-diagonal,diagonal},{-diagonal,-diagonal}};
    for (const auto& sample:samples) {
        const CameraPoint offset{sample[0]*radius,sample[1]*radius,0};
        const auto hit=scene(out.pivot+offset,out.position+offset);
        assert(!hit.hit);
    }
}
} // namespace

int main() {
    CameraPlacement camera;
    const Scene open;
    auto out=camera.Step(Input(),open);
    assert(out.active && Close(out.distance,300) && Close(out.pivotAboveHead,75));
    assert(Close(out.position.y,255) && Close(out.position.z,-300));
    assert(Close(out.direction.x,0) && Close(out.direction.z,1));
    assert(out.traceCount==18 && out.sampleHits==0);

    // A normal back wall lowers the pivot as it pulls the camera in.
    const Scene wall{{{{-1000,-1000,-110},{1000,1000,-100}}}};
    out=camera.Step(Input(),wall);
    assert(out.collisionLimited && Close(out.distance,92));
    assert(Close(out.pivotAboveHead,23));
    CheckPose(wall,out);

    // No artificial minimum may place the camera through a close wall.
    const Scene closeWall{{{{-1000,-1000,-25},{1000,1000,-20}}}};
    out=camera.Step(Input(),closeWall);
    assert(out.distance<40 && Close(out.distance,12));
    CheckPose(closeWall,out);

    // The old head+75 pivot would be inside this roof; the upward trace
    // starts below Harry's head and catches it before selecting the pivot.
    const Scene lowRoof{{{{-1000,210,-1000},{1000,230,1000}}}};
    camera.Reset();
    out=camera.Step(Input(),lowRoof);
    assert(out.ceilingLimited && Close(out.pivotAboveHead,4));
    assert(Close(out.distance,300) && out.position.y+18<210);
    CheckPose(lowRoof,out);
    const Scene veryLowRoof{{{{-1000,190,-1000},{1000,230,1000}}}};
    out=camera.Step(Input(),veryLowRoof);
    assert(out.ceilingLimited && Close(out.pivotAboveHead,-16));
    CheckPose(veryLowRoof,out);

    // A post beside the center ray still blocks the near-plane sample.
    const Scene corner{{{{10,140,-150},{30,300,-130}}}};
    assert(!corner({0,255,0},{0,255,-300}).hit);
    camera.Reset();
    out=camera.Step(Input(),corner);
    assert(out.sampleHits>0 && out.distance<=122.01f);
    CheckPose(corner,out);

    // Lowering the pivot reveals a second obstacle below the first one.
    const Scene stacked{{
        {{-100,220,-120},{100,300,-100}},
        {{-100,175,-60},{100,220,-40}}
    }};
    camera.Reset();
    out=camera.Step(Input(),stacked);
    assert(out.distance<=32.01f);
    CheckPose(stacked,out);

    // Smooth return follows a different, lower path than the desired pose.
    // Checking only the desired head+75 pose would miss this intermediate bar.
    camera.Reset();
    camera.Step(Input(),wall);
    const Scene returnBar{{{{-100,210,-140},{100,230,-120}}}};
    out=camera.Step(Input(0.1f),returnBar);
    assert(out.desiredDistance==300 && out.distance<=112.01f);
    CheckPose(returnBar,out);

    // A long staircase exhausts a fixed iteration budget. Never return the
    // last unverified pivot: the fallback at Harry's head remains clear.
    Scene staircase;
    for (float distance=300;distance>=75;distance-=25) {
        const float y=180+distance/4;
        const float next=distance-25;
        staircase.boxes.push_back({{-100,y-0.1f,-next-9},{100,y+0.1f,-next-8}});
    }
    auto thinInput=Input();thinInput.radius=1;
    camera.Reset();
    out=camera.Step(thinInput,staircase);
    assert(Close(out.distance,0));
    CheckPose(staircase,out,1);

    // Same outward speed at 30 and 120 FPS, and collision pulls in at once.
    CameraPlacement slow,fast;
    slow.Step(Input(),wall);fast.Step(Input(),wall);
    float slowDistance=0;
    for (int i=0;i<15;++i) slowDistance=slow.Step(Input(1.0f/30),open).distance;
    for (int i=0;i<60;++i) out=fast.Step(Input(1.0f/120),open);
    assert(Close(out.distance,slowDistance));
    out=fast.Step(Input(),closeWall);
    assert(Close(out.distance,12));

    // Context changes discard all stale collision smoothing state.
    assert(!fast.Step(CameraPlacementInput{},open).active);
    out=fast.Step(Input(),open);
    assert(Close(out.distance,300));
    auto bad=Input();bad.head.y=std::numeric_limits<float>::quiet_NaN();
    assert(!fast.Step(bad,open).active);
    out=fast.Step(Input(),[](CameraPoint,CameraPoint){
        return CameraRayHit{true,std::numeric_limits<float>::infinity()};
    });
    assert(out.active && Close(out.distance,0));
    assert(std::isfinite(out.position.x) && std::isfinite(out.position.y));

    std::cout<<"HP2 camera placement: low roof, corners, close wall, re-trace and smoothing OK\n";
}
