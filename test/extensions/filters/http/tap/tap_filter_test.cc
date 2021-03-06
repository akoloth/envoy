#include "extensions/filters/http/tap/tap_filter.h"

#include "test/mocks/stream_info/mocks.h"
#include "test/test_common/utility.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::InSequence;
using testing::Return;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace TapFilter {
namespace {

class MockFilterConfig : public FilterConfig {
public:
  MOCK_METHOD0(currentConfig, HttpTapConfigSharedPtr());
  FilterStats& stats() override { return stats_; }

  Stats::IsolatedStoreImpl stats_store_;
  FilterStats stats_{Filter::generateStats("foo", stats_store_)};
};

class MockHttpTapConfig : public HttpTapConfig {
public:
  HttpPerRequestTapperPtr createPerRequestTapper() override {
    return HttpPerRequestTapperPtr{createPerRequestTapper_()};
  }

  MOCK_METHOD0(createPerRequestTapper_, HttpPerRequestTapper*());
};

class MockHttpPerRequestTapper : public HttpPerRequestTapper {
public:
  MOCK_METHOD1(onRequestHeaders, void(const Http::HeaderMap& headers));
  MOCK_METHOD1(onResponseHeaders, void(const Http::HeaderMap& headers));
  MOCK_METHOD2(onDestroyLog, bool(const Http::HeaderMap* request_headers,
                                  const Http::HeaderMap* response_headers));
};

class TapFilterTest : public testing::Test {
public:
  void setup(bool has_config) {
    if (has_config) {
      http_tap_config_ = std::make_shared<MockHttpTapConfig>();
    }

    EXPECT_CALL(*filter_config_, currentConfig()).WillRepeatedly(Return(http_tap_config_));
    if (has_config) {
      http_per_request_tapper_ = new MockHttpPerRequestTapper();
      EXPECT_CALL(*http_tap_config_, createPerRequestTapper_())
          .WillOnce(Return(http_per_request_tapper_));
    }

    filter_ = std::make_unique<Filter>(filter_config_);
  }

  std::shared_ptr<MockFilterConfig> filter_config_{new MockFilterConfig()};
  std::shared_ptr<MockHttpTapConfig> http_tap_config_;
  MockHttpPerRequestTapper* http_per_request_tapper_;
  std::unique_ptr<Filter> filter_;
  StreamInfo::MockStreamInfo stream_info_;
};

// Verify filter functionality when there is no tap config.
TEST_F(TapFilterTest, NoConfig) {
  InSequence s;
  setup(false);

  Http::TestHeaderMapImpl request_headers;
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));
  Buffer::OwnedImpl request_body;
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(request_body, false));
  Http::TestHeaderMapImpl request_trailers;
  EXPECT_EQ(Http::FilterTrailersStatus::Continue, filter_->decodeTrailers(request_trailers));

  Http::TestHeaderMapImpl response_headers;
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->encode100ContinueHeaders(response_headers));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(response_headers, false));
  Buffer::OwnedImpl response_body;
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->encodeData(response_body, false));
  Http::TestHeaderMapImpl response_trailers;
  EXPECT_EQ(Http::FilterTrailersStatus::Continue, filter_->encodeTrailers(response_trailers));
  Http::MetadataMap metadata;
  EXPECT_EQ(Http::FilterMetadataStatus::Continue, filter_->encodeMetadata(metadata));

  filter_->log(&request_headers, &response_headers, &response_trailers, stream_info_);
}

// Verify filter functionality when there is a tap config.
TEST_F(TapFilterTest, Config) {
  InSequence s;
  setup(true);

  Http::TestHeaderMapImpl request_headers;
  EXPECT_CALL(*http_per_request_tapper_, onRequestHeaders(_));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));
  Buffer::OwnedImpl request_body;
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(request_body, false));
  Http::TestHeaderMapImpl request_trailers;
  EXPECT_EQ(Http::FilterTrailersStatus::Continue, filter_->decodeTrailers(request_trailers));

  Http::TestHeaderMapImpl response_headers;
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->encode100ContinueHeaders(response_headers));
  EXPECT_CALL(*http_per_request_tapper_, onResponseHeaders(_));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(response_headers, false));
  Buffer::OwnedImpl response_body;
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->encodeData(response_body, false));
  Http::TestHeaderMapImpl response_trailers;
  EXPECT_EQ(Http::FilterTrailersStatus::Continue, filter_->encodeTrailers(response_trailers));

  EXPECT_CALL(*http_per_request_tapper_, onDestroyLog(&request_headers, &response_headers))
      .WillOnce(Return(true));
  filter_->log(&request_headers, &response_headers, &response_trailers, stream_info_);
  EXPECT_EQ(1UL, filter_config_->stats_.rq_tapped_.value());

  // Workaround InSequence/shared_ptr mock leak.
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(http_tap_config_.get()));
}

} // namespace
} // namespace TapFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
