#include "light/BaseLight.hpp"
#include "logging/log.hpp"
#include "packet2.h"
#include <cstdio>

namespace Light
{
BaseLight::BaseLight()
    : _color(1.0f, 1.0f, 1.0f, 1.0f), _direction(0.0f, -1.0f, 0.0f, 0.0f), _ambientIntensity(0.0f),
      _diffuseIntensity(0.0f), _specularIntensity(0.0f)
{
    packetInformation = packet2_create(8, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1);
    UpdatePacketInformation();
}

void BaseLight::UpdatePacketInformation()
{
    packet2_reset(packetInformation, 0);
    // Breakpoint
    packet2_add_float(packetInformation, _direction.x);
    packet2_add_float(packetInformation, _direction.y);
    packet2_add_float(packetInformation, _direction.z);
    packet2_add_float(packetInformation, _direction.w);
    packet2_add_float(packetInformation, _ambientIntensity);
    packet2_add_float(packetInformation, _diffuseIntensity);
    packet2_add_float(packetInformation, _specularIntensity);
    packet2_add_float(packetInformation, 1.0f);
}

BaseLight::~BaseLight()
{
    packet2_free(packetInformation);
}
} // namespace Light
