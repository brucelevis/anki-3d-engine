// Copyright (C) 2009-2020, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <anki/resource/ShaderProgramResourceSystem.h>
#include <anki/resource/ResourceFilesystem.h>
#include <anki/util/Tracer.h>
#include <anki/gr/GrManager.h>
#include <anki/shader_compiler/ShaderProgramCompiler.h>
#include <anki/util/Filesystem.h>
#include <anki/util/ThreadHive.h>
#include <anki/util/System.h>

namespace anki
{

Error ShaderProgramResourceSystem::compileAllShaders(CString cacheDir, GrManager& gr, ResourceFilesystem& fs,
													 GenericMemoryPoolAllocator<U8>& alloc)
{
	ANKI_TRACE_SCOPED_EVENT(COMPILE_SHADERS);
	ANKI_RESOURCE_LOGI("Compiling shader programs");
	U32 shadersCompileCount = 0;

	ThreadHive threadHive(getCpuCoresCount(), alloc, false);

	// Compute hash for both
	const GpuDeviceCapabilities caps = gr.getDeviceCapabilities();
	const BindlessLimits limits = gr.getBindlessLimits();
	U64 gpuHash = computeHash(&caps, sizeof(caps));
	gpuHash = appendHash(&limits, sizeof(limits), gpuHash);
	gpuHash = appendHash(&SHADER_BINARY_VERSION, sizeof(SHADER_BINARY_VERSION), gpuHash);

	ANKI_CHECK(fs.iterateAllFilenames([&](CString fname) -> Error {
		// Check file extension
		StringAuto extension(alloc);
		getFilepathExtension(fname, extension);
		if(extension.getLength() != 8 || extension != "ankiprog")
		{
			return Error::NONE;
		}

		if(fname.find("/Rt") != CString::NPOS && !gr.getDeviceCapabilities().m_rayTracingEnabled)
		{
			// Skip RT programs
			return Error::NONE;
		}

		// Get some filenames
		StringAuto baseFname(alloc);
		getFilepathFilename(fname, baseFname);
		StringAuto metaFname(alloc);
		metaFname.sprintf("%s/%smeta", cacheDir.cstr(), baseFname.cstr());

		// Get the hash from the meta file
		U64 metafileHash = 0;
		if(fileExists(metaFname))
		{
			File metaFile;
			ANKI_CHECK(metaFile.open(metaFname, FileOpenFlag::READ | FileOpenFlag::BINARY));
			ANKI_CHECK(metaFile.read(&metafileHash, sizeof(metafileHash)));
		}

		// Load interface
		class FSystem : public ShaderProgramFilesystemInterface
		{
		public:
			ResourceFilesystem* m_fsystem = nullptr;

			Error readAllText(CString filename, StringAuto& txt) final
			{
				ResourceFilePtr file;
				ANKI_CHECK(m_fsystem->openFile(filename, file));
				ANKI_CHECK(file->readAllText(txt));
				return Error::NONE;
			}
		} fsystem;
		fsystem.m_fsystem = &fs;

		// Skip interface
		class Skip : public ShaderProgramPostParseInterface
		{
		public:
			U64 m_metafileHash;
			U64 m_newHash;
			U64 m_gpuHash;
			CString m_fname;

			Bool skipCompilation(U64 hash)
			{
				ANKI_ASSERT(hash != 0);
				const Array<U64, 2> hashes = {hash, m_gpuHash};
				const U64 finalHash = computeHash(hashes.getBegin(), hashes.getSizeInBytes());

				m_newHash = finalHash;
				const Bool skip = finalHash == m_metafileHash;

				if(!skip)
				{
					ANKI_RESOURCE_LOGI("\t%s", m_fname.cstr());
				}

				return skip;
			};
		} skip;
		skip.m_metafileHash = metafileHash;
		skip.m_newHash = 0;
		skip.m_gpuHash = gpuHash;
		skip.m_fname = fname;

		// Threading interface
		class TaskManager : public ShaderProgramAsyncTaskInterface
		{
		public:
			ThreadHive* m_hive = nullptr;
			GenericMemoryPoolAllocator<U8> m_alloc;

			void enqueueTask(void (*callback)(void* userData), void* userData)
			{
				class Ctx
				{
				public:
					void (*m_callback)(void* userData);
					void* m_userData;
					GenericMemoryPoolAllocator<U8> m_alloc;
				};
				Ctx* ctx = m_alloc.newInstance<Ctx>();
				ctx->m_callback = callback;
				ctx->m_userData = userData;
				ctx->m_alloc = m_alloc;

				m_hive->submitTask(
					[](void* userData, U32 threadId, ThreadHive& hive, ThreadHiveSemaphore* signalSemaphore) {
						Ctx* ctx = static_cast<Ctx*>(userData);
						ctx->m_callback(ctx->m_userData);
						auto alloc = ctx->m_alloc;
						alloc.deleteInstance(ctx);
					},
					ctx);
			}

			Error joinTasks()
			{
				m_hive->waitAllTasks();
				return Error::NONE;
			}
		} taskManager;
		taskManager.m_hive = &threadHive;
		taskManager.m_alloc = alloc;

		// Compile
		ShaderProgramBinaryWrapper binary(alloc);
		ANKI_CHECK(compileShaderProgram(fname, fsystem, &skip, &taskManager, alloc, caps, limits, binary));

		const Bool cachedBinIsUpToDate = metafileHash == skip.m_newHash;
		if(!cachedBinIsUpToDate)
		{
			++shadersCompileCount;
		}

		// Update the meta file
		if(!cachedBinIsUpToDate)
		{
			File metaFile;
			ANKI_CHECK(metaFile.open(metaFname, FileOpenFlag::WRITE | FileOpenFlag::BINARY));
			ANKI_CHECK(metaFile.write(&skip.m_newHash, sizeof(skip.m_newHash)));
		}

		// Save the binary to the cache
		if(!cachedBinIsUpToDate)
		{
			StringAuto storeFname(alloc);
			storeFname.sprintf("%s/%sbin", cacheDir.cstr(), baseFname.cstr());
			ANKI_CHECK(binary.serializeToFile(storeFname));
		}

		return Error::NONE;
	}));

	ANKI_RESOURCE_LOGI("Compiled %u shader programs", shadersCompileCount);
	return Error::NONE;
}

Error ShaderProgramResourceSystem::createRtPrograms(CString cacheDir, GrManager& gr, ResourceFilesystem& fs,
													GenericMemoryPoolAllocator<U8>& alloc)
{
	ANKI_TRACE_SCOPED_EVENT(COMPILE_SHADERS);
	ANKI_RESOURCE_LOGI("Creating ray tracing programs");

	// Gather the RT program fnames
	StringListAuto rtPrograms(alloc);
	ANKI_CHECK(fs.iterateAllFilenames([&](CString fname) -> Error {
		// Check file extension
		StringAuto extension(alloc);
		getFilepathExtension(fname, extension);
		if(extension.getLength() != 8 || extension != "ankiprog")
		{
			return Error::NONE;
		}

		if(fname.find("/Rt") == CString::NPOS)
		{
			// Skip non-RT programs
			return Error::NONE;
		}

		rtPrograms.pushBack(fname);

		return Error::NONE;
	}));

	// Group things together
	class Shader
	{
	public:
		ShaderPtr m_shader;
		U64 m_hash = 0;
	};

	class HitGroup
	{
	public:
		U32 m_chit = MAX_U32;
		U32 m_ahit = MAX_U32;
		U64 m_mutationHash = 0;
	};

	class RayType
	{
	public:
		RayType(GenericMemoryPoolAllocator<U8> alloc)
			: m_name(alloc)
			, m_hitGroups(alloc)
		{
		}

		U32 m_miss = MAX_U32;
		StringAuto m_name;
		DynamicArrayAuto<HitGroup> m_hitGroups;
	};

	class Lib
	{
	public:
		Lib(GenericMemoryPoolAllocator<U8> alloc)
			: m_alloc(alloc)
		{
		}

		GenericMemoryPoolAllocator<U8> m_alloc;
		StringAuto m_name{m_alloc};
		ShaderPtr m_rayGenShader;
		DynamicArrayAuto<Shader> m_shaders{m_alloc};
		DynamicArrayAuto<RayType> m_rayTypes{m_alloc};
	};

	DynamicArrayAuto<Lib> libs(alloc);

	for(const String& filename : rtPrograms)
	{
		// Get the binary
		StringAuto baseFilename(alloc);
		getFilepathFilename(filename, baseFilename);
		StringAuto binaryFilename(alloc);
		binaryFilename.sprintf("%s/%sbin", cacheDir.cstr(), baseFilename.cstr());
		ShaderProgramBinaryWrapper binaryw(alloc);
		ANKI_CHECK(binaryw.deserializeFromFile(binaryFilename));
		const ShaderProgramBinary& binary = binaryw.getBinary();

		// Checks
		if(binary.m_libraryName[0] == '\0')
		{
			ANKI_RESOURCE_LOGE("Library is missing from program: %s", filename.cstr());
			return Error::USER_DATA;
		}

		CString subLibrary;
		if(binary.m_subLibraryName[0] != '\0')
		{
			subLibrary = &binary.m_subLibraryName[0];
		}

		// Create the program name
		StringAuto progName(alloc);
		getFilepathFilename(filename, progName);
		char* cprogName = const_cast<char*>(progName.cstr());
		if(progName.getLength() > MAX_GR_OBJECT_NAME_LENGTH)
		{
			cprogName[MAX_GR_OBJECT_NAME_LENGTH] = '\0';
		}

		// Find or create the lib
		Lib* lib = nullptr;
		{
			for(Lib& l : libs)
			{
				if(l.m_name == CString(&binary.m_libraryName[0]))
				{
					lib = &l;
					break;
				}
			}

			if(lib == nullptr)
			{
				libs.emplaceBack(alloc);
				lib = &libs.getBack();
				lib->m_name.create(CString(&binary.m_libraryName[0]));
			}
		}

		// Ray gen
		if(!!(binary.m_presentShaderTypes & ShaderTypeBit::RAY_GEN))
		{
			if(lib->m_rayGenShader.get())
			{
				ANKI_RESOURCE_LOGE("The library already has a ray gen shader: %s", filename.cstr());
				return Error::USER_DATA;
			}

			if(!!(binary.m_presentShaderTypes & ~ShaderTypeBit::RAY_GEN))
			{
				ANKI_RESOURCE_LOGE("Ray gen can't co-exist with other types: %s", filename.cstr());
				return Error::USER_DATA;
			}

			if(binary.m_constants.getSize() || binary.m_mutators.getSize())
			{
				ANKI_RESOURCE_LOGE("Ray gen can't have spec constants or mutators ATM: %s", filename.cstr());
				return Error::USER_DATA;
			}

			ShaderInitInfo inf(cprogName);
			inf.m_shaderType = ShaderType::RAY_GEN;
			inf.m_binary = binary.m_codeBlocks[0].m_binary;
			lib->m_rayGenShader = gr.newShader(inf);
		}

		// Miss shaders
		if(!!(binary.m_presentShaderTypes & ShaderTypeBit::MISS))
		{
			if(!!(binary.m_presentShaderTypes & ~ShaderTypeBit::MISS))
			{
				ANKI_RESOURCE_LOGE("Miss shaders can't co-exist with other types: %s", filename.cstr());
				return Error::USER_DATA;
			}

			if(binary.m_constants.getSize() || binary.m_mutators.getSize())
			{
				ANKI_RESOURCE_LOGE("Miss can't have spec constants or mutators ATM: %s", filename.cstr());
				return Error::USER_DATA;
			}

			if(subLibrary.getLength() == 0)
			{
				ANKI_RESOURCE_LOGE("Miss shader should have set the sub-library to be used as ray type: %s",
								   filename.cstr());
				return Error::USER_DATA;
			}

			RayType* rayType = nullptr;
			for(RayType& rt : lib->m_rayTypes)
			{
				if(rt.m_name == subLibrary)
				{
					rayType = &rt;
					break;
				}
			}

			if(rayType == nullptr)
			{
				lib->m_rayTypes.emplaceBack(alloc);
				rayType = &lib->m_rayTypes.getBack();
				rayType->m_name.create(subLibrary);
			}

			if(rayType->m_miss != MAX_U32)
			{
				ANKI_RESOURCE_LOGE(
					"There is another miss program with the same library and sub-library names with this: %s",
					filename.cstr());
				return Error::USER_DATA;
			}

			Shader* shader = nullptr;
			for(Shader& s : lib->m_shaders)
			{
				if(s.m_hash == binary.m_codeBlocks[0].m_hash)
				{
					shader = &s;
					break;
				}
			}

			if(shader == nullptr)
			{
				shader = lib->m_shaders.emplaceBack();

				ShaderInitInfo inf(cprogName);
				inf.m_shaderType = ShaderType::MISS;
				inf.m_binary = binary.m_codeBlocks[0].m_binary;
				shader->m_shader = gr.newShader(inf);
				shader->m_hash = binary.m_codeBlocks[0].m_hash;
			}

			rayType->m_miss = U32(shader - &lib->m_shaders[0]);
		}

		// Hit shaders
		if(!!(binary.m_presentShaderTypes & (ShaderTypeBit::ANY_HIT | ShaderTypeBit::CLOSEST_HIT)))
		{
			if(!!(binary.m_presentShaderTypes & ~(ShaderTypeBit::ANY_HIT | ShaderTypeBit::CLOSEST_HIT)))
			{
				ANKI_RESOURCE_LOGE("Hit shaders can't co-exist with other types: %s", filename.cstr());
				return Error::USER_DATA;
			}

			// Iterate all mutations
			for(U32 m = 0; m < binary.m_mutations.getSize(); ++m)
			{
			}
		}
	}

	return Error::NONE;
}

} // end namespace anki
