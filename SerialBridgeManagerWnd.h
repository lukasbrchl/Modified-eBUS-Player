// *****************************************************************************
//
//     Copyright (c) 2011, Pleora Technologies Inc., All rights reserved.
//
// *****************************************************************************

#ifndef __SERIAL_BRIDGE_MANAGER_H__
#define __SERIAL_BRIDGE_MANAGER_H__

#include "IPlayerController.h"

#include <PvSerialBridgeManagerWnd.h>


///
/// \brief Simple class used to receive serial bridge manager events, forwards them to the player controller
///

class SerialBridgeManagerWnd : public PvSerialBridgeManagerWnd
{
public:

    SerialBridgeManagerWnd( IPlayerController *aController )
        : mController( aController )
    {
    }

    virtual ~SerialBridgeManagerWnd()
    {
    }

protected:

    void OnParameterArrayCreated( PvGenParameterArray *aArray, const PvString &aName )
    {
        mController->AddDeviceParameterArray( aArray, aName );
    }

    void OnParameterArrayDeleted( PvGenParameterArray *aArray )
    {
        mController->RemoveDeviceParameterArray( aArray );
    }

private:

    IPlayerController *mController;
};

#endif // __SERIAL_BRIDGE_MANAGER_H__
