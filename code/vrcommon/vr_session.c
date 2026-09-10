#include "vr_session.h"

#include <string.h>

#include "vr_macros.h"
#include "../vrcommon/vr_graphics.h"

// Session creation is now delegated to graphics-specific implementation
// See vrvk/vr_vk_session.c

XrResult VR_CreateSession(XrInstance instance, XrSystemId systemId, XrSession* session)
{
	return VR_Graphics_CreateSession(instance, systemId, session);
}

XrResult VR_BeginSession(XrSession session)
{
	XrSessionBeginInfo sessionBeginInfo;
	memset(&sessionBeginInfo, 0, sizeof(sessionBeginInfo));
	sessionBeginInfo.type = XR_TYPE_SESSION_BEGIN_INFO;
	sessionBeginInfo.next = NULL;
	sessionBeginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

	return xrBeginSession(session, &sessionBeginInfo);
}

XrResult VR_EndSession(XrSession session)
{
	return xrEndSession(session);
}
