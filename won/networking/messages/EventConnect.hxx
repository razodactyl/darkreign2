#pragma once
#include <enet/enetpp.hxx>
#include <Networking/SendableEventMessage.hxx>

class EventConnect
{
public:
    UniqueClassId_Declare(EventConnect);

    ENetPeer* peer;
    EventConnect(ENetPeer* peer);
};
