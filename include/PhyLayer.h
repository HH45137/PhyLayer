#pragma once

// PhyLayer.h
// 已集成 Box3D 后端实现（header-only adapter）
// 要启用：编译时需能找到 <box3d/box3d.h> 并链接 box3d 库

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <cstring>

using namespace glm;

#define PI glm::pi<float>()

// Box3D headers (需要在编译时可用)
#include <box3d/box3d.h>

namespace Physics
{
    using Vec3 = glm::vec3;
    using Quat = glm::quat;
    using BodyHandle = uint32_t;
    using ShapeHandle = uint32_t;

    enum class ShapeType
    {
        Sphere,
        Box,
        Capsule,
        Convex,
        Trimesh,
        Heightfield
    };

    struct BodyDesc
    {
        bool isDynamic = true;
        float mass = 1.0f;
        Vec3 position = Vec3(0.0f);
        Quat rotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
        Vec3 linearVelocity = Vec3(0.0f);
        Vec3 angularVelocity = Vec3(0.0f);
    };

    // 为了在不额外文件的情况下传递 shape 参数，扩展 ShapeDesc
    struct ShapeDesc
    {
        ShapeType type;
        // sphere parameters
        float radius = 0.5f;
        // box parameters (half extents)
        Vec3 halfExtents = Vec3(0.5f);
        // for trimesh we will pass vertices/indices directly through CreateTrimesh
    };

    struct Contact
    {
        Vec3 point;
        Vec3 normal;
        float penetration = 0.0f;
        BodyHandle a = 0;
        BodyHandle b = 0;
    };

    struct IContactListener
    {
        virtual ~IContactListener() = default;

        virtual void OnContactBegin(const Contact& c)
        {
        }

        virtual void OnContactPersist(const Contact& c)
        {
        }

        virtual void OnContactEnd(const Contact& c)
        {
        }
    };

    struct IPhysicsWorld
    {
        virtual ~IPhysicsWorld() = default;

        virtual void Step(float dt) = 0;

        virtual BodyHandle CreateBody(const BodyDesc& desc) = 0;

        virtual void DestroyBody(BodyHandle b) = 0;

        virtual void ApplyImpulse(BodyHandle b, const Vec3& imp, const Vec3& relPos) = 0;

        virtual void SetLinearVelocity(BodyHandle b, const Vec3& v) = 0;

        virtual Vec3 GetLinearVelocity(BodyHandle b) = 0;

        virtual void SetPosition(BodyHandle b, const Vec3& pos) = 0;

        virtual void GetPosition(BodyHandle b, Vec3& outPos, Quat& outRot) = 0;

        virtual ShapeHandle CreateShape(const ShapeDesc& desc) = 0;

        virtual void AttachShape(BodyHandle b, ShapeHandle s) = 0;

        virtual void DetachShape(BodyHandle b, ShapeHandle s) = 0;

        virtual void DestroyShape(ShapeHandle s) = 0;

        virtual ShapeHandle CreateTrimesh(const float* verts, size_t vcount, const int* indices, size_t icount,
                                          bool singlePrecision) = 0;

        virtual bool Raycast(const Vec3& from, const Vec3& to, Contact& outContact) = 0;

        virtual void SetContactListener(IContactListener* listener) = 0;

        virtual void SetGravity(const Vec3& g) = 0;

        virtual void DebugDraw() = 0;
    };

    enum class Backend
    {
        ODE,
        Box3D,
        Bullet,
        PhysX
    };

    // Forward declaration of factory - implemented below (Box3D adapter)
    std::unique_ptr<IPhysicsWorld> CreatePhysicsWorld(Backend backend);
} // namespace Physics

// ----------------- Box3D adapter implementation (header-only) -----------------
// This implements a minimal adapter for the Physics::IPhysicsWorld interface
// using Box3D C API.

namespace PhysicsBackend
{
    using namespace Physics;

    struct MeshRecord
    {
        b3MeshData* mesh = nullptr;
        b3ShapeId shapeId = {};
        b3BodyId bodyId = {};
    };

    class Box3DAdapter : public IPhysicsWorld
    {
    public:
        Box3DAdapter()
        {
            b3WorldDef def = b3DefaultWorldDef();
            // default gravity is typically {0, -10, 0} in Box3D, but set explicitly to be safe
            def.gravity = b3Vec3{0.0f, -9.81f, 0.0f};
            m_worldId = b3CreateWorld(&def);

            // ensure runtime gravity is set explicitly
            if (!B3_IS_NULL(m_worldId))
            {
                b3World_SetGravity(m_worldId, b3Vec3{0.0f, -9.81f, 0.0f});
            }

            m_nextBodyHandle = 1;
            m_nextShapeHandle = 1;
        }

        ~Box3DAdapter() override
        {
            // Destroy any meshes we created
            for (auto& kv : m_meshes)
            {
                MeshRecord& mr = kv.second;
                if (!B3_IS_NULL(mr.shapeId))
                {
                    b3DestroyShape(mr.shapeId, true);
                }
                if (!B3_IS_NULL(mr.bodyId))
                {
                    b3DestroyBody(mr.bodyId);
                }
                if (mr.mesh)
                {
                    b3DestroyMesh(mr.mesh);
                }
            }
            if (!B3_IS_NULL(m_worldId))
            {
                b3DestroyWorld(m_worldId);
                m_worldId = {};
            }
        }

        void Step(float dt) override
        {
            if (B3_IS_NULL(m_worldId)) return;
            int subSteps = 4;
            b3World_Step(m_worldId, dt, subSteps);
        }

        BodyHandle CreateBody(const BodyDesc& desc) override
        {
            b3BodyDef def = b3DefaultBodyDef();
            def.type = desc.isDynamic ? b3_dynamicBody : b3_staticBody;

            def.position = b3Pos{desc.position.x, desc.position.y, desc.position.z};
            def.rotation = b3Quat{{desc.rotation.x, desc.rotation.y, desc.rotation.z}, desc.rotation.w};

            def.linearVelocity = b3Vec3{desc.linearVelocity.x, desc.linearVelocity.y, desc.linearVelocity.z};
            def.angularVelocity = b3Vec3{desc.angularVelocity.x, desc.angularVelocity.y, desc.angularVelocity.z};

            b3BodyId bid = b3CreateBody(m_worldId, &def);

            BodyHandle h = m_nextBodyHandle++;
            {
                std::lock_guard<std::mutex> lk(m_mutex);
                m_bodies[h] = bid;
            }
            return h;
        }

        void DestroyBody(BodyHandle b) override
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            auto it = m_bodies.find(b);
            if (it == m_bodies.end()) return;
            b3BodyId bid = it->second;
            b3DestroyBody(bid);
            m_bodies.erase(it);
        }

        void ApplyImpulse(BodyHandle b, const Vec3& imp, const Vec3& relPos) override
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            auto it = m_bodies.find(b);
            if (it == m_bodies.end()) return;
            b3BodyId bid = it->second;
            b3Body_ApplyLinearImpulse(bid, b3Vec3{imp.x, imp.y, imp.z}, b3Pos{relPos.x, relPos.y, relPos.z}, true);
        }

        void SetLinearVelocity(BodyHandle b, const Vec3& v) override
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            auto it = m_bodies.find(b);
            if (it == m_bodies.end()) return;
            b3Body_SetLinearVelocity(it->second, b3Vec3{v.x, v.y, v.z});
        }

        Vec3 GetLinearVelocity(BodyHandle b) override
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            auto it = m_bodies.find(b);
            if (it == m_bodies.end()) return Vec3(0.0f);
            b3Vec3 v = b3Body_GetLinearVelocity(it->second);
            return Vec3(v.x, v.y, v.z);
        }

        void SetPosition(BodyHandle b, const Vec3& pos) override
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            auto it = m_bodies.find(b);
            if (it == m_bodies.end()) return;
            b3Body_SetTransform(it->second, b3Pos{pos.x, pos.y, pos.z}, b3Quat{{0, 0, 0}, 1.0f});
        }

        void GetPosition(BodyHandle b, Vec3& outPos, Quat& outRot) override
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            outPos = Vec3(0.0f);
            outRot = Quat(1.0f, 0, 0, 0);
            auto it = m_bodies.find(b);
            if (it == m_bodies.end()) return;
            b3Pos p = b3Body_GetPosition(it->second);
            b3Quat q = b3Body_GetRotation(it->second);
            outPos = Vec3(p.x, p.y, p.z);
            outRot = Quat(q.s, q.v.x, q.v.y, q.v.z); // glm::quat(w,x,y,z)
        }

        ShapeHandle CreateShape(const ShapeDesc& desc) override
        {
            ShapeHandle h = m_nextShapeHandle++;
            std::lock_guard<std::mutex> lk(m_mutex);
            m_shapes[h] = b3_nullShapeId; // placeholder
            m_shapeDefs[h] = desc;
            return h;
        }

        void AttachShape(BodyHandle b, ShapeHandle s) override
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            auto itB = m_bodies.find(b);
            auto itSdef = m_shapeDefs.find(s);
            if (itB == m_bodies.end() || itSdef == m_shapeDefs.end()) return;
            b3BodyId bid = itB->second;
            ShapeDesc sd = itSdef->second;

            // Prepare shapeDef and ensure density for dynamic bodies
            b3ShapeDef shapeDef = b3DefaultShapeDef();

            // if body is dynamic, ensure shape has non-zero density so mass is computed
            b3BodyType bt = b3Body_GetType(bid);
            if (bt == b3_dynamicBody)
            {
                shapeDef.density = 1.0f; // give a sensible default density
                shapeDef.updateBodyMass = true; // ensure body mass is updated
            }
            else
            {
                shapeDef.density = 0.0f;
                shapeDef.updateBodyMass = false;
            }

            b3ShapeId shapeId = {};
            if (sd.type == ShapeType::Sphere)
            {
                b3Sphere sphere;
                sphere.center = b3Vec3_zero;
                sphere.radius = sd.radius;
                shapeId = b3CreateSphereShape(bid, &shapeDef, &sphere);
            }
            else if (sd.type == ShapeType::Box)
            {
                b3BoxHull box = b3MakeBoxHull(sd.halfExtents.x, sd.halfExtents.y, sd.halfExtents.z);
                shapeId = b3CreateHullShape(bid, &shapeDef, &box.base);
            }
            else
            {
                // other types not implemented here
            }

            if (!B3_IS_NULL(shapeId))
            {
                m_shapes[s] = shapeId;
            }
        }

        void DetachShape(BodyHandle b, ShapeHandle s) override
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            auto itShape = m_shapes.find(s);
            if (itShape == m_shapes.end()) return;
            b3ShapeId sid = itShape->second;
            if (B3_IS_NULL(sid)) return;
            b3DestroyShape(sid, true);
            m_shapes.erase(itShape);
        }

        void DestroyShape(ShapeHandle s) override
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            auto itShape = m_shapes.find(s);
            if (itShape != m_shapes.end())
            {
                b3ShapeId sid = itShape->second;
                if (!B3_IS_NULL(sid))
                {
                    b3DestroyShape(sid, true);
                }
                m_shapes.erase(itShape);
            }
            m_shapeDefs.erase(s);
        }

        ShapeHandle CreateTrimesh(const float* verts, size_t vcount, const int* indices, size_t icount,
                                  bool singlePrecision) override
        {
            if (vcount == 0 || icount < 3) return 0;
            std::vector<b3Vec3> vbuf;
            vbuf.reserve(vcount);
            for (size_t i = 0; i < vcount; ++i)
            {
                const float* src = verts + 3 * i;
                vbuf.push_back(b3Vec3{src[0], src[1], src[2]});
            }
            b3MeshDef def = {};
            def.vertices = vbuf.data();
            def.vertexCount = (int)vcount;
            def.indices = (int*)indices;
            def.triangleCount = (int)(icount / 3);
            def.identifyEdges = true;
            def.useMedianSplit = true;
            def.weldVertices = true;
            def.weldTolerance = 0.0015f;

            b3MeshData* mesh = b3CreateMesh(&def, nullptr, 0);
            if (mesh == nullptr) return 0;

            b3BodyDef bodyDef = b3DefaultBodyDef();
            bodyDef.type = b3_staticBody;
            b3BodyId bodyId = b3CreateBody(m_worldId, &bodyDef);

            b3ShapeDef shapeDef = b3DefaultShapeDef();
            b3ShapeId shapeId = b3CreateMeshShape(bodyId, &shapeDef, mesh, b3Vec3_one);

            ShapeHandle h = m_nextShapeHandle++;
            {
                MeshRecord mr;
                mr.mesh = mesh;
                mr.shapeId = shapeId;
                mr.bodyId = bodyId;
                m_meshes[h] = mr;
                m_shapes[h] = shapeId;
            }
            return h;
        }

        bool Raycast(const Vec3& from, const Vec3& to, Contact& outContact) override
        {
            (void)from;
            (void)to;
            (void)outContact;
            return false;
        }

        void SetContactListener(IContactListener* listener) override
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_listener = listener;
        }

        void SetGravity(const Vec3& g) override
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_gravity = g;
            if (!B3_IS_NULL(m_worldId))
            {
                b3World_SetGravity(m_worldId, b3Vec3{g.x, g.y, g.z});
            }
        }

        void DebugDraw() override
        {
            // not implemented here
        }

    private:
        b3WorldId m_worldId = {};
        std::unordered_map<BodyHandle, b3BodyId> m_bodies;
        std::unordered_map<ShapeHandle, b3ShapeId> m_shapes;
        std::unordered_map<ShapeHandle, ShapeDesc> m_shapeDefs;
        std::unordered_map<ShapeHandle, MeshRecord> m_meshes;
        std::mutex m_mutex;
        BodyHandle m_nextBodyHandle = 1;
        ShapeHandle m_nextShapeHandle = 1;
        IContactListener* m_listener = nullptr;
        Vec3 m_gravity = Vec3(0.0f, -9.81f, 0.0f);
    };
} // namespace PhysicsBackend

// Factory implementation
namespace Physics
{
    std::unique_ptr<IPhysicsWorld> CreatePhysicsWorld(Backend backend)
    {
        if (backend == Backend::Box3D)
        {
            return std::unique_ptr<IPhysicsWorld>(new PhysicsBackend::Box3DAdapter());
        }
        // other backends not implemented
        return nullptr;
    }
}
