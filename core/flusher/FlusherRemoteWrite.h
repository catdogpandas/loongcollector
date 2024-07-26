#pragma once

#include "batch/Batcher.h"
#include "compression/Compressor.h"
#include "plugin/interface/Flusher.h"
#include "serializer/Serializer.h"

namespace logtail {

class FlusherRemoteWrite : public Flusher {
public:
    static const std::string sName;

    FlusherRemoteWrite();

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config, Json::Value& optionalGoPipeline) override;
    bool Send(PipelineEventGroup&& g) override;
    bool Flush(size_t key) override;
    bool FlushAll() override;
    sdk::AsynRequest* BuildRequest(SenderQueueItem* item) const override;

private:
    Batcher<> mBatcher;
    std::unique_ptr<EventGroupSerializer> mGroupSerializer;
    std::unique_ptr<Compressor> mCompressor;

    std::string mEndpoint;
    std::string mRemoteWritePath;
    std::string mScheme;
    std::string mUserId;
    std::string mClusterId;
    std::string mRegion;

    bool SerializeAndPush(std::vector<BatchedEventsList>&& groupLists);
    bool SerializeAndPush(BatchedEventsList&& groupList);

    void PushToQueue(std::string&& data, size_t rawSize, RawDataType type);
    bool PushToQueue(QueueKey key, std::unique_ptr<SenderQueueItem>&& item, uint32_t retryTimes = 500);

#ifdef APSARA_UNIT_TEST_MAIN
    friend class FlusherRemoteWriteTest;
#endif
};

} // namespace logtail
