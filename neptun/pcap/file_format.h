//
// Created by freezing on 20/03/2022.
//

#ifndef NEPTUN_NEPTUN_PCAP_FILE_FORMAT_H
#define NEPTUN_NEPTUN_PCAP_FILE_FORMAT_H

#include <iostream>
#include <vector>
#include "common/types.h"

namespace freezing::network {

// https://wiki.wireshark.org/Development/LibpcapFileFormat#global-header
struct PcapGlobalHeader {
  u32 magic_number;   /* magic number */
  u16 version_major;  /* major version number */
  u16 version_minor;  /* minor version number */
  i32 thiszone;       /* GMT to local correction */
  u32 sigfigs;        /* accuracy of timestamps */
  u32 snaplen;        /* max length of captured packets, in octets */
  u32 network;        /* data link type */

  void reverse_byte_order() {
    throw std::runtime_error("reverse byte order not implemented");
  }
};

// https://wiki.wireshark.org/Development/LibpcapFileFormat#record-packet-header
struct PcapRecordHeader {
  u32 ts_sec;         /* timestamp seconds */
  u32 ts_usec;        /* timestamp microseconds */
  u32 incl_len;       /* number of octets of packet saved in file */
  u32 orig_len;       /* actual length of packet */

  void reverse_byte_order() {
    throw std::runtime_error("reverse byte order not implemented");
  }
};

// TODO: Extract packet-related structure into packet_format.h
// Additionally, these should probably be messages.

// https://en.wikipedia.org/wiki/Ethernet_frame
struct EthernetFrame {
  u8 destination_mac_address[6];
  u8 source_mac_address[6];
  u16 ether_type;

  usize serialized_size() const {
    return sizeof(EthernetFrame);
  }
};

// https://en.wikipedia.org/wiki/EtherType
enum EtherType {
  IPv4 = 0x0800
};

// https://en.wikipedia.org/wiki/IPv4#Packet_structure
struct Ipv4Header {
  u8 version;           // 4 bits
  u8 ihl;               // 4 bits
  u8 dscp;              // 6 bits
  u8 ecn;               // 2 bits
  u16 total_length;     // 16 bits
  u16 identification;   // 16 bits
  u8 flags;             // 3 bits
  u16 fragment_offset;  // 13 bits
  u8 time_to_live;      // 8 bits
  u8 protocol;          // 8 bits
  u16 header_checksum;  // 16 bits
  u32 source_ip;        // 32 bits
  u32 destination_ip;   // 32 bits
  u8 options[36];       // 288 bits (only present if IHL > 5)

  usize serialized_size() const {
    if (ihl > 5) {
      return 56;
    } else {
      return 20;
    }
  }
};

// https://en.wikipedia.org/wiki/List_of_IP_protocol_numbers
enum IpProtocol {
  UDP = 0x11,
};

// TODO: Use fmt.
std::ostream &operator<<(std::ostream &os, PcapGlobalHeader global_header) {
  return os << "PcapGlobalHeader("
            << "magic_number=" << global_header.magic_number << ", version_major="
            << global_header.version_major
            << ", version_minor=" << global_header.version_minor << ", thiszone="
            << global_header.thiszone << ", sigfigs="
            << global_header.sigfigs << ", snaplen=" << global_header.snaplen << ", network="
            << global_header.network << ")";
}

std::ostream &operator<<(std::ostream &os, PcapRecordHeader record_header) {
  return os << "PcapRecordHeader(" << "ts_sec=" << record_header.ts_sec << ", ts_usec="
            << record_header.ts_usec
            << ", incl_len=" << record_header.incl_len << ", orig_len=" << record_header.orig_len
            << ")";
}

// https://en.wikipedia.org/wiki/User_Datagram_Protocol
struct UdpHeader {
  u16 source_port;
  u16 destination_port;
  u16 length;
  u16 checksum;

  usize serialized_size() const {
    return 4 * sizeof(u16);
  }
};

}

#endif //NEPTUN_NEPTUN_PCAP_FILE_FORMAT_H
