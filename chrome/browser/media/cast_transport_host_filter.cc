// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cast_transport_host_filter.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "media/cast/net/cast_transport_sender.h"

namespace {

// How often to send raw events.
const int kSendRawEventsIntervalSecs = 1;

}

namespace cast {

CastTransportHostFilter::CastTransportHostFilter()
    : BrowserMessageFilter(CastMsgStart),
      weak_factory_(this) {}

CastTransportHostFilter::~CastTransportHostFilter() {}

bool CastTransportHostFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CastTransportHostFilter, message)
    IPC_MESSAGE_HANDLER(CastHostMsg_New, OnNew)
    IPC_MESSAGE_HANDLER(CastHostMsg_Delete, OnDelete)
    IPC_MESSAGE_HANDLER(CastHostMsg_InitializeAudio, OnInitializeAudio)
    IPC_MESSAGE_HANDLER(CastHostMsg_InitializeVideo, OnInitializeVideo)
    IPC_MESSAGE_HANDLER(CastHostMsg_InsertCodedAudioFrame,
                        OnInsertCodedAudioFrame)
    IPC_MESSAGE_HANDLER(CastHostMsg_InsertCodedVideoFrame,
                        OnInsertCodedVideoFrame)
    IPC_MESSAGE_HANDLER(CastHostMsg_SendSenderReport,
                        OnSendSenderReport)
    IPC_MESSAGE_HANDLER(CastHostMsg_ResendPackets,
                        OnResendPackets)
    IPC_MESSAGE_UNHANDLED(handled = false);
  IPC_END_MESSAGE_MAP();
  return handled;
}

void CastTransportHostFilter::NotifyStatusChange(
    int32 channel_id,
    media::cast::CastTransportStatus status) {
  Send(new CastMsg_NotifyStatusChange(channel_id, status));
}

void CastTransportHostFilter::SendRawEvents(
    int32 channel_id,
    const std::vector<media::cast::PacketEvent>& packet_events,
    const std::vector<media::cast::FrameEvent>& frame_events) {
  if (!packet_events.empty())
    Send(new CastMsg_RawEvents(channel_id,
                               packet_events,
                               frame_events));
}

void CastTransportHostFilter::SendRtt(int32 channel_id,
                                      uint32 ssrc,
                                      base::TimeDelta rtt,
                                      base::TimeDelta avg_rtt,
                                      base::TimeDelta min_rtt,
                                      base::TimeDelta max_rtt) {
  media::cast::RtcpRttReport report;
  report.rtt = rtt;
  report.avg_rtt = avg_rtt;
  report.min_rtt = min_rtt;
  report.max_rtt = max_rtt;
  Send(new CastMsg_Rtt(channel_id, ssrc, report));
}

void CastTransportHostFilter::SendCastMessage(
    int32 channel_id,
    uint32 ssrc,
    const media::cast::RtcpCastMessage& cast_message) {
  Send(new CastMsg_RtcpCastMessage(channel_id, ssrc, cast_message));
}

void CastTransportHostFilter::OnNew(
    int32 channel_id,
    const net::IPEndPoint& remote_end_point) {
  if (id_map_.Lookup(channel_id)) {
    id_map_.Remove(channel_id);
  }

  scoped_ptr<media::cast::CastTransportSender> sender =
      media::cast::CastTransportSender::Create(
          g_browser_process->net_log(),
          &clock_,
          remote_end_point,
          base::Bind(&CastTransportHostFilter::NotifyStatusChange,
                     weak_factory_.GetWeakPtr(),
                     channel_id),
          base::Bind(&CastTransportHostFilter::SendRawEvents,
                     weak_factory_.GetWeakPtr(),
                     channel_id),
          base::TimeDelta::FromSeconds(kSendRawEventsIntervalSecs),
          base::MessageLoopProxy::current());
  id_map_.AddWithID(sender.release(), channel_id);
}

void CastTransportHostFilter::OnDelete(int32 channel_id) {
  media::cast::CastTransportSender* sender =
      id_map_.Lookup(channel_id);
  if (sender) {
    id_map_.Remove(channel_id);
  } else {
    DVLOG(1) << "CastTransportHostFilter::Delete called "
             << "on non-existing channel";
  }
}

void CastTransportHostFilter::OnInitializeAudio(
    int32 channel_id,
    const media::cast::CastTransportRtpConfig& config) {
  media::cast::CastTransportSender* sender =
      id_map_.Lookup(channel_id);
  if (sender) {
    sender->InitializeAudio(
        config,
        base::Bind(&CastTransportHostFilter::SendCastMessage,
                   weak_factory_.GetWeakPtr(),
                   channel_id, config.ssrc),
        base::Bind(&CastTransportHostFilter::SendRtt,
                   weak_factory_.GetWeakPtr(),
                   channel_id, config.ssrc));
  } else {
    DVLOG(1)
        << "CastTransportHostFilter::OnInitializeAudio on non-existing channel";
  }
}

void CastTransportHostFilter::OnInitializeVideo(
    int32 channel_id,
    const media::cast::CastTransportRtpConfig& config) {
  media::cast::CastTransportSender* sender =
      id_map_.Lookup(channel_id);
  if (sender) {
    sender->InitializeVideo(
        config,
        base::Bind(&CastTransportHostFilter::SendCastMessage,
                   weak_factory_.GetWeakPtr(),
                   channel_id, config.ssrc),
        base::Bind(&CastTransportHostFilter::SendRtt,
                   weak_factory_.GetWeakPtr(),
                   channel_id, config.ssrc));
  } else {
    DVLOG(1)
        << "CastTransportHostFilter::OnInitializeVideo on non-existing channel";
  }
}

void CastTransportHostFilter::OnInsertCodedAudioFrame(
    int32 channel_id,
    const media::cast::EncodedFrame& audio_frame) {
  media::cast::CastTransportSender* sender =
      id_map_.Lookup(channel_id);
  if (sender) {
    sender->InsertCodedAudioFrame(audio_frame);
  } else {
    DVLOG(1)
        << "CastTransportHostFilter::OnInsertCodedAudioFrame "
        << "on non-existing channel";
  }
}

void CastTransportHostFilter::OnInsertCodedVideoFrame(
    int32 channel_id,
    const media::cast::EncodedFrame& video_frame) {
  media::cast::CastTransportSender* sender =
      id_map_.Lookup(channel_id);
  if (sender) {
    sender->InsertCodedVideoFrame(video_frame);
  } else {
    DVLOG(1)
        << "CastTransportHostFilter::OnInsertCodedVideoFrame "
        << "on non-existing channel";
  }
}

void CastTransportHostFilter::OnSendSenderReport(
    int32 channel_id,
    uint32 ssrc,
    base::TimeTicks current_time,
    uint32 current_time_as_rtp_timestamp) {
  media::cast::CastTransportSender* sender =
      id_map_.Lookup(channel_id);
  if (sender) {
    sender->SendSenderReport(ssrc,
                             current_time,
                             current_time_as_rtp_timestamp);
  } else {
    DVLOG(1)
        << "CastTransportHostFilter::OnSendSenderReport "
        << "on non-existing channel";
  }
}

void CastTransportHostFilter::OnResendPackets(
    int32 channel_id,
    bool is_audio,
    const media::cast::MissingFramesAndPacketsMap& missing_packets,
    bool cancel_rtx_if_not_in_list,
    base::TimeDelta dedupe_window) {
  media::cast::CastTransportSender* sender =
      id_map_.Lookup(channel_id);
  if (sender) {
    sender->ResendPackets(
        is_audio, missing_packets, cancel_rtx_if_not_in_list, dedupe_window);
  } else {
    DVLOG(1)
        << "CastTransportHostFilter::OnResendPackets on non-existing channel";
  }
}

}  // namespace cast
