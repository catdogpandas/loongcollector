#include <cstdlib>

#include "common/JsonUtil.h"
#include "common/LogstoreFeedbackKey.h"
#include "common/TimeUtil.h"
#include "flusher/FlusherRemoteWrite.h"
#include "plugin/instance/FlusherInstance.h"
#include "sdk/CurlAsynInstance.h"
#include "sender/Sender.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {
class FlusherRemoteWriteTest : public testing::Test {
public:
    void SimpleTest();
    void OnSuccessInit();
    void OnFailedInit();

protected:
    void SetUp() override { ctx.SetConfigName("test_config"); }

private:
    PipelineContext ctx;
};

void FlusherRemoteWriteTest::SimpleTest() {
    FlusherRemoteWrite* flusher = new FlusherRemoteWrite();
    unique_ptr<FlusherInstance> flusherIns = unique_ptr<FlusherInstance>(new FlusherInstance(flusher, "0"));
    Json::Value config, goPipeline;

    // config["Endpoint"] = "cn-hangzhou.arms.aliyuncs.com";
    // config["Scheme"] = "https";
    // config["UserId"] = "123";
    // config["ClusterId"] = "456";
    // config["Region"] = "cn-hangzhou";

    config["Endpoint"] = ToString(getenv("ENDPOINT"));
    config["Scheme"] = ToString(getenv("SCHEME"));
    config["UserId"] = ToString(getenv("USER_ID"));
    config["ClusterId"] = ToString(getenv("CLUSTER_ID"));
    config["Region"] = ToString(getenv("REGION"));

    flusherIns->Init(config, ctx, goPipeline);
    const auto& srcBuf = make_shared<SourceBuffer>();
    auto eGroup = PipelineEventGroup(srcBuf);
    auto event = eGroup.AddMetricEvent();
    event->SetName(string("test_metric"));
    event->SetValue(MetricValue(UntypedSingleValue{1.0}));
    time_t currSeconds = GetCurrentTimeInNanoSeconds() / 1000 / 1000 / 1000;
    event->SetTimestamp(currSeconds);
    event->SetTag(string("test_key_x"), string("test_value_x"));
    event->SetTag(string("__name__"), string("test_metric"));

    flusherIns->Start();
    flusherIns->Send(std::move(eGroup));
    flusherIns->FlushAll();

    APSARA_TEST_EQUAL(1UL, flusher->mItems.size());

    auto item = flusher->mItems.at(0);
    auto req = flusher->BuildRequest(item);
    auto curlIns = sdk::CurlAsynInstance::GetInstance();
    curlIns->AddRequest(req);

    sleep(3);

    auto* closure = req->mCallBack;
    auto statusCode = closure->mHTTPMessage.statusCode;
    APSARA_TEST_EQUAL(404, statusCode);
    delete item;
}

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
    APSARA_TEST_EQUAL(GenerateLogstoreFeedBackKey("remote_write_project", "remote_write_log_store"),
                      flusher->GetLogstoreKey());
    APSARA_TEST_EQUAL("cn-chengdu.arms.aliyuncs.com", flusher->mEndpoint);
    APSARA_TEST_EQUAL(CompressType::SNAPPY, flusher->mCompressor->GetCompressType());
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

UNIT_TEST_CASE(FlusherRemoteWriteTest, SimpleTest)
UNIT_TEST_CASE(FlusherRemoteWriteTest, OnSuccessInit)
UNIT_TEST_CASE(FlusherRemoteWriteTest, OnFailedInit)

} // namespace logtail

UNIT_TEST_MAIN
