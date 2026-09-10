#include "vr_instance.h"

#include <string.h>
#include <stdio.h>

#include "../qcommon/q_shared.h"

#include "vr_base.h"
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
	const VR_Bool eyeGazeExtensionEnabled = VR_HasEnabledInstanceExtension(XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME);

	// Scoped to this call; supportsEyeGazeInteraction is copied out below
	// before the chain that holds it goes out of scope.
	XrSystemEyeGazeInteractionPropertiesEXT gazeProperties;
	memset(&gazeProperties, 0, sizeof(gazeProperties));
	gazeProperties.type = XR_TYPE_SYSTEM_EYE_GAZE_INTERACTION_PROPERTIES_EXT;

	systemProperties->SystemProperties.type = XR_TYPE_SYSTEM_PROPERTIES;
	// A runtime is entitled to reject an unrecognized next struct, so this is
	// only chained when the extension was actually enabled on the instance.
	systemProperties->SystemProperties.next = eyeGazeExtensionEnabled ? &gazeProperties : NULL;

	XR_CHECK(
		xrGetSystemProperties(instance, systemId, &systemProperties->SystemProperties),
		"Failed to get SystemProperties");

	systemProperties->SupportsEyeGaze = eyeGazeExtensionEnabled ? gazeProperties.supportsEyeGazeInteraction : VR_FALSE;
	// systemProperties outlives this call; it must not keep a pointer into a dead stack frame.
	systemProperties->SystemProperties.next = NULL;

	// Graphics requirements are fetched separately via VR_Graphics_GetRequirements()

	return XR_SUCCESS;
}
