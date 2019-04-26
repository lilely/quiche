// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_CORE_QPACK_VALUE_SPLITTING_HEADER_LIST_H_
#define QUICHE_QUIC_CORE_QPACK_VALUE_SPLITTING_HEADER_LIST_H_

#include "net/third_party/quiche/src/quic/platform/api/quic_export.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quiche/src/spdy/core/spdy_header_block.h"

namespace quic {

// A wrapper class around SpdyHeaderBlock that splits header values along '\0'
// characters.
class QUIC_EXPORT_PRIVATE ValueSplittingHeaderList {
 public:
  using value_type = spdy::SpdyHeaderBlock::value_type;

  class const_iterator {
   public:
    // |header_list| must outlive this object.
    const_iterator(const spdy::SpdyHeaderBlock* header_list,
                   spdy::SpdyHeaderBlock::const_iterator header_list_iterator);
    const_iterator(const const_iterator&) = default;
    const_iterator& operator=(const const_iterator&) = default;

    bool operator==(const const_iterator& other) const;
    bool operator!=(const const_iterator& other) const;

    const const_iterator& operator++();

    const value_type& operator*() const;
    const value_type* operator->() const;

   private:
    // Find next '\0' character; update |value_end_| and |header_field_|.
    void UpdateHeaderField();

    const spdy::SpdyHeaderBlock* const header_list_;
    spdy::SpdyHeaderBlock::const_iterator header_list_iterator_;
    QuicStringPiece::size_type value_start_;
    QuicStringPiece::size_type value_end_;
    value_type header_field_;
  };

  // |header_list| must outlive this object.
  explicit ValueSplittingHeaderList(const spdy::SpdyHeaderBlock* header_list);
  ValueSplittingHeaderList(const ValueSplittingHeaderList&) = delete;
  ValueSplittingHeaderList& operator=(const ValueSplittingHeaderList&) = delete;

  const_iterator begin() const;
  const_iterator end() const;

 private:
  const spdy::SpdyHeaderBlock* const header_list_;
};

}  // namespace quic

#endif  // QUICHE_QUIC_CORE_QPACK_VALUE_SPLITTING_HEADER_LIST_H_