#pragma once

#include <stdint.h>

#define CNC_PACKET_MAGIC 0x434E4331UL
#define CNC_PACKET_VERSION 1
#define CNC_LABEL_SIZE 16

typedef struct __attribute__((packed)) {
  uint32_t magic;
  uint8_t version;

  uint32_t sequence_no;

  // Optional. If empty, gateway assigns CNC-01, CNC-02...
  char node_label[CNC_LABEL_SIZE];

  uint16_t tool_id;

  float depth_of_cut_ap;
  float cutting_speed_vc;
  float feed_rate_f;
  float machined_length;

  float fx;
  float fy;
  float fz;
  float total_force;

  float ra;
  float rz;
  float rt;

  // 1 = RUNNING, 2 = STOPPED, 3 = ALARM
  uint8_t machine_status;

} CncEspNowPacket;
