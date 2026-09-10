#include "vr_instance.h"

#include <string.h>
#include <stdio.h>

#include "../qcommon/q_shared.h"

#include "vr_macros.h"

XrResult VR_CreateInstance(const char* app_name, XrVersion api_version, uint32_t extensionsCount, const char* const* extensions, XrInstance* instance)
{
	XrApplicationInfo appInfo;
	memset(&appInfo, 0, sizeof(appInfo));
	Q_strncpyz(appInfo.applicationName, app_name, sizeof(appInfo.applicationName));
	appInfo.applicationVersion = 1;
	Q_strncpyz(appInfo.engineName, app_name, sizeof(appInfo.engineName));
	appInfo.engineVersion = 1;
	appInfo.apiVersion = api_version;

	XrInstanceCreateInfo instanceCreateInfo;
	memset(&instanceCreateInfo, 0, sizeof(instanceCreateInfo));
	instanceCreateInfo.type = XR_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.next = NULL;
	instanceCreateInfo.createFlags = 0;
	instanceCreateInfo.applicationInfo = appInfo;
	instanceCreateInfo.enabledApiLayerCount = 0;
	instanceCreateInfo.enabledApiLayerNames = NULL;
	instanceCreateInfo.enabledExtensionCount = extensionsCount;
	instanceCreateInfo.enabledExtensionNames = extensions;

	return xrCreateInstance(&instanceCreateInfo, instance);
}

XrResult VR_GetHMDSystem(XrInstance instance, XrSystemId* systemId)
{
	XrSystemGetInfo systemGetInfo;
	memset(&systemGetInfo, 0, sizeof(systemGetInfo));
	systemGetInfo.type = XR_TYPE_SYSTEM_GET_INFO;
	systemGetInfo.next = NULL;
	systemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

	return xrGetSystem(instance, &systemGetInfo, systemId);
}

// Graphics requirements are fetched via VR_Graphics_GetRequirements() in vrvk/vr_vk.c

XrResult VR_GetSystemProperties(XrInstance instance, XrSystemId systemId, VR_SystemProperties* systemProperties)
{
	systemProperties->SystemProperties.type = XR_TYPE_SYSTEM_PROPERTIES;
	systemProperties->SystemProperties.next = NULL;

	XR_CHECK(
		xrGetSystemProperties(instance, systemId, &systemProperties->SystemProperties),
		"Failed to get SystemProperties");

	// Graphics requirements are fetched separately via VR_Graphics_GetRequirements()

	return XR_SUCCESS;
}
