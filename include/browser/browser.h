#pragma once

#include <functional>
#include <string>

namespace browser::app {

enum class PipelineStage {
  Fetching,
  Parsing,
  Styling,
  Layout,
  Rendering,
};

const char* pipeline_stage_name(PipelineStage stage);

using StageObserver = std::function<void(PipelineStage stage)>;
using CancelCheck = std::function<bool()>;

struct RunOptions {
  int width = 0;
  int height = 0;
  std::string output_path;
  StageObserver on_stage_enter;
  CancelCheck is_cancelled;
};

struct RunResult {
  bool ok = false;
  std::string message;
};

RunResult run(const std::string& url, const RunOptions& options);

}  // namespace browser::app
