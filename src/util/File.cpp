#include "anki/util/File.h"
#include "anki/util/Exception.h"
#include "anki/util/Assert.h"
#include <fstream>
#include <cstring>
#include <cstdarg>

namespace anki {

//==============================================================================
// File                                                                        =
//==============================================================================

//==============================================================================
File::~File()
{
	if(file)
	{
		if(flags & FT_C)
		{
			fclose((FILE*)file);
		}
		else if(flags & FT_ZIP)
		{
			ANKI_ASSERT(0 && "Not implemented");
		}
		else
		{
			ANKI_ASSERT(0);
		}
	}
}

//==============================================================================
File::Endianness File::getMachineEndianness()
{
	I32 num = 1;
	if(*(char*)&num == 1)
	{
		return E_LITTLE_ENDIAN;
	}
	else
	{
		return E_BIG_ENDIAN;
	}
}

//==============================================================================
void File::open(const char* filename, U8 flags_)
{
	ANKI_ASSERT(filename);
	ANKI_ASSERT(file == nullptr && flags == 0);

	ANKI_ASSERT((flags_ & (OF_READ | OF_WRITE | OF_APPEND | OF_BINARY 
		| E_LITTLE_ENDIAN | E_BIG_ENDIAN)) != 0);

	ANKI_ASSERT((flags_ & OF_READ) != (flags_ & OF_WRITE));

	const char* openMode;

	if(flags_ & OF_READ)
	{
		openMode = "r";
	}
	else if(flags_ & OF_WRITE)
	{
		openMode = "w";
	}
	else if(flags_ & OF_APPEND)
	{
		openMode = "a+";
	}
	else
	{
		ANKI_ASSERT(0);
		openMode = nullptr;
	}

	// Open
	file = (FILE*)fopen(filename, openMode);
	if(!file)
	{
		throw ANKI_EXCEPTION("Failed to open file");
	}

	flags |= flags_ | FT_C;

	// Check endianness
	if((flags_ & (E_BIG_ENDIAN | E_LITTLE_ENDIAN)) == 0)
	{
		// Set the default endianness
		flags |= getMachineEndianness();
	}
	else
	{
		ANKI_ASSERT((flags_ & E_BIG_ENDIAN) != (flags_ & E_LITTLE_ENDIAN));
	}
}

//==============================================================================
void File::close()
{
	ANKI_ASSERT(file);

	if(flags & FT_C)
	{
		fclose((FILE*)file);
	}
	else if(flags & FT_ZIP)
	{
		ANKI_ASSERT(0 && "Not implemented");
	}
	else
	{
		ANKI_ASSERT(0);
	}

	file = nullptr;
	flags = 0;
}

//==============================================================================
void File::read(void* buff, PtrSize size)
{
	ANKI_ASSERT(buff);
	ANKI_ASSERT(size > 0);
	ANKI_ASSERT(file);
	ANKI_ASSERT(flags & OF_READ);

	PtrSize readSize = 0;

	if(flags & FT_C)
	{
		readSize = fread(buff, 1, size, (FILE*)file);
	}
	else if(flags & FT_ZIP)
	{
		ANKI_ASSERT(0 && "Not implemented");
	}
	else
	{
		ANKI_ASSERT(0);
	}

	if(size != readSize)
	{
		throw ANKI_EXCEPTION("File read failed");
	}
}

//==============================================================================
void File::readAllText(std::string& txt)
{
	ANKI_ASSERT(file);
	ANKI_ASSERT(flags);
	ANKI_ASSERT((flags & OF_BINARY) == 0 && "Should not be binary file");
	ANKI_ASSERT(flags & OF_READ);

	if(flags & FT_C)
	{
		// Get file size
		fseek((FILE*)file, 0, SEEK_END);
		PtrSize size = ftell((FILE*)file);
		rewind((FILE*)file);

		// Read and write
		txt.resize(size);
		PtrSize readSize = fread(&txt[0], 1, size, (FILE*)file);
		(void)readSize;
		ANKI_ASSERT(readSize == size);
	}
	else if(flags & FT_ZIP)
	{
		ANKI_ASSERT(0 && "Not implemented");
	}
	else
	{
		ANKI_ASSERT(0);
	}
}

//==============================================================================
void File::readAllTextLines(StringList& lines)
{
	ANKI_ASSERT(file);
	ANKI_ASSERT(flags);
	ANKI_ASSERT((flags & OF_BINARY) == 0 && "Should not be binary file");
	ANKI_ASSERT(flags & OF_READ);

	if(flags & FT_C)
	{
		char line[1024];

		while(fgets(line, sizeof(line), (FILE*)file))
		{
			I len = strlen(line);

			if(len != sizeof(line) - 1)
			{
				// line is big enough

				line[len - 1] = '\0';
				lines.push_back(line);
			}
			else
			{
				throw ANKI_EXCEPTION("Line bigger than temp buffer");
			}			
		}
	}
	else if(flags & FT_ZIP)
	{
		ANKI_ASSERT(0 && "Not implemented");
	}
	else
	{
		ANKI_ASSERT(0);
	}
}

//==============================================================================
U32 File::readU32()
{
	ANKI_ASSERT(file);
	ANKI_ASSERT(flags);
	ANKI_ASSERT(flags & OF_READ);
	ANKI_ASSERT((flags & OF_BINARY) && "Should be binary file");
	ANKI_ASSERT((flags & E_BIG_ENDIAN) != (flags & E_LITTLE_ENDIAN) 
		&& "One of those 2 should be active");

	U32 out;
	read(&out, sizeof(out));
	
	// Copy it
	Endianness machineEndianness = getMachineEndianness();
	Endianness fileEndianness = (flags & E_BIG_ENDIAN) 
		? E_BIG_ENDIAN : E_LITTLE_ENDIAN;

	if(machineEndianness == fileEndianness)
	{
		// Same endianness between the file and the machine. Do nothing
	}
	else if(machineEndianness == E_BIG_ENDIAN 
		&& fileEndianness == E_LITTLE_ENDIAN)
	{
		U8* c = (U8*)&out;
		out = (c[0] | (c[1] << 8) | (c[2] << 16) | (c[3] << 24));
	}
	else
	{
		U8* c = (U8*)&out;
		out = ((c[0] << 24) | (c[1] << 16) | (c[2] << 8) | c[3]);
	}

	return out;
}

//==============================================================================
F32 File::readF32()
{
	F32 out;
	U32 integer = readU32();

	memcpy(&out, &integer, sizeof(F32));

	return out;
}

//==============================================================================
void File::write(void* buff, PtrSize size)
{
	ANKI_ASSERT(buff);
	ANKI_ASSERT(size > 0);
	ANKI_ASSERT(file);
	ANKI_ASSERT(flags & OF_WRITE);

	if(flags & FT_C)
	{
		PtrSize writeSize = 0;
		writeSize = fwrite(buff, 1, size, (FILE*)file);

		if(writeSize != size)
		{
			throw ANKI_EXCEPTION("Failed to write on file");
		}
	}
	else if(flags & FT_ZIP)
	{
		ANKI_ASSERT(0 && "Not implemented");
	}
	else
	{
		ANKI_ASSERT(0);
	}
}

//==============================================================================
void File::writeText(const char* format, ...)
{
	ANKI_ASSERT(format);
	ANKI_ASSERT(file);
	ANKI_ASSERT(flags);
	ANKI_ASSERT(flags & OF_WRITE);
	ANKI_ASSERT((flags & OF_BINARY) == 0);

	va_list args;
	va_start(args, format);

	if(flags & FT_C)
	{
		vfprintf((FILE*)file, format, args);
	}
	else if(flags & FT_ZIP)
	{
		ANKI_ASSERT(0 && "Not implemented");
	}
	else
	{
		ANKI_ASSERT(0);
	}

	va_end(args);
}

//==============================================================================
void File::seek(PtrSize offset, SeekOrigin origin)
{
	ANKI_ASSERT(file);
	ANKI_ASSERT(flags);

	if(flags & FT_C)
	{
		if(fseek((FILE*)file, offset, origin) != 0)
		{
			throw ANKI_EXCEPTION("fseek() failed");
		}
	}
	else if(flags & FT_ZIP)
	{
		ANKI_ASSERT(0 && "Not implemented");
	}
	else
	{
		ANKI_ASSERT(0);
	}
}

//==============================================================================
const char* File::getFileExtension(const char* filename)
{
	ANKI_ASSERT(filename);
	const char* pc = strrchr(filename, '.');

	if(pc == nullptr)
	{
		return nullptr;
	}

	++pc;
	return (*pc == '\0') ? nullptr : pc;
}

} // end namespace anki