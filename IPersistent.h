// *****************************************************************************
//
//     Copyright (c) 2013, Pleora Technologies Inc., All rights reserved.
//
// *****************************************************************************

#ifndef __IPERSISTENT_H__
#define __IPERSISTENT_H__

#include <PvConfigurationReader.h>
#include <PvConfigurationWriter.h>


class IPersistent
{
public:

    virtual PvResult Save( PvConfigurationWriter *aWriter ) = 0;
    virtual PvResult Load( PvConfigurationReader *aReader ) = 0;

};


#endif // __IPERSISTENT_H__

