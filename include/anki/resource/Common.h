// Copyright (C) 2014, Panagiotis Christopoulos Charitos.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#ifndef ANKI_RESOURCE_COMMON_H
#define ANKI_RESOURCE_COMMON_H

#include "anki/util/Allocator.h"
#include "anki/util/Vector.h"
#include "anki/util/DArray.h"
#include "anki/util/String.h"

namespace anki {

// Forward
class GlDevice;
class ResourceManager;

/// @addtogroup resource
/// @{

template<typename T>
using ResourceAllocator = HeapAllocator<T>;

template<typename T>
using ResourceVector = Vector<T, ResourceAllocator<T>>;

template<typename T>
using ResourceDArray = DArray<T, ResourceAllocator<T>>;

using ResourceString = StringBase<ResourceAllocator<char>>;

template<typename T>
using TempResourceAllocator = StackAllocator<T>;

template<typename T>
using TempResourceVector = Vector<T, TempResourceAllocator<T>>;

template<typename T>
using TempResourceDArray = DArray<T, TempResourceAllocator<T>>;

using TempResourceString = StringBase<TempResourceAllocator<char>>;

/// Contains initialization information for the resource classes.
class ResourceInitializer
{
public:
	ResourceAllocator<U8>& m_alloc;
	TempResourceAllocator<U8>& m_tempAlloc;
	ResourceManager& m_resources;

	ResourceInitializer(
		ResourceAllocator<U8>& alloc, 
		TempResourceAllocator<U8>& tempAlloc,
		ResourceManager& resourceManager)
	:	m_alloc(alloc),
		m_tempAlloc(tempAlloc),
		m_resources(resourceManager)
	{}
};

/// @}

/// @privatesection
/// @{

// Sortcut
#define ANKI_CHECK(x_) \
	err = x_; \
	if(ANKI_UNLIKELY(err)) \
	{ \
		return err; \
	}

/// @}

} // end namespace anki

#endif