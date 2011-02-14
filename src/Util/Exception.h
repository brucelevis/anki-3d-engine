#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <exception>
#include <string>


/// Custom exception that takes file, line and function that throw it. Throw it using the EXCEPTION macro
class Exception: public std::exception
{
	public:
		/// Constructor
		Exception(const std::string& err, const char* file = "unknown", int line = -1, const char* func = "unknown");

		/// Copy constructor
		Exception(const Exception& e);

		/// Destructor. Do nothing
		~Exception() throw() {}

		/// Return the error code
		const char* what() const throw();

	private:
		mutable std::string err;
		const char* file;
		int line;
		const char* func;
};


//======================================================================================================================
// Macros                                                                                                              =
//======================================================================================================================

#define EXCEPTION(x) Exception(std::string() + x, __FILE__, __LINE__, __PRETTY_FUNCTION__)

#if DEBUG_ENABLED == 1
	#define RASSERT_THROW_EXCEPTION(x) \
		if(x) \
			throw EXCEPTION("Reverse assertion failed: " #x)
#else
	#define RASSERT_THROW_EXCEPTION(x)
#endif



#endif
