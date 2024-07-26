#include <cstdlib>

#include "common/JsonUtil.h"
#include "common/TimeUtil.h"
#include "flusher/FlusherRemoteWrite.h"
#include "pipeline/Pipeline.h"
#include "sdk/Closure.h"
#include "sdk/CurlAsynInstance.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {
class FlusherRemoteWriteTest : public testing::Test {
public:
    void OnSuccessInit();
    void OnFailedInit();

protected:
    void SetUp() override { ctx.SetConfigName("test_config"); }

private:
    PipelineContext ctx;
};

void FlusherRemoteWriteTest::OnSuccessInit() {
    unique_ptr<FlusherRemoteWrite> flusher;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;
    configStr = R"(
        {
            "Type": "flusher_remote_write",
            "Endpoint": "cn-chengdu.arms.aliyuncs.com",
            "Scheme": "http",
            "UserId": "123",
            "ClusterId": "456",
            "Region": "cn-chengdu"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherRemoteWrite());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherRemoteWrite::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL("cn-chengdu.arms.aliyuncs.com", flusher->mEndpoint);

    // TODO: use SNAPPY
    APSARA_TEST_EQUAL(CompressType::LZ4, flusher->mCompressor->GetCompressType());
}

void FlusherRemoteWriteTest::OnFailedInit() {
    unique_ptr<FlusherRemoteWrite> flusher;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;
    configStr = R"(
        {
            "Type": "flusher_remote_write",
            "Endpoint": "cn-chengdu.arms.aliyuncs.com",
            "Scheme": true,
            "UserId": "123",
            "ClusterId": "456",
            "Region": "cn-chengdu"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherRemoteWrite());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherRemoteWrite::sName, "1");
    APSARA_TEST_FALSE(flusher->Init(configJson, optionalGoPipeline));
}

UNIT_TEST_CASE(FlusherRemoteWriteTest, OnSuccessInit)
UNIT_TEST_CASE(FlusherRemoteWriteTest, OnFailedInit)

} // namespace logtail

UNIT_TEST_MAIN
