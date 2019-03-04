// Copyright (C) 2009-2019, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <anki/renderer/GBufferPost.h>
#include <anki/renderer/Renderer.h>
#include <anki/renderer/GBuffer.h>
#include <anki/renderer/LightShading.h>
#include <anki/misc/ConfigSet.h>

namespace anki
{

GBufferPost::~GBufferPost()
{
}

Error GBufferPost::init(const ConfigSet& cfg)
{
	Error err = initInternal(cfg);
	if(err)
	{
		ANKI_R_LOGE("Failed to initialize GBufferPost pass");
	}
	return err;
}

Error GBufferPost::initInternal(const ConfigSet& cfg)
{
	ANKI_R_LOGI("Initializing GBufferPost pass");

	// Load shaders
	ANKI_CHECK(getResourceManager().loadResource("shaders/GBufferPost.glslp", m_prog));

	ShaderProgramResourceConstantValueInitList<3> consts(m_prog);
	consts.add("CLUSTER_COUNT_X", U32(cfg.getNumber("r.clusterSizeX")));
	consts.add("CLUSTER_COUNT_Y", U32(cfg.getNumber("r.clusterSizeY")));
	consts.add("CLUSTER_COUNT_Z", U32(cfg.getNumber("r.clusterSizeZ")));

	const ShaderProgramResourceVariant* variant;
	m_prog->getOrCreateVariant(consts.get(), variant);
	m_grProg = variant->getProgram();

	// Create FB descr
	m_fbDescr.m_colorAttachmentCount = 2;
	m_fbDescr.m_colorAttachments[0].m_loadOperation = AttachmentLoadOperation::LOAD;
	m_fbDescr.m_colorAttachments[1].m_loadOperation = AttachmentLoadOperation::LOAD;
	m_fbDescr.bake();

	return Error::NONE;
}

void GBufferPost::populateRenderGraph(RenderingContext& ctx)
{
	RenderGraphDescription& rgraph = ctx.m_renderGraphDescr;
	m_runCtx.m_ctx = &ctx;

	// Create pass
	GraphicsRenderPassDescription& rpass = rgraph.newGraphicsRenderPass("GBuffPost");

	rpass.setWork(
		[](RenderPassWorkContext& rgraphCtx) { static_cast<GBufferPost*>(rgraphCtx.m_userData)->run(rgraphCtx); },
		this,
		0);
	rpass.setFramebufferInfo(m_fbDescr, {{m_r->getGBuffer().getColorRt(0), m_r->getGBuffer().getColorRt(1)}}, {});

	rpass.newDependency({m_r->getGBuffer().getColorRt(0), TextureUsageBit::FRAMEBUFFER_ATTACHMENT_READ_WRITE});
	rpass.newDependency({m_r->getGBuffer().getColorRt(1), TextureUsageBit::FRAMEBUFFER_ATTACHMENT_READ_WRITE});
	rpass.newDependency({m_r->getGBuffer().getDepthRt(),
		TextureUsageBit::SAMPLED_FRAGMENT,
		TextureSubresourceInfo(DepthStencilAspectBit::DEPTH)});
}

void GBufferPost::run(RenderPassWorkContext& rgraphCtx)
{
	const RenderingContext& ctx = *m_runCtx.m_ctx;
	const ClusterBinOut& rsrc = ctx.m_clusterBinOut;
	CommandBufferPtr& cmdb = rgraphCtx.m_commandBuffer;

	cmdb->setViewport(0, 0, m_r->getWidth(), m_r->getHeight());
	cmdb->bindShaderProgram(m_grProg);

	cmdb->setBlendFactors(0, BlendFactor::ONE, BlendFactor::SRC_ALPHA, BlendFactor::ZERO, BlendFactor::ONE);
	cmdb->setBlendFactors(1, BlendFactor::ONE, BlendFactor::SRC_ALPHA, BlendFactor::ZERO, BlendFactor::ONE);

	// Bind all
	rgraphCtx.bindTextureAndSampler(0,
		0,
		m_r->getGBuffer().getDepthRt(),
		TextureSubresourceInfo(DepthStencilAspectBit::DEPTH),
		m_r->getNearestSampler());

	bindUniforms(cmdb, 0, 1, ctx.m_lightShadingUniformsToken);
	bindUniforms(cmdb, 0, 2, rsrc.m_decalsToken);

	cmdb->bindTextureAndSampler(0,
		3,
		(rsrc.m_diffDecalTexView) ? rsrc.m_diffDecalTexView : m_r->getDummyTextureView(),
		m_r->getTrilinearRepeatSampler(),
		TextureUsageBit::SAMPLED_FRAGMENT);
	cmdb->bindTextureAndSampler(0,
		4,
		(rsrc.m_specularRoughnessDecalTexView) ? rsrc.m_specularRoughnessDecalTexView : m_r->getDummyTextureView(),
		m_r->getTrilinearRepeatSampler(),
		TextureUsageBit::SAMPLED_FRAGMENT);

	bindStorage(cmdb, 0, 5, rsrc.m_clustersToken);
	bindStorage(cmdb, 0, 6, rsrc.m_indicesToken);

	// Draw
	cmdb->drawArrays(PrimitiveTopology::TRIANGLES, 3);

	// Restore state
	cmdb->setBlendFactors(0, BlendFactor::ONE, BlendFactor::ZERO);
	cmdb->setBlendFactors(1, BlendFactor::ONE, BlendFactor::ZERO);
}

} // end namespace anki
