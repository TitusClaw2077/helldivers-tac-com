#include "protocol.h"

// Simple CRC32 implementation (no lookup table needed at this scale)
uint32_t taccom_crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
            else         crc >>= 1;
        }
    }
    return ~crc;
}

bool validatePacket(const uint8_t* data, int len, MessageType expectedType) {
    if (len < (int)sizeof(PacketHeader) + 4) return false;

    const PacketHeader* hdr = reinterpret_cast<const PacketHeader*>(data);
    if (hdr->magic   != PACKET_MAGIC)      return false;
    if (hdr->version != PROTOCOL_VERSION)  return false;
    if (hdr->msgType != (uint8_t)expectedType) return false;

    // CRC is last 4 bytes of packet
    size_t payloadStart = sizeof(PacketHeader);
    size_t crcOffset    = len - 4;
    uint32_t storedCrc  = *reinterpret_cast<const uint32_t*>(data + crcOffset);
    uint32_t calcCrc    = taccom_crc32(data, crcOffset);

    return storedCrc == calcCrc;
}
