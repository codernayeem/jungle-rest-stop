// =============================================================================
//  Visitors.h  -  NPC visitors. They can ONLY arrive by car:
//
//   DRIVE_IN -> (wait for lot) -> PARKING -> EXIT_CAR -> walk inside ->
//   activities (vending machine, coffee, ATM, sit & watch TV, library,
//   aquarium) -> walk back -> ENTER_CAR -> UNPARK (reverse) -> DRIVE_OUT
//
//  Cars follow paths made of road points + cubic Bezier curves into the
//  parking spot. Only one car manoeuvres in the lot at a time (a "token").
//  People walk on a small waypoint graph using Dijkstra shortest paths.
// =============================================================================
#pragma once
#include <ostream>
#include <vector>
#include "Car.h"
#include "Human.h"
#include "../game/Collision.h"

class VendingMachine;
class ATM;
class CoffeeMachine;
class Aquarium;

struct WorldObjects {
    VendingMachine* vending[3] = {};
    ATM* atm = nullptr;
    CoffeeMachine* coffee = nullptr;
    Aquarium* aquarium = nullptr;
};

class VisitorSystem {
public:
    void init(const WorldObjects& w, bool lowQuality);
    void update(float dt, float time, const Vec3& playerPos, float night, CollisionWorld& col);
    void draw(DrawSink& dyn, const Vec3& cam, float time) const;
    void addLights(float night) const;
    void peoplePositions(std::vector<Vec3>& out) const;
    void arrivingCars(std::vector<Vec3>& out) const;   // cars still driving in on the road
    void forceSpawn() { m_spawnTimer = 0.0f; m_forceSpawn = true; }
    void prewarm(float seconds, CollisionWorld& col, std::ostream* log = nullptr);  // simulate ahead (screenshots)

    int visitorsActive() const;
    int carsParked() const;
    int totalArrived = 0, totalLeft = 0;
    const char* describe(int i) const;
    int maxVisitors = 6;
    float spawnMin = 8.0f, spawnMax = 18.0f;   // seconds between arrivals

    // ---- shared helpers (public for the report figures) ----
    struct Path {
        std::vector<Vec3> pts;
        std::vector<float> cum;
        float length = 0;
        void build();
        Vec3 at(float s, Vec3* tangent = nullptr) const;
    };

private:
    enum VState { DRIVE_IN, WAIT_LOT, PARKING, EXIT_CAR, INSIDE, ENTER_CAR, WAIT_UNPARK, UNPARKING, DRIVE_OUT, GONE };
    enum Act { ACT_VEND, ACT_COFFEE, ACT_ATM, ACT_SOFA, ACT_ARMCHAIR, ACT_BROWSE, ACT_AQUARIUM, ACT_LEAVE };
    enum Pose { POSE_STAND, POSE_WALK, POSE_SIT, POSE_PRESS, POSE_BEND, POSE_DRIVE };

    struct Visitor {
        bool active = false;
        unsigned seed = 0;
        VState state = GONE;
        float stateT = 0;
        // car
        CarLook carLook;
        CarState car;
        Vec3 carPos;
        float carYaw = 0;
        Path carPath;
        float carS = 0, carSpeed = 0;
        bool carReverse = false;
        int spot = -1;
        // person
        HumanLook look;
        Vec3 pos;
        float yaw = 0;
        std::vector<Vec3> walk;
        size_t walkIdx = 0;
        float walkPhase = 0;
        HeldItem held = HELD_NONE;
        bool inCar = true;
        Pose pose = POSE_DRIVE, prevPose = POSE_DRIVE;
        float poseT = 1.0f;
        // activities
        std::vector<Act> plan;
        Act act = ACT_LEAVE;
        int actStage = 0;
        float actT = 0;
        int resource = -1;
        int target = -1;
        float wait = 0;
    };

    struct Target { Vec3 stand; float yaw; int node; Vec3 seat; bool isSeat; };

    void spawn();
    void updateCar(Visitor& v, float dt, const Vec3& player);
    void updatePerson(Visitor& v, float dt, const Vec3& player);
    bool followWalk(Visitor& v, float dt, const Vec3& player);
    void startNextActivity(Visitor& v);
    void doActivity(Visitor& v, float dt);
    void setPose(Visitor& v, Pose p);
    bool carBlocked(const Visitor& v, const Vec3& player) const;
    std::vector<Vec3> graphPath(const Vec3& from, int toNode) const;
    int nearestNode(const Vec3& p) const;
    Vec3 carDoorPoint(const Visitor& v) const;
    std::vector<Vec3> outsidePathIn(const Visitor& v) const;
    void buildDriveIn(Visitor& v);
    void buildPark(Visitor& v);
    void buildUnpark(Visitor& v);
    void buildDriveOut(Visitor& v);
    int reserve(int lo, int hi, int owner);
    void release(int& r);
    Mat4 carMatrix(const Visitor& v) const;

    WorldObjects m_w;
    std::vector<Visitor> m_vis;
    std::vector<Vec3> m_nodes;
    std::vector<std::vector<int>> m_adj;
    std::vector<Target> m_targets;
    std::vector<int> m_resOwner;
    bool m_spotTaken[20] = {};
    int m_token = -1;
    float m_spawnTimer = 4.0f;
    bool m_forceSpawn = false;
    float m_time = 0;
    Vec3 m_player;
    Rng m_rng{4711};
};
