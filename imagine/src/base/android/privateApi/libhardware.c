#include <imagine/util/builtins.h>
#include <imagine/logger/logger.h>
#include <assert.h>
#include <dlfcn.h>
#include "libhardware.h"

typedef int (*hw_get_moduleProto)(const char *id, const struct hw_module_t **module);

static hw_get_moduleProto hw_get_moduleSym = 0;

bool libhardware_dl()
{
	if(hw_get_moduleSym)
		return true;
	void *libhardware = dlopen("libhardware.so", RTLD_LAZY);
	if(!libhardware)
	{
		logErr("libhardware not found");
		return false;
	}
	hw_get_moduleSym = (hw_get_moduleProto)dlsym(libhardware, "hw_get_module");
	if(!hw_get_moduleSym)
	{
		logErr("missing libhardware functions");
		dlclose(libhardware);
		hw_get_moduleSym = 0;
		return false;
	}
	logMsg("libhardware symbols loaded");
	return true;
}

int hw_get_module(const char *id, const struct hw_module_t **module)
{
	assert(hw_get_moduleSym);
	return hw_get_moduleSym(id, module);
}
