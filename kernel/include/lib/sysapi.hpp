#ifndef KOS_LIB_SYSAPI_HPP
#define KOS_LIB_SYSAPI_HPP

extern "C" void InitSysApi();

// Kernel-side utilities to set arguments for the next app
namespace kos { namespace sys {
	void SetArgs(int argc, const int8_t** argv, const int8_t* cmdline);
}}

#endif