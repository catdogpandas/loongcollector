// Copyright 2023 iLogtail Authors
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

#include "plugin/input/InputFileSecurity.h"

// #include "ebpf/security/SecurityServer.h"
#include "ebpf/eBPFServer.h"
#include "ebpf/include/export.h"


using namespace std;

namespace logtail {

const std::string InputFileSecurity::sName = "input_file_security";

bool InputFileSecurity::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    ebpf::eBPFServer::GetInstance()->Init();
    if (!ebpf::eBPFServer::GetInstance()->IsSupportedEnv(nami::PluginType::FILE_SECURITY)) {
        return false;
    }
    std::string prev_pipeline_name
        = ebpf::eBPFServer::GetInstance()->CheckLoadedPipelineName(nami::PluginType::FILE_SECURITY);
    std::string pipeline_name = mContext->GetConfigName();
    if (prev_pipeline_name.size() && prev_pipeline_name != pipeline_name) {
        LOG_WARNING(sLogger,
                    ("pipeline already loaded", "FILE_SECURITY")("prev pipeline", prev_pipeline_name)("curr pipeline",
                                                                                                      pipeline_name));
        return false;
    }
    static const std::unordered_map<std::string, MetricType> metricKeys = {
        {METRIC_PLUGIN_IN_EVENTS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_EBPF_LOSS_KERNEL_EVENTS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_EBPF_PROCESS_CACHE_ENTRIES_NUM, MetricType::METRIC_TYPE_INT_GAUGE},
        {METRIC_PLUGIN_EBPF_PROCESS_CACHE_MISS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
    };

    mPluginMgr = std::make_shared<PluginMetricManager>(
        GetMetricsRecordRef().GetLabels(), metricKeys, MetricCategory::METRIC_CATEGORY_PLUGIN_SOURCE);
    return mSecurityOptions.Init(ebpf::SecurityProbeType::FILE, config, mContext, sName);
}

bool InputFileSecurity::Start() {
    return ebpf::eBPFServer::GetInstance()->EnablePlugin(
        mContext->GetConfigName(), mIndex, nami::PluginType::FILE_SECURITY, mContext, &mSecurityOptions, mPluginMgr);
}

bool InputFileSecurity::Stop(bool isPipelineRemoving) {
    if (!isPipelineRemoving) {
        ebpf::eBPFServer::GetInstance()->SuspendPlugin(mContext->GetConfigName(), nami::PluginType::FILE_SECURITY);
        return true;
    }
    // SecurityServer::GetInstance()->RemoveSecurityOptions(mContext->GetConfigName(), mIndex);
    ebpf::eBPFServer::GetInstance()->DisablePlugin(mContext->GetConfigName(), nami::PluginType::FILE_SECURITY);
    return true;
}

} // namespace logtail
