// Copyright (C) 2009-2017, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <anki/gr/GrManager.h>
#include <anki/gr/vulkan/GrManagerImpl.h>

#include <anki/gr/Buffer.h>
#include <anki/gr/Texture.h>
#include <anki/gr/TextureView.h>
#include <anki/gr/Sampler.h>
#include <anki/gr/Shader.h>
#include <anki/gr/ShaderProgram.h>
#include <anki/gr/CommandBuffer.h>
#include <anki/gr/Framebuffer.h>
#include <anki/gr/OcclusionQuery.h>
#include <anki/gr/RenderGraph.h>

namespace anki
{

GrManager::GrManager()
{
}

GrManager::~GrManager()
{
	// Destroy in reverse order
	m_cacheDir.destroy(m_alloc);
}

Error GrManager::newInstance(GrManagerInitInfo& init, GrManager*& gr)
{
	auto alloc = HeapAllocator<U8>(init.m_allocCallback, init.m_allocCallbackUserData);

	GrManagerImpl* impl = alloc.newInstance<GrManagerImpl>();

	// Init
	impl->m_alloc = alloc;
	impl->m_cacheDir.create(alloc, init.m_cacheDirectory);
	Error err = impl->init(init);

	if(err)
	{
		alloc.deleteInstance(impl);
		gr = nullptr;
	}
	else
	{
		gr = impl;
	}

	return err;
}

void GrManager::deleteInstance(GrManager* gr)
{
	ANKI_ASSERT(gr);
	auto alloc = gr->m_alloc;
	gr->~GrManager();
	alloc.deallocate(gr, 1);
}

void GrManager::beginFrame()
{
	ANKI_VK_SELF(GrManagerImpl);
	self.beginFrame();
}

void GrManager::swapBuffers()
{
	ANKI_VK_SELF(GrManagerImpl);
	self.endFrame();
}

void GrManager::finish()
{
	ANKI_VK_SELF(GrManagerImpl);
	self.finish();
}

BufferPtr GrManager::newBuffer(const BufferInitInfo& init)
{
	return BufferPtr(Buffer::newInstance(this, init));
}

TexturePtr GrManager::newTexture(const TextureInitInfo& init)
{
	return TexturePtr(Texture::newInstance(this, init));
}

TextureViewPtr GrManager::newTextureView(const TextureViewInitInfo& init)
{
	return TextureViewPtr(TextureView::newInstance(this, init));
}

SamplerPtr GrManager::newSampler(const SamplerInitInfo& init)
{
	return SamplerPtr(Sampler::newInstance(this, init));
}

ShaderPtr GrManager::newShader(const ShaderInitInfo& init)
{
	return ShaderPtr(Shader::newInstance(this, init));
}

ShaderProgramPtr GrManager::newShaderProgram(const ShaderProgramInitInfo& init)
{
	return ShaderProgramPtr(ShaderProgram::newInstance(this, init));
}

CommandBufferPtr GrManager::newCommandBuffer(const CommandBufferInitInfo& init)
{
	return CommandBufferPtr(CommandBuffer::newInstance(this, init));
}

FramebufferPtr GrManager::newFramebuffer(const FramebufferInitInfo& init)
{
	return FramebufferPtr(Framebuffer::newInstance(this, init));
}

OcclusionQueryPtr GrManager::newOcclusionQuery()
{
	return OcclusionQueryPtr(OcclusionQuery::newInstance(this));
}

RenderGraphPtr GrManager::newRenderGraph()
{
	return RenderGraphPtr(RenderGraph::newInstance(this));
}

void GrManager::getUniformBufferInfo(U32& bindOffsetAlignment, PtrSize& maxUniformBlockSize) const
{
	ANKI_VK_SELF_CONST(GrManagerImpl);
	bindOffsetAlignment = self.getPhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment;
	maxUniformBlockSize = self.getPhysicalDeviceProperties().limits.maxUniformBufferRange;
}

void GrManager::getStorageBufferInfo(U32& bindOffsetAlignment, PtrSize& maxStorageBlockSize) const
{
	ANKI_VK_SELF_CONST(GrManagerImpl);
	bindOffsetAlignment = self.getPhysicalDeviceProperties().limits.minStorageBufferOffsetAlignment;
	maxStorageBlockSize = self.getPhysicalDeviceProperties().limits.maxStorageBufferRange;
}

void GrManager::getTextureBufferInfo(U32& bindOffsetAlignment, PtrSize& maxRange) const
{
	ANKI_VK_SELF_CONST(GrManagerImpl);
	bindOffsetAlignment = self.getPhysicalDeviceProperties().limits.minTexelBufferOffsetAlignment;
	maxRange = MAX_U32;
}

} // end namespace anki
