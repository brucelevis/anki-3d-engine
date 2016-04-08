// Copyright (C) 2009-2016, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <anki/scene/ParticleEmitter.h>
#include <anki/scene/SceneGraph.h>
#include <anki/scene/Misc.h>
#include <anki/resource/Model.h>
#include <anki/resource/ResourceManager.h>
#include <anki/util/Functions.h>
#include <anki/physics/PhysicsWorld.h>
#include <anki/Gr.h>

namespace anki
{

//==============================================================================
// Misc                                                                        =
//==============================================================================

//==============================================================================
static F32 getRandom(F32 initial, F32 deviation)
{
	return (deviation == 0.0) ? initial : initial + randFloat(deviation) * 2.0
			- deviation;
}

//==============================================================================
static Vec3 getRandom(const Vec3& initial, const Vec3& deviation)
{
	if(deviation == Vec3(0.0))
	{
		return initial;
	}
	else
	{
		Vec3 out;
		for(U i = 0; i < 3; i++)
		{
			out[i] = getRandom(initial[i], deviation[i]);
		}
		return out;
	}
}

//==============================================================================
// ParticleBase                                                                =
//==============================================================================

//==============================================================================
void ParticleBase::revive(const ParticleEmitter& pe,
	const Transform& trf,
	F32 /*prevUpdateTime*/,
	F32 crntTime)
{
	ANKI_ASSERT(isDead());
	const ParticleEmitterProperties& props = pe;

	// life
	m_timeOfDeath = getRandom(
		crntTime + props.m_particle.m_life, props.m_particle.m_lifeDeviation);
	m_timeOfBirth = crntTime;
}

//==============================================================================
// ParticleSimple                                                              =
//==============================================================================

//==============================================================================
void ParticleSimple::simulate(
	const ParticleEmitter& pe, F32 prevUpdateTime, F32 crntTime)
{
	F32 dt = crntTime - prevUpdateTime;

	Vec4 xp = m_position;
	Vec4 xc = m_acceleration * (dt * dt) + m_velocity * dt + xp;

	m_position = xc;

	m_velocity += m_acceleration * dt;
}

//==============================================================================
void ParticleSimple::revive(const ParticleEmitter& pe,
	const Transform& trf,
	F32 prevUpdateTime,
	F32 crntTime)
{
	ParticleBase::revive(pe, trf, prevUpdateTime, crntTime);
	m_velocity = Vec4(0.0);

	const ParticleEmitterProperties& props = pe;

	m_acceleration = getRandom(props.m_particle.m_gravity,
						 props.m_particle.m_gravityDeviation)
						 .xyz0();

	// Set the initial position
	m_position = getRandom(props.m_particle.m_startingPos,
					 props.m_particle.m_startingPosDeviation)
					 .xyz0();

	m_position += trf.getOrigin();
}

//==============================================================================
// Particle                                                                    =
//==============================================================================

#if 0

//==============================================================================
Particle::Particle(
	const char* name, SceneGraph* scene, // SceneNode
	// RigidBody
	PhysicsWorld* masterContainer, const RigidBody::Initializer& init_)
	:	ParticleBase(name, scene, PT_PHYSICS)
{
	RigidBody::Initializer init = init_;

	getSceneGraph().getPhysics().newPhysicsObject<RigidBody>(body, init);

	sceneNodeProtected.rigidBodyC = body;
}

//==============================================================================
Particle::~Particle()
{
	getSceneGraph().getPhysics().deletePhysicsObject(body);
}

//==============================================================================
void Particle::revive(const ParticleEmitter& pe,
	F32 prevUpdateTime, F32 crntTime)
{
	ParticleBase::revive(pe, prevUpdateTime, crntTime);

	const ParticleEmitterProperties& props = pe;

	// pre calculate
	Bool forceFlag = props.forceEnabled;
	Bool worldGravFlag = props.wordGravityEnabled;

	// activate it (Bullet stuff)
	body->forceActivationState(ACTIVE_TAG);
	body->activate();
	body->clearForces();
	body->setLinearVelocity(btVector3(0.0, 0.0, 0.0));
	body->setAngularVelocity(btVector3(0.0, 0.0, 0.0));

	// force
	if(forceFlag)
	{
		Vec3 forceDir = getRandom(props.particle.forceDirection,
			props.particle.forceDirectionDeviation);
		forceDir.normalize();

		if(!pe.identityRotation)
		{
			// the forceDir depends on the particle emitter rotation
			forceDir = pe.getWorldTransform().getRotation() * forceDir;
		}

		F32 forceMag = getRandom(props.particle.forceMagnitude,
			props.particle.forceMagnitudeDeviation);

		body->applyCentralForce(toBt(forceDir * forceMag));
	}

	// gravity
	if(!worldGravFlag)
	{
		body->setGravity(toBt(getRandom(props.particle.gravity,
			props.particle.gravityDeviation)));
	}

	// Starting pos. In local space
	Vec3 pos = getRandom(props.particle.startingPos,
		props.particle.startingPosDeviation);

	if(pe.identityRotation)
	{
		pos += pe.getWorldTransform().getOrigin();
	}
	else
	{
		pos.transform(pe.getWorldTransform());
	}

	btTransform trf(
		toBt(Transform(pos, pe.getWorldTransform().getRotation(), 1.0)));
	body->setWorldTransform(trf);
}
#endif

//==============================================================================
// ParticleEmitterRenderComponent                                              =
//==============================================================================

/// The derived render component for particle emitters.
class ParticleEmitterRenderComponent : public RenderComponent
{
public:
	const ParticleEmitter& getNode() const
	{
		return static_cast<const ParticleEmitter&>(getSceneNode());
	}

	ParticleEmitterRenderComponent(ParticleEmitter* node)
		: RenderComponent(node, &node->m_particleEmitterResource->getMaterial())
	{
	}

	ANKI_USE_RESULT Error buildRendering(
		RenderingBuildInfo& data) const override
	{
		return getNode().buildRendering(data);
	}

	void getRenderWorldTransform(
		Bool& hasTransform, Transform& trf) const override
	{
		hasTransform = true;
		// The particles are already in world position
		trf = Transform::getIdentity();
	}
};

//==============================================================================
// MoveFeedbackComponent                                                       =
//==============================================================================

/// Feedback component
class MoveFeedbackComponent : public SceneComponent
{
public:
	MoveFeedbackComponent(ParticleEmitter* node)
		: SceneComponent(SceneComponentType::NONE, node)
	{
	}

	ANKI_USE_RESULT Error update(
		SceneNode& node, F32, F32, Bool& updated) override
	{
		updated = false; // Don't care about updates for this component

		MoveComponent& move = node.getComponent<MoveComponent>();
		if(move.getTimestamp() == node.getGlobalTimestamp())
		{
			static_cast<ParticleEmitter&>(node).onMoveComponentUpdate(move);
		}

		return ErrorCode::NONE;
	}
};

//==============================================================================
// ParticleEmitter                                                             =
//==============================================================================

//==============================================================================
ParticleEmitter::ParticleEmitter(SceneGraph* scene)
	: SceneNode(scene)
{
}

//==============================================================================
ParticleEmitter::~ParticleEmitter()
{
	// Delete simple particles
	if(m_simulationType == SimulationType::SIMPLE)
	{
		for(ParticleBase* part : m_particles)
		{
			getSceneAllocator().deleteInstance(part);
		}
	}

	m_particles.destroy(getSceneAllocator());
}

//==============================================================================
Error ParticleEmitter::init(const CString& name, const CString& filename)
{
	ANKI_CHECK(SceneNode::init(name));
	SceneComponent* comp;

	// Load resource
	ANKI_CHECK(
		getResourceManager().loadResource(filename, m_particleEmitterResource));

	// Move component
	comp = getSceneAllocator().newInstance<MoveComponent>(this);
	addComponent(comp, true);

	// Move component feedback
	comp = getSceneAllocator().newInstance<MoveFeedbackComponent>(this);
	addComponent(comp, true);

	// Spatial component
	comp = getSceneAllocator().newInstance<SpatialComponent>(this, &m_obb);
	addComponent(comp, true);

	// Render component
	ParticleEmitterRenderComponent* rcomp =
		getSceneAllocator().newInstance<ParticleEmitterRenderComponent>(this);

	ANKI_CHECK(rcomp->init());
	comp = rcomp;
	addComponent(comp, true);

	// Other
	m_obb.setCenter(Vec4(0.0));
	m_obb.setExtend(Vec4(1.0, 1.0, 1.0, 0.0));
	m_obb.setRotation(Mat3x4::getIdentity());

	// copy the resource to me
	ParticleEmitterProperties& me = *this;
	const ParticleEmitterProperties& other =
		m_particleEmitterResource->getProperties();
	me = other;

	if(m_usePhysicsEngine)
	{
		createParticlesSimulation(&getSceneGraph());
		m_simulationType = SimulationType::PHYSICS_ENGINE;
	}
	else
	{
		createParticlesSimpleSimulation();
		m_simulationType = SimulationType::SIMPLE;
	}

	// Create the vertex buffer and object
	m_vertBuffSize = m_maxNumOfParticles * ParticleEmitterResource::VERTEX_SIZE;

	GrManager& gr = getSceneGraph().getGrManager();

	ResourceGroupInitInfo rcinit;
	m_particleEmitterResource->getMaterial().fillResourceGroupInitInfo(rcinit);

	for(U i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		m_vertBuffs[i] = gr.newInstance<Buffer>(m_vertBuffSize,
			BufferUsageBit::VERTEX,
			BufferAccessBit::CLIENT_MAP_WRITE);

		rcinit.m_vertexBuffers[0].m_buffer = m_vertBuffs[i];

		m_grGroups[i] = gr.newInstance<ResourceGroup>(rcinit);
	}

	return ErrorCode::NONE;
}

//==============================================================================
Error ParticleEmitter::buildRendering(RenderingBuildInfo& data) const
{
	ANKI_ASSERT(data.m_subMeshIndicesCount == 1);

	if(m_aliveParticlesCount == 0)
	{
		return ErrorCode::NONE;
	}

	PipelinePtr ppline =
		m_particleEmitterResource->getPipeline(data.m_key.m_lod);
	data.m_cmdb->bindPipeline(ppline);

	U frame = (getGlobalTimestamp() % 3);

	data.m_cmdb->bindResourceGroup(
		m_grGroups[frame], 0, data.m_dynamicBufferInfo);

	data.m_cmdb->drawArrays(m_aliveParticlesCount, data.m_subMeshIndicesCount);

	return ErrorCode::NONE;
}

//==============================================================================
void ParticleEmitter::onMoveComponentUpdate(MoveComponent& move)
{
	m_identityRotation =
		move.getWorldTransform().getRotation() == Mat3x4::getIdentity();

	SpatialComponent& sp = getComponent<SpatialComponent>();
	sp.setSpatialOrigin(move.getWorldTransform().getOrigin());
	sp.markForUpdate();
}

//==============================================================================
void ParticleEmitter::createParticlesSimulation(SceneGraph* scene)
{
#if 0
	collShape = getSceneAllocator().newInstance<btSphereShape>(particle.size);

	RigidBody::Initializer binit;
	binit.shape = collShape;
	binit.group = PhysicsWorld::CG_PARTICLE;
	binit.mask = PhysicsWorld::CG_MAP;

	particles.reserve(maxNumOfParticles);

	for(U i = 0; i < maxNumOfParticles; i++)
	{
		binit.mass = getRandom(particle.mass, particle.massDeviation);

		Particle* part = getSceneGraph().newSceneNode<Particle>(
			nullptr, &scene->getPhysics(), binit);

		part->size = getRandom(particle.size, particle.sizeDeviation);
		part->alpha = getRandom(particle.alpha, particle.alphaDeviation);
		part->getRigidBody()->forceActivationState(DISABLE_SIMULATION);

		particles.push_back(part);
	}
#endif
}

//==============================================================================
void ParticleEmitter::createParticlesSimpleSimulation()
{
	m_particles.create(getSceneAllocator(), m_maxNumOfParticles);

	for(U i = 0; i < m_maxNumOfParticles; i++)
	{
		ParticleSimple* part =
			getSceneAllocator().newInstance<ParticleSimple>();

		part->m_size = getRandom(m_particle.m_size, m_particle.m_sizeDeviation);
		part->m_alpha =
			getRandom(m_particle.m_alpha, m_particle.m_alphaDeviation);

		m_particles[i] = part;
	}
}

//==============================================================================
Error ParticleEmitter::frameUpdate(F32 prevUpdateTime, F32 crntTime)
{
	// - Deactivate the dead particles
	// - Calc the AABB
	// - Calc the instancing stuff
	//
	Vec4 aabbmin(MAX_F32, MAX_F32, MAX_F32, 0.0);
	Vec4 aabbmax(MIN_F32, MIN_F32, MIN_F32, 0.0);
	m_aliveParticlesCount = 0;

	U frame = getGlobalTimestamp() % 3;
	F32* verts = static_cast<F32*>(m_vertBuffs[frame]->map(
		0, m_vertBuffSize, BufferAccessBit::CLIENT_MAP_WRITE));
	const F32* verts_base = verts;
	(void)verts_base;

	for(ParticleBase* p : m_particles)
	{
		if(p->isDead())
		{
			// if its already dead so dont deactivate it again
			continue;
		}

		if(p->getTimeOfDeath() < crntTime)
		{
			// Just died
			p->kill();
		}
		else
		{
			// It's alive

			// Do checks
			ANKI_ASSERT((PtrSize(verts) + ParticleEmitterResource::VERTEX_SIZE
							- PtrSize(verts_base))
				<= m_vertBuffSize);

			// This will calculate a new world transformation
			p->simulate(*this, prevUpdateTime, crntTime);

			const Vec4& origin = p->getPosition();

			for(U i = 0; i < 3; i++)
			{
				aabbmin[i] = std::min(aabbmin[i], origin[i]);
				aabbmax[i] = std::max(aabbmax[i], origin[i]);
			}

			F32 lifePercent = (crntTime - p->getTimeOfBirth())
				/ (p->getTimeOfDeath() - p->getTimeOfBirth());

			verts[0] = origin.x();
			verts[1] = origin.y();
			verts[2] = origin.z();

			// XXX set a flag for scale
			verts[3] = p->m_size + (lifePercent * m_particle.m_sizeAnimation);

			// Set alpha
			if(m_particle.m_alphaAnimation)
			{
				verts[4] = sin((lifePercent)*getPi<F32>()) * p->m_alpha;
			}
			else
			{
				verts[4] = p->m_alpha;
			}

			++m_aliveParticlesCount;
			verts += 5;
		}
	}

	m_vertBuffs[frame]->unmap();

	if(m_aliveParticlesCount != 0)
	{
		Vec4 min = aabbmin - m_particle.m_size;
		Vec4 max = aabbmax + m_particle.m_size;
		Vec4 center = (min + max) / 2.0;

		m_obb = Obb(center, Mat3x4::getIdentity(), max - center);
	}
	else
	{
		m_obb = Obb(Vec4(0.0), Mat3x4::getIdentity(), Vec4(0.001));
	}

	getComponent<SpatialComponent>().markForUpdate();

	//
	// Emit new particles
	//
	if(m_timeLeftForNextEmission <= 0.0)
	{
		MoveComponent& move = getComponent<MoveComponent>();

		U particlesCount = 0; // How many particles I am allowed to emmit
		for(ParticleBase* pp : m_particles)
		{
			ParticleBase& p = *pp;
			if(!p.isDead())
			{
				// its alive so skip it
				continue;
			}

			p.revive(*this, move.getWorldTransform(), prevUpdateTime, crntTime);

			// do the rest
			++particlesCount;
			if(particlesCount >= m_particlesPerEmittion)
			{
				break;
			}
		} // end for all particles

		m_timeLeftForNextEmission = m_emissionPeriod;
	} // end if can emit
	else
	{
		m_timeLeftForNextEmission -= crntTime - prevUpdateTime;
	}

	return ErrorCode::NONE;
}

} // end namespace anki
