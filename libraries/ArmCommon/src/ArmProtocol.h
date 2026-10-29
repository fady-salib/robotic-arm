/* ArmProtocol.h - wire format shared by the transmitter and the arm. */
#ifndef ARM_PROTOCOL_H
#define ARM_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ARM_NUM_FINGERS  5
#define ARM_PACKET_MAGIC 0xA5
#define ARM_UDP_PORT     4210

/* Flex/EMG values are calibrated to 0..1000. Multi-byte fields use AVR
 * little-endian order; the packed structure is not portable serialization.
 * XOR is error detection only, not authentication. */
typedef struct __attribute__((packed)) {
    uint8_t  magic;                    /* ARM_PACKET_MAGIC                */
    uint8_t  seq;                      /* modulo-256 rolling counter      */
    uint16_t flex[ARM_NUM_FINGERS];    /* thumb..pinky, 0 open, 1000 fist */
    uint16_t emg;                      /* muscle envelope, 0..1000        */
    uint8_t  bpm;                      /* heart rate, 0 = no reading      */
    uint8_t  checksum;                 /* XOR of every preceding byte     */
} ArmPacket;

uint8_t arm_checksum(const uint8_t *data, size_t len);
void    arm_packet_seal(ArmPacket *p);        /* sets magic + checksum */
bool    arm_packet_valid(const ArmPacket *p); /* magic, ranges, checksum */
/* Within a live session, reject duplicates/older packets; half-range ordering.
 * After link timeout the receiver starts a new sequence epoch. */
bool    arm_sequence_newer(uint8_t incoming, uint8_t previous);

#ifdef __cplusplus
}
#endif
#endif
