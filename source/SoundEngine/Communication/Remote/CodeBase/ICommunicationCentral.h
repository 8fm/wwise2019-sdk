/***********************************************************************
  The content of this file includes source code for the sound engine
  portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
  Two Source Code" as defined in the Source Code Addendum attached
  with this file.  Any use of the Level Two Source Code shall be
  subject to the terms and conditions outlined in the Source Code
  Addendum and the End User License Agreement for Wwise(R).

  Version:  Build: 
  Copyright (c) 2006-2020 Audiokinetic Inc.
 ***********************************************************************/

/// \file 
/// Communication Central interface.
/// \sa
/// - \ref initialization_comm
/// - \ref framerendering_comm
/// - \ref termination_comm

#pragma once

namespace AK
{
	namespace Comm
	{
		class ICommunicationCentralNotifyHandler;
		class ICommandChannelHandler;
		class INotificationChannel;

		/// Communication Central interface.
		/// \sa
		/// - \ref initialization_comm
		/// - \ref framerendering_comm
		/// - \ref termination_comm
		class ICommunicationCentral
		{
		protected:
			/// Virtual destructor on interface to avoid warnings.
			virtual ~ICommunicationCentral(){}

		public:
			/// Initialize central.
			/// This method has to be called once, before any other method call. 
			/// \return Initialization successful (\b true or \b false).
			/// \sa
			/// - \ref initialization_comm
			virtual bool Init( 
				ICommunicationCentralNotifyHandler* in_pNotifyHandler,	///< Pointer to notify interface (part of AK::Comm::IProxyFrameworkConnected).
				ICommandChannelHandler* in_pCmdChannelHandler,			///< Pointer to channel interface (part of AK::Comm::IProxyFrameworkConnected).
				bool in_bInitSystemLib									///< Tells if the base console communication library should be initialized.  
				) = 0;

			virtual void PreTerm() = 0;

			/// Terminate central.
			/// This method has to be called once, before Destroy().
			/// \sa
			/// - \ref termination_comm
			virtual void Term() = 0;

			/// Destroy communication central.
			/// \sa
			/// - \ref termination_comm
			virtual void Destroy() = 0;

			/// Tell central to process communication at this time.
			/// This method has to be called periodically (usually once per game frame).
			/// \sa
			/// - \ref framerendering_comm
			/// \return Error code. Ak_Fail means that Central needs to be recovered, and should be reset; Ak_PartialSucces if not initialized; and Ak_Success otherwise
			virtual AKRESULT Process() = 0;

			/// Get notification channel.
			/// \return Pointer to notification channel interface.
			virtual INotificationChannel* GetNotificationChannel() = 0;

			/// Set the Network name used when connecting Wwise Authoring tool to the Application
			virtual void SetNetworkControllerName(const char* in_pszNetworkAppName) = 0;

			/// Interrupts connection and releases network resources.
			/// Network resources can be re-acquired using Resume().
			/// Closes all active sockets.
			/// \sa
			/// - \ref termination_comm
			virtual void Suspend() = 0;

			/// Re-acquire network resources released after a call to Suspend().
			/// Binds sockets again. Connections can then be restablished.
			/// \sa
			/// - \ref initialization_comm 
			virtual bool Resume() = 0;
		};
	}
}

