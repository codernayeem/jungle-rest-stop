#include "Visitors.h"
#include "../objects/ATM.h"
#include "../objects/Appliances.h"
#include "../objects/VendingMachine.h"
#include "../scene/Layout.h"
#include "../scene/Lights.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <queue>
#include <string>

using namespace Layout;

namespace {
// resources (one visitor at a time each)
enum { R_VEND0 = 0, R_COFFEE = 3, R_ATM = 4, R_SOFA0 = 5, R_SOFA_END = 11, R_ARM0 = 12, R_ARM1 = 13, R_BROWSE0 = 14, R_BROWSE1 = 15, R_AQUA = 16, R_COUNT = 17 };
const float WALK_SPEED = 1.35f;
const float POSE_BLEND_TIME = 0.35f;

Vec3 bezier(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, float t) {
    float u = 1 - t;
    return a * (u * u * u) + b * (3 * u * u * t) + c * (3 * u * t * t) + d * (t * t * t);
}
float yawTowards(const Vec3& d) { return toDegrees(std::atan2(d.x, d.z)); }
float turnTowards(float cur, float target, float maxStep) {
    float diff = wrapAngle(toRadians(target - cur));
    float step = clampf(toDegrees(diff), -maxStep, maxStep);
    return cur + step;
}
}  // namespace

// =============================================================================
//  Path (poly-line with arc-length parameter)
// =============================================================================
void VisitorSystem::Path::build() {
    cum.assign(pts.size(), 0.0f);
    for (size_t i = 1; i < pts.size(); ++i) cum[i] = cum[i - 1] + (pts[i] - pts[i - 1]).length();
    length = pts.empty() ? 0.0f : cum.back();
}

Vec3 VisitorSystem::Path::at(float s, Vec3* tangent) const {
    if (pts.size() < 2) { if (tangent) *tangent = Vec3(0, 0, 1); return pts.empty() ? Vec3() : pts[0]; }
    s = clampf(s, 0.0f, length);
    size_t i = std::upper_bound(cum.begin(), cum.end(), s) - cum.begin();
    if (i == 0) i = 1;
    if (i >= pts.size()) i = pts.size() - 1;
    float seg = cum[i] - cum[i - 1];
    float t = seg > 1e-5f ? (s - cum[i - 1]) / seg : 0.0f;
    if (tangent) *tangent = normalize(pts[i] - pts[i - 1]);
    return mix(pts[i - 1], pts[i], t);
}

// =============================================================================
//  Init: navigation graph + activity targets
// =============================================================================
void VisitorSystem::init(const WorldObjects& w, bool low) {
    m_w = w;
    maxVisitors = low ? 4 : 6;
    m_vis.assign(maxVisitors, Visitor());
    m_nodes = {Vec3(0, 0, 3.6f),     Vec3(0, 0, 1.6f),     Vec3(-3.6f, 0, 1.2f), Vec3(3.6f, 0, 1.2f),  Vec3(-4.6f, 0, -3.0f),
               Vec3(3.7f, 0, -3.0f), Vec3(-6.3f, 0, -2.6f), Vec3(-7.7f, 0, -2.6f), Vec3(-9.5f, 0, -1.5f), Vec3(6.3f, 0, -0.4f),
               Vec3(7.7f, 0, -0.4f), Vec3(10.4f, 0, 0.6f), Vec3(-1.6f, 0, -1.5f), Vec3(1.6f, 0, -1.5f), Vec3(-3.2f, 0, 3.6f),
               Vec3(3.2f, 0, 3.6f),  Vec3(-8.0f, 0, 0.4f),  Vec3(-9.5f, 0, 0.2f)};
    const int edges[][2] = {{0, 1}, {1, 2}, {1, 3}, {2, 4}, {2, 6}, {4, 6}, {6, 7}, {7, 16}, {16, 17}, {17, 8}, {3, 5},
                            {3, 9}, {5, 9}, {9, 10}, {10, 11}, {2, 12}, {3, 13}, {12, 13}, {2, 14}, {3, 15}, {0, 14}, {0, 15}};
    m_adj.assign(m_nodes.size(), {});
    for (auto& e : edges) { m_adj[e[0]].push_back(e[1]); m_adj[e[1]].push_back(e[0]); }

    m_targets.assign(R_COUNT, Target());
    for (int i = 0; i < 3; ++i) m_targets[R_VEND0 + i] = {w.vending[i]->standPos(), 180.0f, 5, Vec3(), false};
    m_targets[R_COFFEE] = {w.coffee->standPos(), 180.0f, 4, Vec3(), false};
    m_targets[R_ATM] = {w.atm->standPos(), 180.0f, 8, Vec3(), false};
    const float sofaX[3] = {0.72f, 0.0f, -0.72f};
    for (int i = 0; i < 3; ++i) m_targets[R_SOFA0 + i] = {Vec3(sofaX[i], 0, -1.5f), 180.0f, sofaX[i] > 0 ? 13 : 12, Vec3(sofaX[i], 0, -0.66f), true};
    const float frontX[4] = {-4.96f, -4.24f, 4.24f, 4.96f};
    for (int i = 0; i < 4; ++i) m_targets[R_SOFA0 + 3 + i] = {Vec3(frontX[i], 0, 3.62f), 180.0f, frontX[i] < 0 ? 14 : 15, Vec3(frontX[i], 0, 4.26f), true};
    m_targets[R_ARM0] = {Vec3(9.0f, 0, 1.35f), 0.0f, 11, Vec3(9.0f, 0, 2.2f), true};
    m_targets[R_ARM1] = {Vec3(11.2f, 0, 1.35f), 0.0f, 11, Vec3(11.2f, 0, 2.2f), true};
    m_targets[R_BROWSE0] = {Vec3(9.0f, 0, -3.95f), 180.0f, 11, Vec3(), false};
    m_targets[R_BROWSE1] = {Vec3(14.0f, 0, -2.3f), 90.0f, 11, Vec3(), false};
    m_targets[R_AQUA] = {w.aquarium->viewPos(), 0.0f, 11, Vec3(), false};
    m_resOwner.assign(R_COUNT, -1);
}

int VisitorSystem::nearestNode(const Vec3& p) const {
    int best = 0;
    float bd = 1e9f;
    for (size_t i = 0; i < m_nodes.size(); ++i) {
        float d = distXZ(p, m_nodes[i]);
        if (d < bd) { bd = d; best = (int)i; }
    }
    return best;
}

// Dijkstra shortest path on the waypoint graph
std::vector<Vec3> VisitorSystem::graphPath(const Vec3& from, int to) const {
    int start = nearestNode(from);
    size_t n = m_nodes.size();
    std::vector<float> dist(n, 1e9f);
    std::vector<int> prev(n, -1);
    using QE = std::pair<float, int>;
    std::priority_queue<QE, std::vector<QE>, std::greater<QE>> q;
    dist[start] = 0;
    q.push({0, start});
    while (!q.empty()) {
        auto [d, u] = q.top();
        q.pop();
        if (d > dist[u]) continue;
        for (int v : m_adj[u]) {
            float nd = d + distXZ(m_nodes[u], m_nodes[v]);
            if (nd < dist[v]) { dist[v] = nd; prev[v] = u; q.push({nd, v}); }
        }
    }
    std::vector<Vec3> out;
    for (int c = to; c != -1; c = prev[c]) out.push_back(m_nodes[c]);
    std::reverse(out.begin(), out.end());
    return out;
}

int VisitorSystem::reserve(int lo, int hi, int owner) {
    std::vector<int> free;
    for (int r = lo; r <= hi; ++r)
        if (m_resOwner[r] == -1) free.push_back(r);
    if (free.empty()) return -1;
    int r = free[m_rng.irange(0, (int)free.size() - 1)];
    m_resOwner[r] = owner;
    return r;
}

void VisitorSystem::release(int& r) {
    if (r >= 0) m_resOwner[r] = -1;
    r = -1;
}

// =============================================================================
//  Car paths
// =============================================================================
Mat4 VisitorSystem::carMatrix(const Visitor& v) const { return rotateYXZ(translate(Mat4(), v.carPos), {0, v.carYaw, 0}); }

static float spotZ(int spot) { return spot < 10 ? SPOT_NORTH_Z : SPOT_SOUTH_Z; }

void VisitorSystem::buildDriveIn(Visitor& v) {
    Path p;
    for (float x = WORLD_HALF - 2.0f; x > LOT_X1 + 1.0f; x -= 3.0f) p.pts.push_back(Vec3(x, 0, roadCenterZ(x)));
    float sx = spotX(v.spot % 10);
    p.pts.push_back(Vec3(LOT_X1, 0, AISLE_Z));
    p.pts.push_back(Vec3(sx + 5.0f, 0, AISLE_Z));
    p.build();
    v.carPath = p;
    v.carS = 0;
    v.carReverse = false;
}

void VisitorSystem::buildPark(Visitor& v) {
    float sx = spotX(v.spot % 10), sz = spotZ(v.spot);
    float dir = v.spot < 10 ? 1.0f : -1.0f;   // aisle side relative to the spot
    Vec3 P0(sx + 5.0f, 0, AISLE_Z), P1(sx + 2.2f, 0, AISLE_Z), P2(sx, 0, sz + dir * 2.8f), P3(sx, 0, sz);
    Path p;
    for (int i = 0; i <= 18; ++i) p.pts.push_back(bezier(P0, P1, P2, P3, i / 18.0f));
    p.build();
    v.carPath = p;
    v.carS = 0;
    v.carReverse = false;
}

void VisitorSystem::buildUnpark(Visitor& v) {
    float sx = spotX(v.spot % 10), sz = spotZ(v.spot);
    float dir = v.spot < 10 ? 1.0f : -1.0f;
    Vec3 Q0(sx, 0, sz), Q1(sx, 0, sz + dir * 2.8f), Q2(sx + 1.8f, 0, AISLE_Z), Q3(sx + 4.5f, 0, AISLE_Z);
    Path p;
    for (int i = 0; i <= 18; ++i) p.pts.push_back(bezier(Q0, Q1, Q2, Q3, i / 18.0f));
    p.build();
    v.carPath = p;
    v.carS = 0;
    v.carReverse = true;
}

void VisitorSystem::buildDriveOut(Visitor& v) {
    Path p;
    float sx = spotX(v.spot % 10);
    p.pts.push_back(Vec3(sx + 4.5f, 0, AISLE_Z));
    p.pts.push_back(Vec3(LOT_X0, 0, AISLE_Z));
    for (float x = LOT_X0 - 2.0f; x > -WORLD_HALF + 2.0f; x -= 3.0f) p.pts.push_back(Vec3(x, 0, roadCenterZ(x)));
    p.build();
    v.carPath = p;
    v.carS = 0;
    v.carReverse = false;
}

Vec3 VisitorSystem::carDoorPoint(const Visitor& v) const {
    CarDims d = Car::dims(v.carLook.type);
    Vec3 p = carMatrix(v).transformPoint(Vec3(-d.width * 0.5f - 0.55f, 0, d.driverSeat.z + 0.1f));
    p.y = 0;
    return p;
}

std::vector<Vec3> VisitorSystem::outsidePathIn(const Visitor& v) const {
    Vec3 d = carDoorPoint(v);
    std::vector<Vec3> p;
    if (v.spot < 10) {
        p = {d, Vec3(d.x, 0, 14.2f), Vec3(0.0f, 0, 14.2f), Vec3(0.0f, 0, 8.6f), Vec3(0.0f, 0, 6.6f), m_nodes[0]};
    } else {
        p = {d, Vec3(d.x, 0, 17.8f), Vec3(0.0f, 0, 17.8f), Vec3(0.0f, 0, 14.2f), Vec3(0.0f, 0, 8.6f), Vec3(0.0f, 0, 6.6f), m_nodes[0]};
    }
    return p;
}

// =============================================================================
//  Spawning
// =============================================================================
void VisitorSystem::spawn() {
    int slot = -1;
    for (int i = 0; i < (int)m_vis.size(); ++i)
        if (!m_vis[i].active) { slot = i; break; }
    if (slot < 0) return;
    // the road entry must be clear
    for (const auto& o : m_vis)
        if (o.active && o.state == DRIVE_IN && o.carS < 30.0f) return;
    std::vector<int> freeSpots;
    for (int s = 0; s < 20; ++s)
        if (!m_spotTaken[s]) freeSpots.push_back(s);
    if (freeSpots.empty()) return;

    Visitor v;
    v.active = true;
    v.seed = m_rng.next();
    v.spot = freeSpots[m_rng.irange(0, (int)freeSpots.size() - 1)];
    m_spotTaken[v.spot] = true;
    v.carLook = Car::randomLook(v.seed);
    v.look = Human::randomLook(v.seed ^ 0x9E3779B9u);
    v.state = DRIVE_IN;
    buildDriveIn(v);
    Vec3 t;
    v.carPos = v.carPath.at(0, &t);
    v.carYaw = yawTowards(t);
    v.carSpeed = 7.0f;
    v.inCar = true;
    v.pose = v.prevPose = POSE_DRIVE;

    // plan 1..3 activities
    Rng r(v.seed);
    std::vector<Act> pool = {ACT_VEND, ACT_VEND, ACT_COFFEE, ACT_SOFA, ACT_SOFA, ACT_ATM, ACT_ARMCHAIR, ACT_BROWSE, ACT_AQUARIUM};
    int count = r.irange(1, 3);
    for (int i = 0; i < count; ++i) {
        Act a = pool[r.irange(0, (int)pool.size() - 1)];
        if (std::find(v.plan.begin(), v.plan.end(), a) != v.plan.end()) continue;
        if (a == ACT_ATM) v.plan.insert(v.plan.begin(), a);   // get cash first
        else v.plan.push_back(a);
    }
    m_vis[slot] = v;
    totalArrived++;
}

// =============================================================================
//  Car update
// =============================================================================
bool VisitorSystem::carBlocked(const Visitor& v, const Vec3& player) const {
    Vec3 tan;
    v.carPath.at(v.carS, &tan);
    Vec3 dir = v.carReverse ? -tan : tan;   // direction of motion
    bool owner = (m_token >= 0 && &m_vis[m_token] == &v);
    // people in front of the car
    auto personAhead = [&](const Vec3& p) {
        Vec3 rel = p - v.carPos;
        float ahead = dot(rel, dir), lat = std::fabs(rel.x * dir.z - rel.z * dir.x);
        return ahead > 0.0f && ahead < 5.5f && lat < 1.5f;
    };
    if (personAhead(player)) return true;
    for (const auto& o : m_vis)
        if (o.active && !o.inCar && personAhead(o.pos)) return true;
    if (owner) return false;
    for (const auto& o : m_vis) {
        if (!o.active || &o == &v || o.state == GONE) continue;
        Vec3 rel = o.carPos - v.carPos;
        float ahead = dot(rel, dir), lat = std::fabs(rel.x * dir.z - rel.z * dir.x);
        if (ahead > 0.0f && ahead < 7.5f && lat < 2.2f && (o.state != EXIT_CAR && o.state != INSIDE && o.state != ENTER_CAR && o.state != WAIT_UNPARK)) return true;
    }
    // a car manoeuvring in the lot ahead of us -> wait
    if (m_token >= 0 && (v.state == DRIVE_IN || v.state == DRIVE_OUT) && std::fabs(v.carPos.x) < LOT_X1 + 4.0f) {
        const Visitor& o = m_vis[m_token];
        float sx = spotX(o.spot % 10);
        float d = v.carPos.x - sx;   // we drive towards -X
        if (d > -3.0f && d < 12.0f && std::fabs(v.carPos.z - AISLE_Z) < 3.0f) return true;
    }
    return false;
}

void VisitorSystem::updateCar(Visitor& v, float dt, const Vec3& player) {
    bool moving = v.state == DRIVE_IN || v.state == PARKING || v.state == UNPARKING || v.state == DRIVE_OUT;
    if (!moving) { v.carSpeed = 0; v.car.brake = false; return; }
    float remaining = v.carPath.length - v.carS;
    float vmax;
    bool inLot = std::fabs(v.carPos.x) < LOT_X1 + 6.0f;
    if (v.state == PARKING) vmax = 1.8f;
    else if (v.state == UNPARKING) vmax = 1.3f;
    else vmax = inLot ? 4.0f : 9.0f;
    if (v.state == DRIVE_IN || v.state == PARKING || v.state == UNPARKING) vmax = std::min(vmax, std::sqrt(2.0f * 2.2f * std::max(remaining, 0.0f)) + 0.25f);
    if (carBlocked(v, player)) vmax = 0.0f;
    float prev = v.carSpeed;
    v.carSpeed = approach(v.carSpeed, vmax, dt * (vmax < v.carSpeed ? 5.0f : 2.2f));
    v.car.brake = v.carSpeed < prev - 0.001f || v.carSpeed < 0.05f;
    float ds = v.carSpeed * dt;
    v.carS += ds;
    Vec3 tan;
    Vec3 p = v.carPath.at(v.carS, &tan);
    float newYaw = yawTowards(v.carReverse ? -tan : tan);
    float yawRate = toDegrees(wrapAngle(toRadians(newYaw - v.carYaw)));
    CarDims d = Car::dims(v.carLook.type);
    float steerTarget = ds > 1e-4f ? clampf(toDegrees(std::atan(d.wheelBase * toRadians(yawRate) / ds)) * (v.carReverse ? -1.0f : 1.0f), -35.0f, 35.0f) : v.car.steer;
    v.car.steer = approach(v.car.steer, steerTarget, dt * 90.0f);
    v.carYaw = newYaw;
    v.carPos = p;
    v.car.wheelSpin += (v.carReverse ? -ds : ds) / d.wheelR;
    v.car.reversing = v.carReverse && v.carSpeed > 0.05f;
}

// =============================================================================
//  People
// =============================================================================
void VisitorSystem::setPose(Visitor& v, Pose p) {
    if (v.pose == p) return;
    v.prevPose = v.pose;
    v.pose = p;
    v.poseT = 0.0f;
}

bool VisitorSystem::followWalk(Visitor& v, float dt, const Vec3& player) {
    if (v.walkIdx >= v.walk.size()) return true;
    Vec3 target = v.walk[v.walkIdx];
    Vec3 d = target - v.pos;
    d.y = 0;
    float dist = d.length();
    if (dist < 0.08f) {
        v.walkIdx++;
        return v.walkIdx >= v.walk.size();
    }
    Vec3 dir = d / dist;
    // pause if the player stands right in front
    Vec3 rp = player - v.pos;
    bool blocked = dot(rp, dir) > 0 && dot(rp, dir) < 0.75f && std::fabs(rp.x * dir.z - rp.z * dir.x) < 0.45f;
    if (blocked && v.wait < 2.5f) {
        v.wait += dt;
        setPose(v, POSE_STAND);
        return false;
    }
    v.wait = 0;
    setPose(v, POSE_WALK);
    float step = std::min(dist, WALK_SPEED * dt);
    v.pos += dir * step;
    v.walkPhase += dt * 6.8f;
    v.yaw = turnTowards(v.yaw, yawTowards(dir), 400.0f * dt);
    return false;
}

void VisitorSystem::startNextActivity(Visitor& v) {
    release(v.resource);
    v.actStage = 0;
    v.actT = 0;
    v.wait = 0;
    while (!v.plan.empty()) {
        Act a = v.plan.front();
        v.plan.erase(v.plan.begin());
        int lo = 0, hi = 0;
        switch (a) {
            case ACT_VEND: lo = R_VEND0; hi = R_VEND0 + 2; break;
            case ACT_COFFEE: lo = hi = R_COFFEE; break;
            case ACT_ATM: lo = hi = R_ATM; break;
            case ACT_SOFA: lo = R_SOFA0; hi = R_SOFA_END; break;
            case ACT_ARMCHAIR: lo = R_ARM0; hi = R_ARM1; break;
            case ACT_BROWSE: lo = R_BROWSE0; hi = R_BROWSE1; break;
            case ACT_AQUARIUM: lo = hi = R_AQUA; break;
            default: break;
        }
        int r = reserve(lo, hi, (int)(&v - &m_vis[0]));
        if (r < 0) continue;
        v.act = a;
        v.resource = r;
        v.target = r;
        const Target& t = m_targets[r];
        v.walk = graphPath(v.pos, t.node);
        v.walk.push_back(t.stand);
        v.walkIdx = 0;
        return;
    }
    // nothing left: go back to the car
    v.act = ACT_LEAVE;
    v.walk = graphPath(v.pos, 0);
    auto out = outsidePathIn(v);
    std::reverse(out.begin(), out.end());
    v.walk.insert(v.walk.end(), out.begin() + 1, out.end());
    v.walkIdx = 0;
}

void VisitorSystem::doActivity(Visitor& v, float dt) {
    v.actT += dt;
    if (v.act == ACT_LEAVE) {
        if (followWalk(v, dt, m_player)) {
            v.state = ENTER_CAR;
            v.stateT = 0;
            setPose(v, POSE_STAND);
        }
        return;
    }
    const Target& t = m_targets[v.target];
    if (v.actStage == 0) {
        if (followWalk(v, dt, m_player)) {
            v.actStage = 1;
            v.actT = 0;
            setPose(v, POSE_STAND);
        }
        return;
    }
    v.yaw = turnTowards(v.yaw, t.yaw, 300.0f * dt);
    auto finish = [&]() { setPose(v, POSE_STAND); startNextActivity(v); };

    switch (v.act) {
        case ACT_VEND: {
            VendingMachine* m = m_w.vending[v.target - R_VEND0];
            if (v.actStage == 1) {
                if (v.actT > 0.4f && m->npcStartPurchase(v.seed + (unsigned)(m_time * 10))) { setPose(v, POSE_PRESS); v.actStage = 2; v.actT = 0; }
                else if (v.actT > 9.0f) finish();
            } else if (v.actStage == 2 && v.actT > 1.5f) { setPose(v, POSE_STAND); v.actStage = 3; v.actT = 0; }
            else if (v.actStage == 3) {
                if (m->itemWaiting()) { setPose(v, POSE_BEND); v.actStage = 4; v.actT = 0; }
                else if (v.actT > 10.0f) finish();
            } else if (v.actStage == 4 && v.actT > 0.8f) {
                ProductShape s = m->lastItemShape();
                m->npcTakeItem();
                v.held = (s == P_CAN || s == P_BOTTLE || s == P_JUICE) ? HELD_CAN : HELD_SNACK;
                setPose(v, POSE_STAND);
                v.actStage = 5;
                v.actT = 0;
            } else if (v.actStage == 5 && v.actT > 0.6f) finish();
            break;
        }
        case ACT_COFFEE: {
            CoffeeMachine* c = m_w.coffee;
            if (v.actStage == 1) {
                if (v.actT > 0.4f && c->npcBrew((int)(v.seed % 3))) { setPose(v, POSE_PRESS); v.actStage = 2; v.actT = 0; }
                else if (v.actT > 9.0f) finish();
            } else if (v.actStage == 2 && v.actT > 1.2f) { setPose(v, POSE_STAND); v.actStage = 3; v.actT = 0; }
            else if (v.actStage == 3) {
                if (c->cupReady()) { setPose(v, POSE_PRESS); v.actStage = 4; v.actT = 0; }
                else if (v.actT > 14.0f) finish();
            } else if (v.actStage == 4 && v.actT > 0.7f) {
                c->npcTakeCup();
                v.held = HELD_CUP;
                finish();
            }
            break;
        }
        case ACT_ATM: {
            ATM* a = m_w.atm;
            if (v.actStage == 1) {
                if (v.actT > 0.5f && a->npcStart()) { setPose(v, POSE_PRESS); v.actStage = 2; v.actT = 0; }
                else if (v.actT > 15.0f) finish();
            } else if (v.actStage == 2) {
                if (!a->inUseByNpc() && v.actT > 1.0f) { v.held = HELD_PHONE; finish(); }
            }
            break;
        }
        case ACT_SOFA:
        case ACT_ARMCHAIR: {
            if (v.actStage == 1) {          // turn and sit down
                float k = std::min(1.0f, v.actT / 0.9f);
                v.yaw = t.yaw;
                v.pos = mix(t.stand, t.seat, smoothstepf(0, 1, k));
                if (v.actT > 0.2f) setPose(v, POSE_SIT);
                if (k >= 1.0f) { v.actStage = 2; v.actT = 0; v.wait = 12.0f + (v.seed % 17); }
            } else if (v.actStage == 2) {   // sit, watch TV / relax
                if (v.actT > v.wait) { v.actStage = 3; v.actT = 0; setPose(v, POSE_STAND); }
            } else if (v.actStage == 3) {   // stand up
                float k = std::min(1.0f, v.actT / 0.9f);
                v.pos = mix(t.seat, t.stand, smoothstepf(0, 1, k));
                if (k >= 1.0f) finish();
            }
            break;
        }
        case ACT_BROWSE: {
            if (v.actT > 3.0f && v.actT < 4.5f) setPose(v, POSE_PRESS);
            else setPose(v, POSE_STAND);
            if (v.actT > 7.0f + (v.seed % 5)) finish();
            break;
        }
        case ACT_AQUARIUM:
            if (v.actT > 6.0f + (v.seed % 5)) finish();
            break;
        default: finish(); break;
    }
}

void VisitorSystem::updatePerson(Visitor& v, float dt, const Vec3& player) {
    (void)player;
    v.poseT += dt;
    CarDims d = Car::dims(v.carLook.type);
    Mat4 cm = carMatrix(v);
    Vec3 seatW = cm.transformPoint(d.driverSeat);
    switch (v.state) {
        case EXIT_CAR: {
            v.stateT += dt;
            float t = v.stateT;
            v.car.doorAngle = t < 0.7f ? 70.0f * (t / 0.7f) : (t < 1.6f ? 70.0f : 70.0f * std::max(0.0f, 1.0f - (t - 1.6f) / 0.6f));
            Vec3 door = carDoorPoint(v);
            if (t > 0.6f && v.inCar) { v.inCar = false; setPose(v, POSE_WALK); v.pos = seatW; v.yaw = v.carYaw - 90.0f; }
            if (!v.inCar) {
                float k = clampf((t - 0.6f) / 0.9f, 0.0f, 1.0f);
                Vec3 s = seatW; s.y = 0;
                v.pos = mix(s, door, k);
                v.walkPhase += dt * 6.0f;
            }
            if (t > 2.2f) {
                v.car.doorAngle = 0;
                v.state = INSIDE;
                v.stateT = 0;
                // walk to the building, then start the first activity
                auto path = outsidePathIn(v);
                v.walk.assign(path.begin() + 1, path.end());
                v.walkIdx = 0;
                v.act = ACT_LEAVE;
                v.actStage = -1;
            }
            break;
        }
        case INSIDE:
            if (v.actStage == -1) {   // still walking in from the car
                if (followWalk(v, dt, m_player)) startNextActivity(v);
            } else {
                doActivity(v, dt);
            }
            break;
        case ENTER_CAR: {
            v.stateT += dt;
            float t = v.stateT;
            v.car.doorAngle = t < 0.7f ? 70.0f * (t / 0.7f) : (t < 1.7f ? 70.0f : 70.0f * std::max(0.0f, 1.0f - (t - 1.7f) / 0.6f));
            if (t > 0.6f && !v.inCar) {
                float k = clampf((t - 0.6f) / 0.9f, 0.0f, 1.0f);
                Vec3 s = seatW; s.y = 0;
                v.pos = mix(carDoorPoint(v), s, k);
                v.yaw = v.carYaw + 90.0f;
                if (k >= 1.0f) { v.inCar = true; setPose(v, POSE_DRIVE); v.held = HELD_NONE; }
            }
            if (t > 2.4f) {
                v.car.doorAngle = 0;
                v.state = WAIT_UNPARK;
                v.stateT = 0;
            }
            break;
        }
        default: break;
    }
}

// =============================================================================
//  Main update
// =============================================================================
void VisitorSystem::update(float dt, float time, const Vec3& player, float night, CollisionWorld& col) {
    m_time = time;
    m_player = player;
    m_spawnTimer -= dt;
    if (m_spawnTimer <= 0.0f) {
        spawn();
        m_spawnTimer = m_forceSpawn ? 2.0f : m_rng.range(spawnMin, spawnMax);
        m_forceSpawn = false;
    }

    for (int i = 0; i < (int)m_vis.size(); ++i) {
        Visitor& v = m_vis[i];
        if (!v.active) continue;
        v.car.time = time;
        bool engineOn = v.state == DRIVE_IN || v.state == WAIT_LOT || v.state == PARKING || v.state == WAIT_UNPARK ||
                        v.state == UNPARKING || v.state == DRIVE_OUT;
        v.car.headlights = engineOn && night > 0.3f;
        v.car.indicator = v.state == WAIT_LOT || v.state == PARKING || v.state == UNPARKING || v.state == WAIT_UNPARK;
        updateCar(v, dt, player);

        switch (v.state) {
            case DRIVE_IN:
                if (v.carS >= v.carPath.length - 0.05f) { v.state = WAIT_LOT; v.stateT = 0; }
                break;
            case WAIT_LOT:
                v.car.brake = true;
                if (m_token == -1) { m_token = i; buildPark(v); v.state = PARKING; }
                break;
            case PARKING:
                if (v.carS >= v.carPath.length - 0.02f) {
                    m_token = -1;
                    v.state = EXIT_CAR;
                    v.stateT = 0;
                    v.car.steer = 0;
                }
                break;
            case WAIT_UNPARK: {
                v.stateT += dt;
                v.car.brake = true;
                if (v.stateT < 1.0f || m_token != -1) break;
                float sx = spotX(v.spot % 10);
                bool clear = true;
                for (const auto& o : m_vis) {
                    if (!o.active || &o == &v) continue;
                    if ((o.state == DRIVE_IN || o.state == WAIT_LOT || o.state == DRIVE_OUT) && std::fabs(o.carPos.z - AISLE_Z) < 3.5f &&
                        o.carPos.x > sx - 4.0f && o.carPos.x < sx + 24.0f)
                        clear = false;
                }
                if (clear) { m_token = i; buildUnpark(v); v.state = UNPARKING; }
                break;
            }
            case UNPARKING:
                if (v.carS >= v.carPath.length - 0.02f) {
                    m_token = -1;
                    m_spotTaken[v.spot] = false;
                    buildDriveOut(v);
                    v.state = DRIVE_OUT;
                }
                break;
            case DRIVE_OUT:
                if (v.carS >= v.carPath.length - 0.5f) {
                    v.active = false;
                    v.state = GONE;
                    totalLeft++;
                }
                break;
            default:
                updatePerson(v, dt, player);
                break;
        }
        // car + person colliders for the player
        if (v.active) {
            CarDims d = Car::dims(v.carLook.type);
            Vec3 c = carMatrix(v).transformPoint(Vec3(0, 0, 0));
            col.addOriented(c, d.width * 0.5f, d.length * 0.5f, v.carYaw, 0.0f, d.height, true);
            if (!v.inCar) col.addCircle(v.pos.x, v.pos.z, 0.22f, true);
        }
    }
}

void VisitorSystem::prewarm(float seconds, CollisionWorld& col, std::ostream* log) {
    float t = 0;
    std::vector<std::string> last(m_vis.size());
    while (t < seconds) {
        if (log) {   // write every state change of every visitor (state-machine trace)
            for (size_t i = 0; i < m_vis.size(); ++i) {
                std::string d = m_vis[i].active ? describe((int)i) : "";
                if (d != last[i] && !d.empty()) {
                    char buf[160];
                    std::snprintf(buf, sizeof(buf), "t=%6.1fs  visitor %zu (spot %2d): %s", t, i, m_vis[i].spot, d.c_str());
                    *log << buf << std::endl;
                }
                last[i] = d;
            }
        }
        col.clearDynamic();
        update(0.05f, t, Vec3(1000, 0, 1000), 0.0f, col);
        // let the machines advance too (they are updated by the app normally)
        for (auto* vm : m_w.vending) vm->update(0.05f);
        m_w.atm->update(0.05f);
        m_w.coffee->update(0.05f);
        t += 0.05f;
    }
    col.clearDynamic();
}

// =============================================================================
//  Drawing
// =============================================================================
void VisitorSystem::draw(DrawSink& dyn, const Vec3& cam, float time) const {
    for (const auto& v : m_vis) {
        if (!v.active) continue;
        Mat4 cm = carMatrix(v);
        if ((v.carPos - cam).length() < 160.0f) Car::draw(Node(dyn, cm), v.carLook, v.car);
        auto poseOf = [&](Pose p) {
            switch (p) {
                case POSE_WALK: return Human::walk(v.walkPhase, 1.0f);
                case POSE_SIT: return Human::sit(time + v.seed % 10);
                case POSE_PRESS: return Human::press(time);
                case POSE_BEND: return Human::bend(1.0f);
                case POSE_DRIVE: return Human::drive(time);
                default: return Human::idle(time + v.seed % 7);
            }
        };
        HumanPose pose = Human::blend(poseOf(v.prevPose), poseOf(v.pose), clampf(v.poseT / POSE_BLEND_TIME, 0.0f, 1.0f));
        if (v.inCar) {
            if ((v.carPos - cam).length() < 60.0f) {
                CarDims d = Car::dims(v.carLook.type);
                Human::draw(Node(dyn, cm).at(d.driverSeat), v.look, Human::drive(time), HELD_NONE);
            }
        } else if ((v.pos - cam).length() < 70.0f) {
            Human::draw(Node(dyn, rotateYXZ(translate(Mat4(), v.pos), {0, v.yaw, 0})), v.look, pose, v.held);
        }
    }
}

void VisitorSystem::addLights(float night) const {
    if (night < 0.3f) return;
    for (const auto& v : m_vis) {
        if (!v.active || !v.car.headlights) continue;
        CarDims d = Car::dims(v.carLook.type);
        Mat4 cm = carMatrix(v);
        LightDef l;
        l.kind = LIGHT_SPOT;
        l.pos = cm.transformPoint(Vec3(0, d.hoodTop - 0.1f, d.length * 0.5f + 0.3f));
        l.dir = normalize(cm.transformDir(Vec3(0, -0.12f, 1)));
        l.color = Vec3(1.0f, 0.95f, 0.85f);
        l.intensity = 9.0f;
        l.range = 26.0f;
        l.innerDeg = 16.0f;
        l.outerDeg = 30.0f;
        l.group = LG_MACHINES;
        LightManager::get().addFrame(l);
    }
}

void VisitorSystem::peoplePositions(std::vector<Vec3>& out) const {
    for (const auto& v : m_vis)
        if (v.active && !v.inCar) out.push_back(v.pos);
}

int VisitorSystem::visitorsActive() const {
    int n = 0;
    for (const auto& v : m_vis) n += v.active ? 1 : 0;
    return n;
}

int VisitorSystem::carsParked() const {
    int n = 0;
    for (const auto& v : m_vis)
        if (v.active && (v.state == EXIT_CAR || v.state == INSIDE || v.state == ENTER_CAR || v.state == WAIT_UNPARK)) n++;
    return n;
}

const char* VisitorSystem::describe(int i) const {
    if (i < 0 || i >= (int)m_vis.size() || !m_vis[i].active) return "";
    const Visitor& v = m_vis[i];
    switch (v.state) {
        case DRIVE_IN: return "driving in";
        case WAIT_LOT: return "waiting to park";
        case PARKING: return "parking";
        case EXIT_CAR: return "getting out";
        case INSIDE:
            if (v.actStage == -1) return "walking in";
            switch (v.act) {
                case ACT_VEND: return "at a vending machine";
                case ACT_COFFEE: return "making coffee";
                case ACT_ATM: return "using the ATM";
                case ACT_SOFA: return "watching TV";
                case ACT_ARMCHAIR: return "reading";
                case ACT_BROWSE: return "browsing books";
                case ACT_AQUARIUM: return "watching fish";
                default: return "leaving";
            }
        case ENTER_CAR: return "getting in";
        case WAIT_UNPARK: return "about to leave";
        case UNPARKING: return "reversing out";
        case DRIVE_OUT: return "driving away";
        default: return "";
    }
}
