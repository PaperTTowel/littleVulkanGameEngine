#include "physics_engine.hpp"

namespace lve{
  PhysicsEngine::PhysicsEngine(){
    broadphase = std::make_unique<btDbvtBroadphase>();
    collisionConfiguration = std::make_unique<btDefaultCollisionConfiguration>();
    dispatcher = std::make_unique<btCollisionDispatcher>(collisionConfiguration.get());
    solver = std::make_unique<btSequentialImpulseConstraintSolver>();
    dynamicsWorld = std::make_unique<btDiscreteDynamicsWorld>(
        dispatcher.get(), broadphase.get(), solver.get(), collisionConfiguration.get());
    dynamicsWorld->setGravity(btVector3(0, -9.81f, 0));
  }
  
  PhysicsEngine::~PhysicsEngine(){
    // 동적 할당한 rigidbody나 shape 따로 해제하는 곳 (메모리 누수 방지)
  }
  
  void PhysicsEngine::stepSimulation(float deltaTime){
    dynamicsWorld->stepSimulation(deltaTime);
  }
} // namespace lve

