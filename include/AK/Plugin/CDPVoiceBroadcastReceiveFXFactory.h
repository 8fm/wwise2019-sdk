// CDPVoiceBroadcastReceiveFXFactory.h

#ifndef __CDPVOICEBORADCASTRECEIVEFXFACTORY_H__
#define __CDPVOICEBORADCASTRECEIVEFXFACTORY_H__

AK_STATIC_LINK_PLUGIN(CDPVoiceBroadcastReceiveFX)
AK_STATIC_LINK_PLUGIN( CDPVoiceBroadcastReceiveStereoFX )

AK_FUNC(void, CDPVoiceBroadcastSwapBuffers)(AK::IAkGlobalPluginContext* context, AkGlobalCallbackLocation location, void* cookie);

#endif // __CDPVOICEBORADCASTRECEIVEFXFACTORY_H__
