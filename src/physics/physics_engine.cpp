#include "physics_engine.hpp"
#include "lve_game_object.hpp"

#include <iostream>

namespace lve{
  PhysicsEngine::PhysicsEngine(){
    broadphase = std::make_unique<btDbvtBroadphase>();
    collisionConfiguration = std::make_unique<btDefaultCollisionConfiguration>();
    dispatcher = std::make_unique<btCollisionDispatcher>(collisionConfiguration.get());
    solver = std::make_unique<btSequentialImpulseConstraintSolver>();
    dynamicsWorld = std::make_unique<btDiscreteDynamicsWorld>(
        dispatcher.get(), broadphase.get(), solver.get(), collisionConfiguration.get());
    dynamicsWorld->setGravity(btVector3(0, 9.81f, 0));
  }
  
  PhysicsEngine::~PhysicsEngine(){
    // 동적 할당한 rigidbody나 shape 따로 해제하는 곳 (메모리 누수 방지)
  }
  
  void PhysicsEngine::stepSimulation(float deltaTime) {
    dynamicsWorld->stepSimulation(deltaTime);

    int numManifolds = dynamicsWorld->getDispatcher()->getNumManifolds();
    for (int i = 0; i < numManifolds; i++) {
      btPersistentManifold* contactManifold = dynamicsWorld->getDispatcher()->getManifoldByIndexInternal(i);

      const btCollisionObject* obA = contactManifold->getBody0();
      const btCollisionObject* obB = contactManifold->getBody1();

      int numContacts = contactManifold->getNumContacts();
      for (int j = 0; j < numContacts; j++) {
        btManifoldPoint& pt = contactManifold->getContactPoint(j);
        if (pt.getDistance() < 0.f) {
          btVector3 ptA = pt.getPositionWorldOnA();
          btVector3 ptB = pt.getPositionWorldOnB();
          btVector3 normalOnB = pt.m_normalWorldOnB;

          // 🔍 여기에 충돌 이벤트 처리
          std::cout << "Collision detected!" << std::endl;
        }
      }
    }
  }

  void PhysicsEngine::addBoxRigidBody(LveGameObject& obj, float mass) {
    using namespace glm;

    // C++20 ONLY
    // if (rigidBodies.contains(obj.getId())) return; // 중복 방지
    if (rigidBodies.find(obj.getId()) != rigidBodies.end()) {
      return; // 이미 존재
    }

    // 1. 충돌 크기 (박스 기준) — GameObject의 크기 사용 (절반 크기로)
    vec3 halfExtents = vec3(0.5f);  // 임시 기본값
    if (obj.model) {
      auto bounds = obj.model->getBoundingBox();  // AABB 정보가 있다면
      halfExtents = (bounds.max - bounds.min) * 0.5f;
    }

    auto shape = new btBoxShape(btVector3(halfExtents.x, halfExtents.y, halfExtents.z));

    // 2. 초기 위치
    vec3 pos = obj.transform.translation;
    btTransform startTransform;
    startTransform.setIdentity();
    startTransform.setOrigin(btVector3(pos.x, pos.y, pos.z));

    // 3. 관성 계산 (정지 물체면 관성 0)
    btVector3 inertia(0, 0, 0);
    if (mass != 0.f) {
      shape->calculateLocalInertia(mass, inertia);
    }

    // 4. MotionState
    auto motionState = new btDefaultMotionState(startTransform);

    // 5. RigidBody 생성
    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, shape, inertia);
    auto rigidBody = new btRigidBody(rbInfo);

    // 6. 동적 세계에 추가
    dynamicsWorld->addRigidBody(rigidBody);

    // 7. Map에 보관
    rigidBodies[obj.getId()] = PhysicsBody{rigidBody, shape, motionState};
    obj.hasPhysics = true;
  }

  void PhysicsEngine::removeRigidBody(LveGameObject& obj) {
    auto it = rigidBodies.find(obj.getId());
    if (it == rigidBodies.end()) return;

    auto& body = it->second;
    dynamicsWorld->removeRigidBody(body.rigidBody);

    delete body.rigidBody;
    delete body.shape;
    delete body.motionState;

    rigidBodies.erase(it);
    obj.hasPhysics = false;
  }

  void PhysicsEngine::syncTransforms(const std::vector<LveGameObject *> &objects){
    for (auto &[id, body] : rigidBodies){
      for (auto *obj : objects){
        if (obj == nullptr)
          continue;
        if (obj->getId() != id)
          continue;

        // transform 동기화
        btTransform trans;
        body.rigidBody->getMotionState()->getWorldTransform(trans);
        btVector3 origin = trans.getOrigin();

        obj->transform.translation = glm::vec3(origin.x(), origin.y(), origin.z());
        break; // 매칭된 거 찾았으면 루프 종료
      }
    }
  }
} // namespace lve