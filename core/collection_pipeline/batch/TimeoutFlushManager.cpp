// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "collection_pipeline/batch/TimeoutFlushManager.h"

using namespace std;

namespace logtail {

void TimeoutFlushManager::UpdateRecord(
    const string& config, size_t index, size_t key, uint32_t timeoutSecs, Flusher* f) {
    lock_guard<recursive_mutex> lock(mMux);
    auto& item = mTimeoutRecords[config];
    auto it = item.find({index, key});
    if (it == item.end()) {
        item.try_emplace({index, key}, f, key, timeoutSecs);
    } else {
        it->second.Update();
    }
}

void TimeoutFlushManager::FlushTimeoutBatch() {
    lock_guard<recursive_mutex> lock(mMux);
    vector<pair<Flusher*, size_t>> records;
    for (auto& item : mTimeoutRecords) {
        for (auto it = item.second.begin(); it != item.second.end();) {
            if (time(nullptr) - it->second.mUpdateTime >= it->second.mTimeoutSecs) {
                // cannot flush here, since flush may also update record, which might invalidate map iterator
                records.emplace_back(it->second.mFlusher, it->second.mKey);
                it = item.second.erase(it);
            } else {
                ++it;
            }
        }
    }
    for (auto& item : records) {
        item.first->Flush(item.second);
    }
}

void TimeoutFlushManager::ClearRecords(const string& config) {
    lock_guard<recursive_mutex> lock(mMux);
    mTimeoutRecords.erase(config);
}

} // namespace logtail
