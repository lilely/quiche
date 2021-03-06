// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/src/quic/core/qpack/qpack_encoder.h"

#include <string>

#include "net/third_party/quiche/src/quic/core/qpack/qpack_encoder_test_utils.h"
#include "net/third_party/quiche/src/quic/core/qpack/qpack_test_utils.h"
#include "net/third_party/quiche/src/quic/core/qpack/qpack_utils.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_test.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_text_utils.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::StrictMock;
using ::testing::Values;

namespace quic {
namespace test {
namespace {

class QpackEncoderTest : public QuicTest {
 protected:
  QpackEncoderTest() : encoder_(&decoder_stream_error_delegate_) {
    encoder_.set_qpack_stream_sender_delegate(&encoder_stream_sender_delegate_);
    encoder_.SetMaximumBlockedStreams(1);
  }

  ~QpackEncoderTest() override = default;

  std::string Encode(const spdy::SpdyHeaderBlock& header_list) {
    return encoder_.EncodeHeaderList(/* stream_id = */ 1, header_list);
  }

  StrictMock<MockDecoderStreamErrorDelegate> decoder_stream_error_delegate_;
  StrictMock<MockQpackStreamSenderDelegate> encoder_stream_sender_delegate_;
  QpackEncoder encoder_;
};

TEST_F(QpackEncoderTest, Empty) {
  spdy::SpdyHeaderBlock header_list;
  std::string output = Encode(header_list);

  EXPECT_EQ(QuicTextUtils::HexDecode("0000"), output);
}

TEST_F(QpackEncoderTest, EmptyName) {
  spdy::SpdyHeaderBlock header_list;
  header_list[""] = "foo";
  std::string output = Encode(header_list);

  EXPECT_EQ(QuicTextUtils::HexDecode("0000208294e7"), output);
}

TEST_F(QpackEncoderTest, EmptyValue) {
  spdy::SpdyHeaderBlock header_list;
  header_list["foo"] = "";
  std::string output = Encode(header_list);

  EXPECT_EQ(QuicTextUtils::HexDecode("00002a94e700"), output);
}

TEST_F(QpackEncoderTest, EmptyNameAndValue) {
  spdy::SpdyHeaderBlock header_list;
  header_list[""] = "";
  std::string output = Encode(header_list);

  EXPECT_EQ(QuicTextUtils::HexDecode("00002000"), output);
}

TEST_F(QpackEncoderTest, Simple) {
  spdy::SpdyHeaderBlock header_list;
  header_list["foo"] = "bar";
  std::string output = Encode(header_list);

  EXPECT_EQ(QuicTextUtils::HexDecode("00002a94e703626172"), output);
}

TEST_F(QpackEncoderTest, Multiple) {
  spdy::SpdyHeaderBlock header_list;
  header_list["foo"] = "bar";
  // 'Z' would be Huffman encoded to 8 bits, so no Huffman encoding is used.
  header_list["ZZZZZZZ"] = std::string(127, 'Z');
  std::string output = Encode(header_list);

  EXPECT_EQ(
      QuicTextUtils::HexDecode(
          "0000"                // prefix
          "2a94e703626172"      // foo: bar
          "27005a5a5a5a5a5a5a"  // 7 octet long header name, the smallest number
                                // that does not fit on a 3-bit prefix.
          "7f005a5a5a5a5a5a5a"  // 127 octet long header value, the smallest
          "5a5a5a5a5a5a5a5a5a"  // number that does not fit on a 7-bit prefix.
          "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
          "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
          "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
          "5a5a5a5a5a5a5a5a5a"),
      output);
}

TEST_F(QpackEncoderTest, StaticTable) {
  {
    spdy::SpdyHeaderBlock header_list;
    header_list[":method"] = "GET";
    header_list["accept-encoding"] = "gzip, deflate, br";
    header_list["location"] = "";

    std::string output = Encode(header_list);
    EXPECT_EQ(QuicTextUtils::HexDecode("0000d1dfcc"), output);
  }
  {
    spdy::SpdyHeaderBlock header_list;
    header_list[":method"] = "POST";
    header_list["accept-encoding"] = "compress";
    header_list["location"] = "foo";

    std::string output = Encode(header_list);
    EXPECT_EQ(QuicTextUtils::HexDecode("0000d45f108621e9aec2a11f5c8294e7"),
              output);
  }
  {
    spdy::SpdyHeaderBlock header_list;
    header_list[":method"] = "TRACE";
    header_list["accept-encoding"] = "";

    std::string output = Encode(header_list);
    EXPECT_EQ(QuicTextUtils::HexDecode("00005f000554524143455f1000"), output);
  }
}

TEST_F(QpackEncoderTest, DecoderStreamError) {
  EXPECT_CALL(decoder_stream_error_delegate_,
              OnDecoderStreamError(Eq("Encoded integer too large.")));

  QpackEncoder encoder(&decoder_stream_error_delegate_);
  encoder.set_qpack_stream_sender_delegate(&encoder_stream_sender_delegate_);
  encoder.decoder_stream_receiver()->Decode(
      QuicTextUtils::HexDecode("ffffffffffffffffffffff"));
}

TEST_F(QpackEncoderTest, SplitAlongNullCharacter) {
  spdy::SpdyHeaderBlock header_list;
  header_list["foo"] = QuicStringPiece("bar\0bar\0baz", 11);
  std::string output = Encode(header_list);

  EXPECT_EQ(QuicTextUtils::HexDecode("0000"            // prefix
                                     "2a94e703626172"  // foo: bar
                                     "2a94e703626172"  // foo: bar
                                     "2a94e70362617a"  // foo: baz
                                     ),
            output);
}

TEST_F(QpackEncoderTest, ZeroInsertCountIncrement) {
  // Encoder receives insert count increment with forbidden value 0.
  EXPECT_CALL(decoder_stream_error_delegate_,
              OnDecoderStreamError(Eq("Invalid increment value 0.")));
  encoder_.OnInsertCountIncrement(0);
}

TEST_F(QpackEncoderTest, TooLargeInsertCountIncrement) {
  // Encoder receives insert count increment with value that increases Known
  // Received Count to a value (one) which is larger than the number of dynamic
  // table insertions sent (zero).
  EXPECT_CALL(
      decoder_stream_error_delegate_,
      OnDecoderStreamError(Eq("Increment value 1 raises known received count "
                              "to 1 exceeding inserted entry count 0")));
  encoder_.OnInsertCountIncrement(1);
}

TEST_F(QpackEncoderTest, InvalidHeaderAcknowledgement) {
  // Encoder receives header acknowledgement for a stream on which no header
  // block with dynamic table entries was ever sent.
  EXPECT_CALL(
      decoder_stream_error_delegate_,
      OnDecoderStreamError(Eq("Header Acknowledgement received for stream 0 "
                              "with no outstanding header blocks.")));
  encoder_.OnHeaderAcknowledgement(/* stream_id = */ 0);
}

TEST_F(QpackEncoderTest, DynamicTable) {
  encoder_.SetMaximumDynamicTableCapacity(4096);
  encoder_.SetMaximumBlockedStreams(1);

  spdy::SpdyHeaderBlock header_list;
  header_list["foo"] = "bar";
  header_list.AppendValueOrAddHeader("foo",
                                     "baz");  // name matches dynamic entry
  header_list["cookie"] = "baz";              // name matches static entry

  // Insert three entries into the dynamic table.
  EXPECT_CALL(encoder_stream_sender_delegate_,
              WriteStreamData(Eq(QuicTextUtils::HexDecode(
                  "62"             // insert without name reference
                  "94e7"           // Huffman-encoded name "foo"
                  "03626172"))));  // value "bar"
  EXPECT_CALL(encoder_stream_sender_delegate_,
              WriteStreamData(Eq(QuicTextUtils::HexDecode(
                  "80"  // insert with name reference, dynamic index 0
                  "0362617a"))));  // value "baz"
  EXPECT_CALL(encoder_stream_sender_delegate_,
              WriteStreamData(Eq(QuicTextUtils::HexDecode(
                  "c5"             // insert with name reference, static index 5
                  "0362617a"))));  // value "baz"

  EXPECT_EQ(QuicTextUtils::HexDecode(
                "0400"      // prefix
                "828180"),  // dynamic entries with relative index 0, 1, and 2
            Encode(header_list));
}

// There is no room in the dynamic table after inserting the first entry.
TEST_F(QpackEncoderTest, SmallDynamicTable) {
  encoder_.SetMaximumDynamicTableCapacity(QpackEntry::Size("foo", "bar"));
  encoder_.SetMaximumBlockedStreams(1);

  spdy::SpdyHeaderBlock header_list;
  header_list["foo"] = "bar";
  header_list.AppendValueOrAddHeader("foo",
                                     "baz");  // name matches dynamic entry
  header_list["cookie"] = "baz";              // name matches static entry
  header_list["bar"] = "baz";                 // no match

  // Insert one entry into the dynamic table.
  EXPECT_CALL(encoder_stream_sender_delegate_,
              WriteStreamData(Eq(QuicTextUtils::HexDecode(
                  "62"             // insert without name reference
                  "94e7"           // Huffman-encoded name "foo"
                  "03626172"))));  // value "bar"

  EXPECT_EQ(QuicTextUtils::HexDecode("0200"  // prefix
                                     "80"    // dynamic entry 0
                                     "40"  // reference to dynamic entry 0 name
                                     "0362617a"  // with literal value "baz"
                                     "55"  // reference to static entry 5 name
                                     "0362617a"    // with literal value "baz"
                                     "23626172"    // literal name "bar"
                                     "0362617a"),  // with literal value "baz"
            Encode(header_list));
}

TEST_F(QpackEncoderTest, BlockedStream) {
  encoder_.SetMaximumDynamicTableCapacity(4096);
  encoder_.SetMaximumBlockedStreams(1);

  spdy::SpdyHeaderBlock header_list1;
  header_list1["foo"] = "bar";

  // Insert one entry into the dynamic table.
  EXPECT_CALL(encoder_stream_sender_delegate_,
              WriteStreamData(Eq(QuicTextUtils::HexDecode(
                  "62"             // insert without name reference
                  "94e7"           // Huffman-encoded name "foo"
                  "03626172"))));  // value "bar"

  EXPECT_EQ(QuicTextUtils::HexDecode("0200"  // prefix
                                     "80"),  // dynamic entry 0
            encoder_.EncodeHeaderList(/* stream_id = */ 1, header_list1));

  // Stream 1 is blocked.  Stream 2 is not allowed to block.
  spdy::SpdyHeaderBlock header_list2;
  header_list2["foo"] = "bar";  // name and value match dynamic entry
  header_list2.AppendValueOrAddHeader("foo",
                                      "baz");  // name matches dynamic entry
  header_list2["cookie"] = "baz";              // name matches static entry
  header_list2["bar"] = "baz";                 // no match

  EXPECT_EQ(QuicTextUtils::HexDecode("0000"        // prefix
                                     "2a94e7"      // literal name "foo"
                                     "03626172"    // with literal value "bar"
                                     "2a94e7"      // literal name "foo"
                                     "0362617a"    // with literal value "baz"
                                     "55"          // name of static entry 5
                                     "0362617a"    // with literal value "baz"
                                     "23626172"    // literal name "bar"
                                     "0362617a"),  // with literal value "baz"
            encoder_.EncodeHeaderList(/* stream_id = */ 2, header_list2));

  // Peer acknowledges receipt of one dynamic table entry.
  // Stream 1 is no longer blocked.
  encoder_.OnInsertCountIncrement(1);

  // Insert three entries into the dynamic table.
  EXPECT_CALL(encoder_stream_sender_delegate_,
              WriteStreamData(Eq(QuicTextUtils::HexDecode(
                  "80"  // insert with name reference, dynamic index 0
                  "0362617a"))));  // value "baz"
  EXPECT_CALL(encoder_stream_sender_delegate_,
              WriteStreamData(Eq(QuicTextUtils::HexDecode(
                  "c5"             // insert with name reference, static index 5
                  "0362617a"))));  // value "baz"
  EXPECT_CALL(encoder_stream_sender_delegate_,
              WriteStreamData(Eq(QuicTextUtils::HexDecode(
                  "43"                       // insert without name reference
                  "626172"                   // name "bar"
                  "0362617a"))));            // value "baz"
  EXPECT_EQ(QuicTextUtils::HexDecode("0500"  // prefix
                                     "83828180"),  // dynamic entries
            encoder_.EncodeHeaderList(/* stream_id = */ 3, header_list2));

  // Stream 3 is blocked.  Stream 4 is not allowed to block, but it can
  // reference already acknowledged dynamic entry 0.
  EXPECT_EQ(QuicTextUtils::HexDecode("0200"        // prefix
                                     "80"          // dynamic entry 0
                                     "2a94e7"      // literal name "foo"
                                     "0362617a"    // with literal value "baz"
                                     "2c21cfd4c5"  // literal name "cookie"
                                     "0362617a"    // with literal value "baz"
                                     "23626172"    // literal name "bar"
                                     "0362617a"),  // with literal value "baz"
            encoder_.EncodeHeaderList(/* stream_id = */ 4, header_list2));

  // Peer acknowledges receipt of two more dynamic table entries.
  // Stream 3 is still blocked.
  encoder_.OnInsertCountIncrement(2);

  // Stream 5 is not allowed to block, but it can reference already acknowledged
  // dynamic entries 0, 1, and 2.
  EXPECT_EQ(QuicTextUtils::HexDecode("0400"        // prefix
                                     "828180"      // dynamic entries
                                     "23626172"    // literal name "bar"
                                     "0362617a"),  // with literal value "baz"
            encoder_.EncodeHeaderList(/* stream_id = */ 5, header_list2));

  // Peer acknowledges decoding header block on stream 3.
  // Stream 3 is not blocked any longer.
  encoder_.OnHeaderAcknowledgement(3);

  EXPECT_EQ(QuicTextUtils::HexDecode("0500"        // prefix
                                     "83828180"),  // dynamic entries
            encoder_.EncodeHeaderList(/* stream_id = */ 6, header_list2));
}

TEST_F(QpackEncoderTest, Draining) {
  // TODO(b/112770235): Remove when already blocking stream can emit blocking
  // references.
  encoder_.SetMaximumBlockedStreams(2);

  spdy::SpdyHeaderBlock header_list1;
  header_list1["one"] = "foo";
  header_list1["two"] = "foo";
  header_list1["three"] = "foo";
  header_list1["four"] = "foo";
  header_list1["five"] = "foo";
  header_list1["six"] = "foo";
  header_list1["seven"] = "foo";
  header_list1["eight"] = "foo";
  header_list1["nine"] = "foo";
  header_list1["ten"] = "foo";

  // Make just enough room in the dynamic table for the header list plus the
  // first entry duplicated.  This will ensure that the oldest entries are
  // draining.
  uint64_t maximum_dynamic_table_capacity = 0;
  for (const auto& header_field : header_list1) {
    maximum_dynamic_table_capacity +=
        QpackEntry::Size(header_field.first, header_field.second);
  }
  maximum_dynamic_table_capacity += QpackEntry::Size("one", "foo");
  encoder_.SetMaximumDynamicTableCapacity(maximum_dynamic_table_capacity);

  // Insert ten entries into the dynamic table.
  EXPECT_CALL(encoder_stream_sender_delegate_, WriteStreamData(_)).Times(10);

  EXPECT_EQ(
      QuicTextUtils::HexDecode("0b00"                    // prefix
                               "89888786858483828180"),  // dynamic entries
      Encode(header_list1));

  // Entry is identical to oldest one, which is draining.  It will be
  // duplicated and referenced.
  spdy::SpdyHeaderBlock header_list2;
  header_list2["one"] = "foo";

  // Duplicate oldest entry.
  EXPECT_CALL(encoder_stream_sender_delegate_,
              WriteStreamData(Eq(QuicTextUtils::HexDecode("09"))));

  EXPECT_EQ(QuicTextUtils::HexDecode("0c00"  // prefix
                                     "80"),  // most recent dynamic table entry
            Encode(header_list2));

  spdy::SpdyHeaderBlock header_list3;
  // Entry is identical to second oldest one, which is draining.  There is no
  // room to duplicate, it will be encoded with string literals.
  header_list3.AppendValueOrAddHeader("two", "foo");
  // Entry has name identical to second oldest one, which is draining.  There is
  // no room to insert new entry, it will be encoded with string literals.
  header_list3.AppendValueOrAddHeader("two", "bar");

  EXPECT_EQ(QuicTextUtils::HexDecode("0000"        // prefix
                                     "2374776f"    // literal name "two"
                                     "8294e7"      // literal value "foo"
                                     "2374776f"    // literal name "two"
                                     "03626172"),  // literal value "bar"
            Encode(header_list3));
}

}  // namespace
}  // namespace test
}  // namespace quic
