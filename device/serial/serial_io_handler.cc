// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/serial_io_handler.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"

namespace device {

SerialIoHandler::SerialIoHandler(
    scoped_refptr<base::MessageLoopProxy> file_thread_message_loop)
    : pending_read_buffer_len_(0),
      pending_write_buffer_len_(0),
      file_thread_message_loop_(file_thread_message_loop) {
}

SerialIoHandler::~SerialIoHandler() {
  DCHECK(CalledOnValidThread());
  Close();
}

void SerialIoHandler::Initialize(const ReadCompleteCallback& read_callback,
                                 const WriteCompleteCallback& write_callback) {
  DCHECK(CalledOnValidThread());
  read_complete_ = read_callback;
  write_complete_ = write_callback;
}

void SerialIoHandler::Open(const std::string& port,
                           const OpenCompleteCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(open_complete_.is_null());
  open_complete_ = callback;
  DCHECK(file_thread_message_loop_);
  file_thread_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&SerialIoHandler::StartOpen,
                 this,
                 port,
                 base::MessageLoopProxy::current()));
}

void SerialIoHandler::StartOpen(
    const std::string& port,
    scoped_refptr<base::MessageLoopProxy> io_message_loop) {
  DCHECK(!open_complete_.is_null());
  DCHECK(file_thread_message_loop_->RunsTasksOnCurrentThread());
  DCHECK(!file_.IsValid());
  // It's the responsibility of the API wrapper around SerialIoHandler to
  // validate the supplied path against the set of valid port names, and
  // it is a reasonable assumption that serial port names are ASCII.
  DCHECK(base::IsStringASCII(port));
  base::FilePath path(base::FilePath::FromUTF8Unsafe(MaybeFixUpPortName(port)));
  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
              base::File::FLAG_EXCLUSIVE_READ | base::File::FLAG_WRITE |
              base::File::FLAG_EXCLUSIVE_WRITE | base::File::FLAG_ASYNC |
              base::File::FLAG_TERMINAL_DEVICE;
  base::File file(path, flags);
  io_message_loop->PostTask(
      FROM_HERE,
      base::Bind(&SerialIoHandler::FinishOpen, this, Passed(file.Pass())));
}

void SerialIoHandler::FinishOpen(base::File file) {
  DCHECK(CalledOnValidThread());
  DCHECK(!open_complete_.is_null());
  OpenCompleteCallback callback = open_complete_;
  open_complete_.Reset();

  if (!file.IsValid()) {
    callback.Run(false);
    return;
  }

  file_ = file.Pass();

  bool success = PostOpen();
  if (!success)
    Close();
  callback.Run(success);
}

bool SerialIoHandler::PostOpen() {
  return true;
}

void SerialIoHandler::Close() {
  if (file_.IsValid()) {
    DCHECK(file_thread_message_loop_);
    file_thread_message_loop_->PostTask(
        FROM_HERE, base::Bind(&SerialIoHandler::DoClose, Passed(file_.Pass())));
  }
}

// static
void SerialIoHandler::DoClose(base::File port) {
  // port closed by destructor.
}

void SerialIoHandler::Read(int max_bytes) {
  DCHECK(CalledOnValidThread());
  DCHECK(!IsReadPending());
  pending_read_buffer_ = new net::IOBuffer(max_bytes);
  pending_read_buffer_len_ = max_bytes;
  read_canceled_ = false;
  AddRef();
  ReadImpl();
}

void SerialIoHandler::Write(const std::string& data) {
  DCHECK(CalledOnValidThread());
  DCHECK(!IsWritePending());
  int length = static_cast<int>(data.length());
  pending_write_buffer_ = new net::IOBuffer(length);
  pending_write_buffer_len_ = length;
  memcpy(pending_write_buffer_->data(), data.data(), pending_write_buffer_len_);
  write_canceled_ = false;
  AddRef();
  WriteImpl();
}

void SerialIoHandler::ReadCompleted(int bytes_read,
                                    serial::ReceiveError error) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsReadPending());
  read_complete_.Run(std::string(pending_read_buffer_->data(), bytes_read),
                     error);
  pending_read_buffer_ = NULL;
  pending_read_buffer_len_ = 0;
  Release();
}

void SerialIoHandler::WriteCompleted(int bytes_written,
                                     serial::SendError error) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsWritePending());
  write_complete_.Run(bytes_written, error);
  pending_write_buffer_ = NULL;
  pending_write_buffer_len_ = 0;
  Release();
}

bool SerialIoHandler::IsReadPending() const {
  DCHECK(CalledOnValidThread());
  return pending_read_buffer_ != NULL;
}

bool SerialIoHandler::IsWritePending() const {
  DCHECK(CalledOnValidThread());
  return pending_write_buffer_ != NULL;
}

void SerialIoHandler::CancelRead(serial::ReceiveError reason) {
  DCHECK(CalledOnValidThread());
  if (IsReadPending()) {
    read_canceled_ = true;
    read_cancel_reason_ = reason;
    CancelReadImpl();
  }
}

void SerialIoHandler::CancelWrite(serial::SendError reason) {
  DCHECK(CalledOnValidThread());
  if (IsWritePending()) {
    write_canceled_ = true;
    write_cancel_reason_ = reason;
    CancelWriteImpl();
  }
}

void SerialIoHandler::QueueReadCompleted(int bytes_read,
                                         serial::ReceiveError error) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&SerialIoHandler::ReadCompleted, this, bytes_read, error));
}

void SerialIoHandler::QueueWriteCompleted(int bytes_written,
                                          serial::SendError error) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&SerialIoHandler::WriteCompleted, this, bytes_written, error));
}

}  // namespace device
