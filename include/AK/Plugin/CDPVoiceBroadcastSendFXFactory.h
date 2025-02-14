// CDPVoiceBroadcastSendFXFactory.h

#ifndef __CDPVOICEBORADCASTSENDFXFACTORY_H__
#define __CDPVOICEBORADCASTSENDFXFACTORY_H__

AK_STATIC_LINK_PLUGIN(CDPVoiceBroadcastSendFX)
AK_STATIC_LINK_PLUGIN( CDPVoiceBroadcastSendStereoFX )

AK_FUNC(void, CDPVoiceBroadcastSwapBuffers)(AK::IAkGlobalPluginContext* context, AkGlobalCallbackLocation location, void* cookie);

#endif // __CDPVOICEBORADCASTSENDFXFACTORY_H__
