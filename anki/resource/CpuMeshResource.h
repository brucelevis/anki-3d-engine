// Copyright (C) 2009-2020, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <anki/resource/ResourceObject.h>
#include <anki/Math.h>
#include <anki/util/WeakArray.h>

namespace anki
{

/// @addtogroup resource
/// @{

/// CPU Mesh Resource. It contains the geometry packed in CPU buffers.
class CpuMeshResource : public ResourceObject
{
public:
	/// Default constructor
	CpuMeshResource(ResourceManager* manager);

	~CpuMeshResource();

	/// Load from a mesh file
	ANKI_USE_RESULT Error load(const ResourceFilename& filename, Bool async);

	ConstWeakArray<Vec3> getPositions() const
	{
		return m_positions;
	}

private:
	DynamicArray<Vec3> m_positions;
	DynamicArray<U32> m_indices;
};
/// @}

} // namespace anki