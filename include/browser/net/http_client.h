#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace browser::net {

struct Response {
  int status_code = 0;
  std::string reason;
  std::string http_version;
  std::map<std::string, std::string> headers;
  std::string body;
  std::string final_url;
  std::string error;
  double total_duration_seconds = 0.0;
  bool timed_out = false;
};

Response fetch(const std::string& url, int max_redirects = 5, int timeout_seconds = 10);

enum class RequestMethod {
    Get,
    Head,
};

const char* request_method_name(RequestMethod method);

struct Request {
    RequestMethod method = RequestMethod::Get;
    std::string url;
    std::map<std::string, std::string> headers;
};

enum class RequestStage {
    Created,
    Dispatched,
    Received,
    Complete,
    Error,
};

const char* request_stage_name(RequestStage stage);

struct RequestEvent {
    RequestStage stage;
    std::chrono::steady_clock::time_point timestamp;
    std::string detail;
};

struct RequestTransaction {
    Request request;
    Response response;
    std::vector<RequestEvent> events;

    void record(RequestStage stage, const std::string& detail = {});
};

using TransactionObserver = std::function<void(const RequestTransaction& txn, RequestStage stage)>;

struct FetchOptions {
    int max_redirects = 5;
    int timeout_seconds = 10;
    TransactionObserver observer;
};

RequestTransaction fetch_with_contract(const std::string& url, const FetchOptions& options = {});

bool parse_http_status_line(const std::string& status_line,
                            std::string& http_version,
                            int& status_code,
                            std::string& reason,
                            std::string& err);

enum class CachePolicy {
    NoCache,
    CacheAll,
};

const char* cache_policy_name(CachePolicy policy);

struct CacheEntry {
    Response response;
    std::chrono::steady_clock::time_point cached_at;
};

class ResponseCache {
public:
    explicit ResponseCache(CachePolicy policy = CachePolicy::NoCache);

    void set_policy(CachePolicy policy);
    CachePolicy policy() const;

    bool lookup(const std::string& url, Response& out) const;
    void store(const std::string& url, const Response& response);
    void clear();
    std::size_t size() const;

private:
    CachePolicy policy_;
    std::map<std::string, CacheEntry> entries_;
};

enum class PolicyViolation {
    None,
    TooManyRedirects,
    CrossOriginBlocked,
    CorsResponseBlocked,
    CspConnectSrcBlocked,
    UnsupportedScheme,
    EmptyUrl,
};

const char* policy_violation_name(PolicyViolation violation);

struct RequestPolicy {
    int max_redirects = 5;
    bool allow_cross_origin = true;
    bool require_acao_for_cross_origin = true;
    bool attach_origin_header_for_cors = true;
    bool credentials_mode_include = false;
    bool require_acac_for_credentialed_cors = true;
    bool enforce_connect_src = false;
    std::vector<std::string> allowed_schemes = {"http", "https", "file"};
    std::vector<std::string> connect_src_sources;
    std::vector<std::string> default_src_sources;
    std::string origin;
};

struct PolicyCheckResult {
    bool allowed = true;
    PolicyViolation violation = PolicyViolation::None;
    std::string message;
};

PolicyCheckResult check_request_policy(const std::string& url, const RequestPolicy& policy);

PolicyCheckResult check_cors_response_policy(const std::string& url,
                                             const Response& response,
                                             const RequestPolicy& policy);

std::map<std::string, std::string> build_request_headers_for_policy(
    const std::string& url,
    const RequestPolicy& policy);

RequestTransaction fetch_with_policy_contract(const std::string& url,
                                              const RequestPolicy& policy,
                                              const FetchOptions& options = {});

}  // namespace browser::net
