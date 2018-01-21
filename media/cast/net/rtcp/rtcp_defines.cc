// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtcp/rtcp_defines.h"

#include "media/cast/logging/logging_defines.h"

namespace media {
namespace cast {

RtcpCastMessage::RtcpCastMessage(uint32 ssrc)
    : media_ssrc(ssrc), ack_frame_id(0u), target_delay_ms(0) {}
RtcpCastMessage::RtcpCastMessage()
    : media_ssrc(0), ack_frame_id(0u), target_delay_ms(0) {}
RtcpCastMessage::~RtcpCastMessage() {}

RtcpReceiverEventLogMessage::RtcpReceiverEventLogMessage()
    : type(UNKNOWN), packet_id(0u) {}
RtcpReceiverEventLogMessage::~RtcpReceiverEventLogMessage() {}

RtcpReceiverFrameLogMessage::RtcpReceiverFrameLogMessage(uint32 timestamp)
    : rtp_timestamp_(timestamp) {}
RtcpReceiverFrameLogMessage::~RtcpReceiverFrameLogMessage() {}

RtcpRpsiMessage::RtcpRpsiMessage()
    : remote_ssrc(0u), payload_type(0u), picture_id(0u) {}
RtcpRpsiMessage::~RtcpRpsiMessage() {}

RtcpNackMessage::RtcpNackMessage() : remote_ssrc(0u) {}
RtcpNackMessage::~RtcpNackMessage() {}

RtcpRembMessage::RtcpRembMessage() : remb_bitrate(0u) {}
RtcpRembMessage::~RtcpRembMessage() {}

RtcpReceiverReferenceTimeReport::RtcpReceiverReferenceTimeReport()
    : remote_ssrc(0u), ntp_seconds(0u), ntp_fraction(0u) {}
RtcpReceiverReferenceTimeReport::~RtcpReceiverReferenceTimeReport() {}

RtcpEvent::RtcpEvent() : type(UNKNOWN), packet_id(0u) {}
RtcpEvent::~RtcpEvent() {}

RtcpRttReport::RtcpRttReport() {}
RtcpRttReport::~RtcpRttReport() {}

}  // namespace cast
}  // namespace media
