#include "FlusherRemoteWrite.h"

#include "compression/CompressorFactory.h"
#include "prometheus/Constants.h"
#include "queue/SenderQueueManager.h"
#include "sdk/Common.h"
#include "sender/Sender.h"
#include "serializer/RemoteWriteSerializer.h"

using namespace std;

namespace logtail {

const string FlusherRemoteWrite::sName = "flusher_remote_write";

FlusherRemoteWrite::FlusherRemoteWrite() {
    LOG_INFO(sLogger, ("LOG_INFO flusher", ""));
}

bool FlusherRemoteWrite::Init(const Json::Value& config, Json::Value&) {
    string errorMsg;
    LOG_INFO(sLogger, ("LOG_INFO flusher config", config.toStyledString()));

    // config
    // {
    //   "Type": "flusher_remote_write",
    //   "Endpoint": "cn-hangzhou.arms.aliyuncs.com",
    //   "Scheme": "https",
    //   "UserId": "******",
    //   "ClusterId": "*******",
    //   "Region": "cn-hangzhou"
    // }

    if (!config.isMember(prometheus::ENDPOINT) || !config[prometheus::ENDPOINT].isString()
        || !config.isMember(prometheus::SCHEME) || !config[prometheus::SCHEME].isString()
        || !config.isMember(prometheus::USERID) || !config[prometheus::USERID].isString()
        || !config.isMember(prometheus::CLUSTERID) || !config[prometheus::CLUSTERID].isString()
        || !config.isMember(prometheus::REGION) || !config[prometheus::REGION].isString()) {
        errorMsg = "flusher_remote_write config error, must have Endpoint, Scheme, UserId, ClusterId, Region";
        LOG_ERROR(sLogger, ("LOG_ERROR flusher config", config.toStyledString())("error msg", errorMsg));
        return false;
    }
    mEndpoint = config[prometheus::ENDPOINT].asString();
    mScheme = config[prometheus::SCHEME].asString();
    mUserId = config[prometheus::USERID].asString();
    mClusterId = config[prometheus::CLUSTERID].asString();
    mRegion = config[prometheus::REGION].asString();

    mRemoteWritePath = "/prometheus/" + mUserId + "/" + mClusterId + "/" + mRegion + "/api/v2/write";

    // compressor
    // TODO: use SNAPPY
    mCompressor = CompressorFactory::GetInstance()->Create(config, *mContext, sName, CompressType::LZ4);

    DefaultFlushStrategyOptions strategy{
        static_cast<uint32_t>(1024000), static_cast<uint32_t>(1000), static_cast<uint32_t>(1)};
    if (!mBatcher.Init(Json::Value(), this, strategy, false)) {
        LOG_WARNING(sLogger, ("mBatcher init info:", "init err"));
        return false;
    }

    mGroupSerializer = make_unique<RemoteWriteEventGroupSerializer>(this);

    GenerateQueueKey("remote_write");
    SenderQueueManager::GetInstance()->CreateQueue(
        mQueueKey, vector<shared_ptr<ConcurrencyLimiter>>{make_shared<ConcurrencyLimiter>()}, 1024000);

    LOG_INFO(sLogger, ("init info:", "prometheus remote write init successful !"));

    return true;
}

bool FlusherRemoteWrite::Send(PipelineEventGroup&& g) {
    vector<BatchedEventsList> res;
    mBatcher.Add(std::move(g), res);
    return SerializeAndPush(std::move(res));
}

bool FlusherRemoteWrite::Flush(size_t key) {
    BatchedEventsList res;
    mBatcher.FlushQueue(key, res);
    return SerializeAndPush(std::move(res));
}

bool FlusherRemoteWrite::FlushAll() {
    vector<BatchedEventsList> res;
    mBatcher.FlushAll(res);
    return SerializeAndPush(std::move(res));
}

sdk::AsynRequest* FlusherRemoteWrite::BuildRequest(SenderQueueItem* item) const {
    SendClosure* closure = new SendClosure;
    closure->mDataPtr = item;
    sdk::Response* response = new sdk::PostLogStoreLogsResponse();
    string httpMethod = "POST";
    bool httpsFlag = mScheme == "https";
    int32_t port = httpsFlag ? 443 : 80;
    map<string, string> headers;
    headers.insert(make_pair("Content-Encoding", "snappy"));
    headers.insert(make_pair("Content-Type", "application/x-protobuf"));
    headers.insert(make_pair("User-Agent", "RemoteWrite-v0.0.1"));
    headers.insert(make_pair("X-Prometheus-Remote-Write-Version", "0.1.0"));
    return new sdk::AsynRequest(
        httpMethod, mEndpoint, port, mRemoteWritePath, "", headers, item->mData, 30, "", httpsFlag, closure, response);
}

bool FlusherRemoteWrite::SerializeAndPush(std::vector<BatchedEventsList>&& groupLists) {
    for (auto& groupList : groupLists) {
        SerializeAndPush(std::move(groupList));
    }
    return true;
}

bool FlusherRemoteWrite::SerializeAndPush(BatchedEventsList&& groupList) {
    for (auto& batchedEv : groupList) {
        string data;
        string compressedData;
        string errMsg;
        mGroupSerializer->Serialize(std::move(batchedEv), data, errMsg);
        if (!errMsg.empty()) {
            LOG_ERROR(sLogger, ("LOG_ERROR serialize event group", errMsg));
            continue;
        }

        size_t packageSize = 0;
        packageSize += data.size();
        if (mCompressor) {
            if (!mCompressor->Compress(data, compressedData, errMsg)) {
                LOG_WARNING(mContext->GetLogger(),
                            ("failed to compress arms metrics event group",
                             errMsg)("action", "discard data")("plugin", sName)("config", mContext->GetConfigName()));
                return false;
            }
        } else {
            compressedData = data;
        }
        PushToQueue(mQueueKey,
                    make_unique<SenderQueueItem>(
                        std::move(compressedData), packageSize, this, mQueueKey, RawDataType::EVENT_GROUP, false),
                    1);
    }
    return true;
}

bool FlusherRemoteWrite::PushToQueue(QueueKey key, unique_ptr<SenderQueueItem>&& item, uint32_t) {
    SenderQueueManager::GetInstance()->PushQueue(key, std::move(item));
    return true;
}

} // namespace logtail
