#include "ArmProtocol.h"

uint8_t arm_checksum(const uint8_t *data, size_t len)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) sum ^= data[i];
    return sum;
}

void arm_packet_seal(ArmPacket *p)
{
    p->magic = ARM_PACKET_MAGIC;
    p->checksum = arm_checksum((const uint8_t *)p, sizeof(ArmPacket) - 1);
}

bool arm_packet_valid(const ArmPacket *p)
{
    if (p->magic != ARM_PACKET_MAGIC) return false;
    return p->checksum ==
           arm_checksum((const uint8_t *)p, sizeof(ArmPacket) - 1);
}
