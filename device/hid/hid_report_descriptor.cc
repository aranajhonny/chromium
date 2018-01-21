// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_report_descriptor.h"

#include "base/stl_util.h"

namespace device {

namespace {

const int kBitsPerByte = 8;

}  // namespace

HidReportDescriptor::HidReportDescriptor(const uint8_t* bytes, size_t size) {
  size_t header_index = 0;
  HidReportDescriptorItem* item = NULL;
  while (header_index < size) {
    item = new HidReportDescriptorItem(&bytes[header_index], item);
    items_.push_back(linked_ptr<HidReportDescriptorItem>(item));
    header_index += item->GetSize();
  }
}

HidReportDescriptor::~HidReportDescriptor() {}

void HidReportDescriptor::GetDetails(
    std::vector<HidCollectionInfo>* top_level_collections,
    int* max_input_report_size,
    int* max_output_report_size,
    int* max_feature_report_size) {
  DCHECK(top_level_collections);
  DCHECK(max_input_report_size);
  DCHECK(max_output_report_size);
  DCHECK(max_feature_report_size);
  STLClearObject(top_level_collections);

  *max_input_report_size = 0;
  *max_output_report_size = 0;
  *max_feature_report_size = 0;

  // Global tags data:
  HidUsageAndPage::Page current_usage_page = HidUsageAndPage::kPageUndefined;
  int current_report_count = 0;
  int cached_report_count = 0;
  int current_report_size = 0;
  int cached_report_size = 0;
  int current_input_report_size = 0;
  int current_output_report_size = 0;
  int current_feature_report_size = 0;

  // Local tags data:
  uint16_t current_usage = 0;

  for (std::vector<linked_ptr<HidReportDescriptorItem> >::const_iterator
           items_iter = items().begin();
       items_iter != items().end();
       ++items_iter) {
    linked_ptr<HidReportDescriptorItem> current_item = *items_iter;

    switch (current_item->tag()) {
      // Main tags:
      case HidReportDescriptorItem::kTagCollection:
        if (!current_item->parent()) {
          // This is a top-level collection.
          HidCollectionInfo collection;
          collection.usage = HidUsageAndPage(current_usage, current_usage_page);
          top_level_collections->push_back(collection);
        }
        break;
      case HidReportDescriptorItem::kTagInput:
        current_input_report_size += current_report_count * current_report_size;
        break;
      case HidReportDescriptorItem::kTagOutput:
        current_output_report_size +=
            current_report_count * current_report_size;
        break;
      case HidReportDescriptorItem::kTagFeature:
        current_feature_report_size +=
            current_report_count * current_report_size;
        break;

      // Global tags:
      case HidReportDescriptorItem::kTagUsagePage:
        current_usage_page =
            (HidUsageAndPage::Page)current_item->GetShortData();
        break;
      case HidReportDescriptorItem::kTagReportId:
        if (top_level_collections->size() > 0) {
          // Store report ID.
          top_level_collections->back().report_ids.insert(
              current_item->GetShortData());

          // We need to increase report sizes by report ID field length.
          if (current_input_report_size > 0)
            current_input_report_size += kBitsPerByte;
          if (current_output_report_size > 0)
            current_output_report_size += kBitsPerByte;
          if (current_feature_report_size > 0)
            current_feature_report_size += kBitsPerByte;

          // Update max report sizes.
          *max_input_report_size =
              std::max(*max_input_report_size, current_input_report_size);
          *max_output_report_size =
              std::max(*max_output_report_size, current_output_report_size);
          *max_feature_report_size =
              std::max(*max_feature_report_size, current_feature_report_size);

          // Set report sizes to be 1-byte long (report ID field).
          current_input_report_size = 0;
          current_output_report_size = 0;
          current_feature_report_size = 0;
        }
        break;
      case HidReportDescriptorItem::kTagReportCount:
        current_report_count = current_item->GetShortData();
        break;
      case HidReportDescriptorItem::kTagReportSize:
        current_report_size = current_item->GetShortData();
        break;
      case HidReportDescriptorItem::kTagPush:
        // Cache report count and size.
        cached_report_count = current_report_count;
        cached_report_size = current_report_size;
        break;
      case HidReportDescriptorItem::kTagPop:
        // Restore cache.
        current_report_count = cached_report_count;
        current_report_size = cached_report_size;
        // Reset cache.
        cached_report_count = 0;
        cached_report_size = 0;
        break;

      // Local tags:
      case HidReportDescriptorItem::kTagUsage:
        current_usage = current_item->GetShortData();
        break;

      default:
        break;
    }
  }

  if (top_level_collections->size() > 0 &&
      top_level_collections->back().report_ids.size() > 0) {
    // We need to increase report sizes by report ID field length.
    if (current_input_report_size > 0)
      current_input_report_size += kBitsPerByte;
    if (current_output_report_size > 0)
      current_output_report_size += kBitsPerByte;
    if (current_feature_report_size > 0)
      current_feature_report_size += kBitsPerByte;
  }

  // Update max report sizes
  *max_input_report_size =
      std::max(*max_input_report_size, current_input_report_size);
  *max_output_report_size =
      std::max(*max_output_report_size, current_output_report_size);
  *max_feature_report_size =
      std::max(*max_feature_report_size, current_feature_report_size);

  // Convert bits into bytes
  *max_input_report_size /= kBitsPerByte;
  *max_output_report_size /= kBitsPerByte;
  *max_feature_report_size /= kBitsPerByte;
}

}  // namespace device
