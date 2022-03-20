#include <cassert>
#include <chrono>
#include <string>
#include <iostream>
#include <fstream>
#include <span>
#include <sstream>
#include <ctime>

#include "common/types.h"
#include "network/ip_address.h"
#include "network/io_buffer.h"
#include "neptun/pcap/file_format.h"
#include "neptun/messages/packet_header.h"
#include "neptun/messages/segment.h"
#include "neptun/messages/reliable_message.h"
#include "neptun/messages/unreliable_message.h"

using namespace freezing;
using namespace freezing::network;

std::tuple<PcapGlobalHeader, bool> read_global_header(std::fstream &file) {
  PcapGlobalHeader global_header;
  file.read(reinterpret_cast<char *>(&global_header), sizeof(global_header));

  bool reverse_byte_order = false;
  if (global_header.magic_number == 0xd4c3b2a1) {
    reverse_byte_order = true;
  }

  if (reverse_byte_order) {
    global_header.reverse_byte_order();
  }
  return {global_header, reverse_byte_order};
}

PcapRecordHeader read_record_header(std::fstream &file, bool reverse_byte_order) {
  PcapRecordHeader record_header{};
  file.read(reinterpret_cast<char *>(&record_header), sizeof(record_header));

  if (reverse_byte_order) {
    record_header.reverse_byte_order();
  }
  return record_header;
}

byte_span read_packet_data(std::fstream &file, PcapRecordHeader record_header, byte_span buffer) {
  assert(record_header.incl_len == record_header.orig_len);
  file.read(reinterpret_cast<char *>(buffer.data()), record_header.incl_len);
  return buffer.first(record_header.incl_len);
}

EthernetFrame read_ether_frame(byte_span payload) {
  IoBuffer io{payload};
  EthernetFrame frame{};
  auto destination = io.read_byte_array(0, sizeof(frame.destination_mac_address));
  for (usize i = 0; i < sizeof(frame.destination_mac_address); i++) {
    frame.destination_mac_address[i] = destination[i];
  }
  auto source =
      io.read_byte_array(sizeof(frame.destination_mac_address), sizeof(frame.source_mac_address));
  for (usize i = 0; i < sizeof(frame.source_mac_address); i++) {
    frame.source_mac_address[i] = destination[i];
  }
  frame.ether_type =
      io.read_u16(sizeof(frame.destination_mac_address) + sizeof(frame.source_mac_address));
  return frame;
}

Ipv4Header read_ipv4_header(byte_span payload) {
  IoBuffer io{payload};
  Ipv4Header packet{};
  packet.version = io.read_bits<u8, 0, 4>();

  if (packet.version != 4) {
    // Malformed data.
    throw std::runtime_error("Not IPv4");
  }

  packet.ihl = io.read_bits<u8, 4, 8>();
  packet.dscp = io.read_bits<u8, 8, 14>();
  packet.ecn = io.read_bits<u8, 14, 16>();
  packet.total_length = io.read_bits<u16, 16, 32>();
  packet.identification = io.read_bits<u16, 32, 48>();
  packet.flags = io.read_bits<u8, 48, 51>();
  packet.fragment_offset = io.read_bits<u16, 51, 64>();
  packet.time_to_live = io.read_bits<u8, 64, 72>();
  packet.protocol = io.read_bits<u8, 72, 80>();
  packet.header_checksum = io.read_bits<u16, 80, 96>();
  packet.source_ip = io.read_bits<u32, 96, 128>();
  packet.destination_ip = io.read_bits<u32, 128, 160>();
  return packet;
}

UdpHeader read_udp_header(byte_span payload) {
  IoBuffer io{payload};
  UdpHeader header{};
  header.source_port = io.read_u16(0);
  header.destination_port = io.read_u16(2);
  header.length = io.read_u16(4);
  header.checksum = io.read_u16(6);
  return header;
}

std::chrono::sys_time<std::chrono::microseconds> to_sys_time(u32 s, u32 u) {
  std::chrono::seconds seconds(s);
  std::chrono::microseconds micros(u);
  return std::chrono::sys_time<std::chrono::microseconds>{seconds + micros};
}

std::string to_hex(byte_span payload) {
  std::stringstream ss;
  std::string sep{};
  for (auto byte : payload) {
    ss << sep << std::hex << (int) byte;
    sep = " ";
  }
  return ss.str();
};

std::string format_neptun_structure(byte_span payload) {
  // TODO: Header must have protocol_id to allow us to identify neptun packets.
  auto packet_header = PacketHeader(payload);
  payload = advance(payload, PacketHeader::kSerializedSize);
  std::stringstream ss;
  ss << "[packet_id=" << packet_header.id() << ", ack_seq_num="
     << packet_header.ack_sequence_number() << ", ack_bitmask=" << packet_header.ack_bitmask()
     << "]";
  ss << " ";

  auto append_segment = [&ss, &payload](const Segment &segment) {
    auto format_manager_type = [](u8 manager_type) {
      switch (manager_type) {
      case ManagerType::RELIABLE_STREAM:return "Reliable";
      case ManagerType::UNRELIABLE_STREAM:return "Unreliable";
      default:return "Unknown";
      }
    };

    auto append_reliable_segment = [&ss, &payload](usize msg_count) {
      for (usize idx = 0; idx < msg_count; idx++) {
        auto msg = ReliableMessage(payload);
        payload = advance(payload, msg.serialized_size());
        ss << "[seq_num=" << msg.sequence_number() << ", length=" << msg.length() << ", payload="
           << to_hex(msg.payload()) << "]";
      }
    };

    auto append_unreliable_segment = [&ss, &payload](usize msg_count) {
      for (usize idx = 0; idx < msg_count; idx++) {
        auto msg = UnreliableMessage(payload);
        payload = advance(payload, msg.serialized_size());
        ss << "[length=" << msg.length() << ", payload=" << to_hex(msg.payload()) << "]";
      }
    };

    // TODO: Group Segment and ReliableMessages into ReliableSegment message.
    ss << "[" << format_manager_type(segment.manager_type()) << ", msg_count="
       << std::to_string(segment.message_count()) << "]";
    payload = advance(payload, Segment::kSerializedSize);

    switch (segment.manager_type()) {
    case ManagerType::RELIABLE_STREAM:append_reliable_segment(segment.message_count());
      break;
    case ManagerType::UNRELIABLE_STREAM:append_unreliable_segment(segment.message_count());
      break;
    default:
      throw std::runtime_error("Unknown manager type: " + std::to_string(segment.manager_type()));
    }
  };

  std::string sep{};
  while (!payload.empty()) {
    ss << sep;
    auto segment = Segment(payload);
    append_segment(segment);
    sep = " ";
  }
  return ss.str();
}

void process_record(PcapRecordHeader record_header, byte_span record_payload, usize record_index) {
  EthernetFrame ether_frame = read_ether_frame(record_payload);
  record_payload = advance(record_payload, ether_frame.serialized_size());
  if (ether_frame.ether_type != EtherType::IPv4) {
    return;
  }

  Ipv4Header ip_header = read_ipv4_header(record_payload);
  record_payload = advance(record_payload, ip_header.serialized_size());
  if (ip_header.protocol != IpProtocol::UDP) {
    return;
  }

  UdpHeader udp_header = read_udp_header(record_payload);
  record_payload = advance(record_payload, udp_header.serialized_size());

  // Port is not known at the IP level.
  IpAddress source_ip = IpAddress::from_u32(ip_header.source_ip, udp_header.source_port);
  IpAddress
      destination_ip = IpAddress::from_u32(ip_header.destination_ip, udp_header.destination_port);

  auto time = to_sys_time(record_header.ts_sec, record_header.ts_usec);
  auto time_t = std::chrono::system_clock::to_time_t(time);
  std::string time_str(ctime(&time_t));
  if (time_str.back() == '\n') {
    time_str.erase(time_str.size() - 1);
  }

  // TODO: Neptun packets must have protocol id to identify them.
  // For now, filter by known port for prototyping purposes.
  bool is_neptun_packet = source_ip.port() == 7778 || source_ip.port() == 7779;
  std::string neptun_structure =
      is_neptun_packet ? format_neptun_structure(record_payload) : "<not neptun packet>";
  std::cout << record_index << ": " << time_str << " UDP " << source_ip.to_string() << " > "
            << destination_ip.to_string()
            << " length " << udp_header.length << " " << "neptun_payload=" << neptun_structure
            << std::endl;
}

void process_pcap_file(std::fstream &file) {
  auto[global_header, reverse_byte_order] = read_global_header(file);
  std::vector<u8> record_data_buffer(global_header.snaplen);
  usize count = 0;
  while (!file.eof()) {
    PcapRecordHeader record_header = read_record_header(file, reverse_byte_order);
    auto payload = read_packet_data(file, record_header, record_data_buffer);
    // Last record in the pcap file has a header with all zeroes.
    if (!payload.empty()) {
      // Count + 1 to be consistent with wireshark's view.
      process_record(record_header, payload, count + 1 /* record_index */);
      count++;
    }
  }
  std::cout << "Processed " << count << " records." << std::endl;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cout << "Usage: <filename>" << std::endl;
    std::cout
        << "To capture UDP packet data run: [sudo tcpdump -i lo udp -w ./test_data/lo-neptun-example.pcap]"
        << std::endl;
    return -1;
  }
  std::string filename(argv[1]);
  std::fstream file(filename, std::ifstream::binary | std::ifstream::in);

  if (file.fail()) {
    std::cerr << "Couldn't open the pcap file: " << filename << std::endl;
    return -1;
  }

  process_pcap_file(file);
  file.close();
  return 0;
}