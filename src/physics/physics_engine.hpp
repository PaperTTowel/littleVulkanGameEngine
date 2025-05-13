#pragma once

#include <btBulletDynamicsCommon.h>
#include <memory>
#include <vector>

namespace lve{
  class PhysicsEngine{
  public:
    PhysicsEngine();
    ~PhysicsEngine();

    void stepSimulation(float deltaTime);

    btDiscreteDynamicsWorld *getWorld() { return dynamicsWorld.get(); }

  private:
    std::unique_ptr<btBroadphaseInterface> broadphase;
    std::unique_ptr<btDefaultCollisionConfiguration> collisionConfiguration;
    std::unique_ptr<btCollisionDispatcher> dispatcher;
    std::unique_ptr<btSequentialImpulseConstraintSolver> solver;
    std::unique_ptr<btDiscreteDynamicsWorld> dynamicsWorld;
  };
} // namespcae lve