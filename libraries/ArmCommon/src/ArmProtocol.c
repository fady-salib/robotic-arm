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
    if (!p || p->magic != ARM_PACKET_MAGIC || p->emg > 1000) return false;
    for (uint8_t i = 0; i < ARM_NUM_FINGERS; ++i) {
        if (p->flex[i] > 1000) return false;
    }
    return p->checksum ==
           arm_checksum((const uint8_t *)p, sizeof(ArmPacket) - 1);
}

bool arm_sequence_newer(uint8_t incoming, uint8_t previous)
{
    uint8_t delta = (uint8_t)(incoming - previous);
    return delta != 0 && delta < 128;
}
