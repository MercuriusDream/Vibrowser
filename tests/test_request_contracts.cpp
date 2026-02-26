// Test: Request/response lifecycle contracts
// Story 5.1 acceptance test

#include "browser/net/http_client.h"

#include <iostream>
#include <string>
#include <vector>

int main() {
    int failures = 0;

    // Test 1: RequestMethod names
    {
        bool ok = true;
        if (std::string(browser::net::request_method_name(browser::net::RequestMethod::Get)) != "GET") ok = false;
        if (std::string(browser::net::request_method_name(browser::net::RequestMethod::Head)) != "HEAD") ok = false;

        if (!ok) {
            std::cerr << "FAIL: request_method_name incorrect\n";
            ++failures;
        } else {
            std::cerr << "PASS: request_method_name returns correct values\n";
        }
    }

    // Test 2: RequestStage names
    {
        bool ok = true;
        if (std::string(browser::net::request_stage_name(browser::net::RequestStage::Created)) != "Created") ok = false;
        if (std::string(browser::net::request_stage_name(browser::net::RequestStage::Dispatched)) != "Dispatched") ok = false;
        if (std::string(browser::net::request_stage_name(browser::net::RequestStage::Received)) != "Received") ok = false;
        if (std::string(browser::net::request_stage_name(browser::net::RequestStage::Complete)) != "Complete") ok = false;
        if (std::string(browser::net::request_stage_name(browser::net::RequestStage::Error)) != "Error") ok = false;

        if (!ok) {
            std::cerr << "FAIL: request_stage_name incorrect\n";
            ++failures;
        } else {
            std::cerr << "PASS: request_stage_name returns correct values\n";
        }
    }

    // Test 3: RequestTransaction records events
    {
        browser::net::RequestTransaction txn;
        txn.request.method = browser::net::RequestMethod::Get;
        txn.request.url = "http://example.com/test";

        txn.record(browser::net::RequestStage::Created);
        txn.record(browser::net::RequestStage::Dispatched);
        txn.record(browser::net::RequestStage::Received);
        txn.record(browser::net::RequestStage::Complete, "status=200");

        if (txn.events.size() != 4) {
            std::cerr << "FAIL: expected 4 events, got " << txn.events.size() << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: transaction records 4 events\n";
        }

        if (txn.events[0].stage != browser::net::RequestStage::Created) {
            std::cerr << "FAIL: first event not Created\n";
            ++failures;
        } else {
            std::cerr << "PASS: first event is Created\n";
        }

        if (txn.events[3].stage != browser::net::RequestStage::Complete) {
            std::cerr << "FAIL: last event not Complete\n";
            ++failures;
        } else if (txn.events[3].detail != "status=200") {
            std::cerr << "FAIL: Complete event missing detail\n";
            ++failures;
        } else {
            std::cerr << "PASS: last event is Complete with detail\n";
        }
    }

    // Test 4: Event timestamps are ordered
    {
        browser::net::RequestTransaction txn;
        txn.record(browser::net::RequestStage::Created);
        txn.record(browser::net::RequestStage::Dispatched);
        txn.record(browser::net::RequestStage::Received);
        txn.record(browser::net::RequestStage::Complete);

        bool ordered = true;
        for (std::size_t i = 1; i < txn.events.size(); ++i) {
            if (txn.events[i].timestamp < txn.events[i - 1].timestamp) {
                ordered = false;
                break;
            }
        }

        if (!ordered) {
            std::cerr << "FAIL: timestamps not ordered\n";
            ++failures;
        } else {
            std::cerr << "PASS: timestamps are ordered\n";
        }
    }

    // Test 5: Request struct has correct defaults
    {
        browser::net::Request req;
        if (req.method != browser::net::RequestMethod::Get) {
            std::cerr << "FAIL: default method should be Get\n";
            ++failures;
        } else if (!req.url.empty()) {
            std::cerr << "FAIL: default url should be empty\n";
            ++failures;
        } else if (!req.headers.empty()) {
            std::cerr << "FAIL: default headers should be empty\n";
            ++failures;
        } else {
            std::cerr << "PASS: Request has correct defaults\n";
        }
    }

    // Test 6: Error stage records error detail
    {
        browser::net::RequestTransaction txn;
        txn.record(browser::net::RequestStage::Created);
        txn.record(browser::net::RequestStage::Error, "Connection refused");

        if (txn.events.size() != 2) {
            std::cerr << "FAIL: expected 2 events\n";
            ++failures;
        } else if (txn.events[1].detail != "Connection refused") {
            std::cerr << "FAIL: error detail not recorded\n";
            ++failures;
        } else {
            std::cerr << "PASS: error stage records detail\n";
        }
    }

    // Test 7: fetch_with_contract to invalid URL records error lifecycle
    {
        std::vector<browser::net::RequestStage> observed_stages;
        browser::net::FetchOptions options;
        options.timeout_seconds = 2;
        options.observer = [&](const browser::net::RequestTransaction&, browser::net::RequestStage stage) {
            observed_stages.push_back(stage);
        };

        auto txn = browser::net::fetch_with_contract("http://127.0.0.1:1/nonexistent", options);

        // Should have Created, Dispatched, Received (or Error)
        if (txn.events.size() < 3) {
            std::cerr << "FAIL: expected at least 3 events, got " << txn.events.size() << "\n";
            ++failures;
        } else {
            std::cerr << "PASS: fetch_with_contract records at least 3 lifecycle events\n";
        }

        if (txn.events[0].stage != browser::net::RequestStage::Created) {
            std::cerr << "FAIL: first stage not Created\n";
            ++failures;
        } else {
            std::cerr << "PASS: fetch_with_contract starts with Created\n";
        }

        if (txn.events[1].stage != browser::net::RequestStage::Dispatched) {
            std::cerr << "FAIL: second stage not Dispatched\n";
            ++failures;
        } else {
            std::cerr << "PASS: fetch_with_contract dispatches\n";
        }

        // Observer should have been called
        if (observed_stages.size() < 3) {
            std::cerr << "FAIL: observer not called enough times\n";
            ++failures;
        } else {
            std::cerr << "PASS: observer called for each stage\n";
        }

        // Request metadata should be populated
        if (txn.request.url != "http://127.0.0.1:1/nonexistent") {
            std::cerr << "FAIL: request url not populated\n";
            ++failures;
        } else {
            std::cerr << "PASS: request metadata populated\n";
        }
    }

    // Test 8: FetchOptions defaults
    {
        browser::net::FetchOptions opts;
        if (opts.max_redirects != 5) {
            std::cerr << "FAIL: default max_redirects should be 5\n";
            ++failures;
        } else if (opts.timeout_seconds != 10) {
            std::cerr << "FAIL: default timeout_seconds should be 10\n";
            ++failures;
        } else if (opts.observer) {
            std::cerr << "FAIL: default observer should be null\n";
            ++failures;
        } else {
            std::cerr << "PASS: FetchOptions has correct defaults\n";
        }
    }

    // Test 9: fetch_with_policy_contract blocks disallowed cross-origin before dispatch
    {
        browser::net::RequestPolicy policy;
        policy.allow_cross_origin = false;
        policy.origin = "http://example.com";

        browser::net::FetchOptions options;
        options.timeout_seconds = 1;

        auto txn = browser::net::fetch_with_policy_contract("http://other.com/data", policy, options);
        if (txn.response.error.empty()) {
            std::cerr << "FAIL: expected policy block error\n";
            ++failures;
        } else if (txn.events.empty() || txn.events.back().stage != browser::net::RequestStage::Error) {
            std::cerr << "FAIL: expected transaction to end in Error stage\n";
            ++failures;
        } else {
            std::cerr << "PASS: policy-aware fetch blocks disallowed cross-origin request\n";
        }
    }

    // Test 10: fetch_with_policy_contract blocks request when CSP connect-src disallows URL
    {
        browser::net::RequestPolicy policy;
        policy.enforce_connect_src = true;
        policy.connect_src_sources = {"'self'"};
        policy.origin = "https://app.example.com";

        browser::net::FetchOptions options;
        options.timeout_seconds = 1;

        auto txn = browser::net::fetch_with_policy_contract("https://api.example.com/data", policy, options);
        if (txn.response.error.find("CSP connect-src blocked") == std::string::npos) {
            std::cerr << "FAIL: expected CSP connect-src policy block error\n";
            ++failures;
        } else if (txn.events.size() < 2 || txn.events[1].stage != browser::net::RequestStage::Error) {
            std::cerr << "FAIL: expected immediate Error stage for blocked request\n";
            ++failures;
        } else {
            std::cerr << "PASS: policy-aware fetch enforces connect-src before dispatch\n";
        }
    }

    // Test 11: status-line parser captures HTTP/1.x protocol versions and rejects unsupported transports
    {
        std::string http_version;
        int status_code = 0;
        std::string reason;
        std::string err;
        if (!browser::net::parse_http_status_line(
                "HTTP/1.1 200 OK", http_version, status_code, reason, err)) {
            std::cerr << "FAIL: expected valid HTTP/1.1 status line to parse\n";
            ++failures;
        } else if (http_version != "HTTP/1.1" || status_code != 200 || reason != "OK") {
            std::cerr << "FAIL: status-line parse produced unexpected fields\n";
            ++failures;
        } else {
            std::cerr << "PASS: status-line parser captures HTTP/1.x version/status/reason\n";
        }

        if (!browser::net::parse_http_status_line(
                "HTTP/1.0 204 No Content", http_version, status_code, reason, err)) {
            std::cerr << "FAIL: expected valid HTTP/1.0 status line to parse\n";
            ++failures;
        } else if (http_version != "HTTP/1.0" || status_code != 204 || reason != "No Content") {
            std::cerr << "FAIL: HTTP/1.0 status-line parse produced unexpected fields\n";
            ++failures;
        } else {
            std::cerr << "PASS: HTTP/1.0 status line parses as supported HTTP/1.x transport\n";
        }

        if (browser::net::parse_http_status_line(
                "HTTP/2 200 OK", http_version, status_code, reason, err)) {
            std::cerr << "FAIL: HTTP/2 status line should be rejected until h2 transport is implemented\n";
            ++failures;
        } else if (err.find("HTTP/2 status line received") == std::string::npos) {
            std::cerr << "FAIL: expected explicit HTTP/2 status-line rejection message\n";
            ++failures;
        } else {
            std::cerr << "PASS: HTTP/2 status line is rejected with explicit not-implemented message\n";
        }

        if (browser::net::parse_http_status_line(
                "HTTP/2\t200 OK", http_version, status_code, reason, err)) {
            std::cerr << "FAIL: tab-separated HTTP/2 status line should be rejected explicitly\n";
            ++failures;
        } else if (err.find("HTTP/2 status line received") == std::string::npos) {
            std::cerr << "FAIL: expected explicit HTTP/2 status-line rejection for tab-separated variant\n";
            ++failures;
        } else {
            std::cerr << "PASS: tab-separated HTTP/2 status line is rejected explicitly\n";
        }

        if (browser::net::parse_http_status_line(
                "200 OK", http_version, status_code, reason, err)) {
            std::cerr << "FAIL: malformed status line should be rejected\n";
            ++failures;
        } else {
            std::cerr << "PASS: malformed status line is rejected\n";
        }

        if (browser::net::parse_http_status_line(
                "HTTP/1.1 99 Continue", http_version, status_code, reason, err)) {
            std::cerr << "FAIL: expected 2-digit HTTP status code to be rejected\n";
            ++failures;
        } else if (err.find("Invalid HTTP status code") == std::string::npos) {
            std::cerr << "FAIL: expected invalid HTTP status code error for 2-digit status\n";
            ++failures;
        } else {
            std::cerr << "PASS: 2-digit HTTP status code is rejected\n";
        }

        if (browser::net::parse_http_status_line(
                "HTTP/1.1 2000 Too Many Digits", http_version, status_code, reason, err)) {
            std::cerr << "FAIL: expected 4-digit HTTP status code to be rejected\n";
            ++failures;
        } else if (err.find("Invalid HTTP status code") == std::string::npos) {
            std::cerr << "FAIL: expected invalid HTTP status code error for 4-digit status\n";
            ++failures;
        } else {
            std::cerr << "PASS: 4-digit HTTP status code is rejected\n";
        }

        if (browser::net::parse_http_status_line(
                "HTTP/1.1 600 Invalid", http_version, status_code, reason, err)) {
            std::cerr << "FAIL: expected out-of-range HTTP status code to be rejected\n";
            ++failures;
        } else if (err.find("Invalid HTTP status code") == std::string::npos) {
            std::cerr << "FAIL: expected invalid HTTP status code error for out-of-range status\n";
            ++failures;
        } else {
            std::cerr << "PASS: out-of-range HTTP status code is rejected\n";
        }

        if (browser::net::parse_http_status_line(
                std::string("HTTP/1.1 200 OK\x01", 16), http_version, status_code, reason, err)) {
            std::cerr << "FAIL: expected status line with control octet to be rejected\n";
            ++failures;
        } else if (err.find("Malformed HTTP status line") == std::string::npos) {
            std::cerr << "FAIL: expected malformed status-line error for control-octet variant\n";
            ++failures;
        } else {
            std::cerr << "PASS: status line with control octet is rejected\n";
        }

        if (browser::net::parse_http_status_line(
                std::string("HTTP/1.1 200 OK\x80", 16), http_version, status_code, reason, err)) {
            std::cerr << "FAIL: expected status line with non-ASCII octet to be rejected\n";
            ++failures;
        } else if (err.find("Malformed HTTP status line") == std::string::npos) {
            std::cerr << "FAIL: expected malformed status-line error for non-ASCII variant\n";
            ++failures;
        } else {
            std::cerr << "PASS: status line with non-ASCII octet is rejected\n";
        }

        if (browser::net::parse_http_status_line(
                "HTTP/3 200 OK", http_version, status_code, reason, err)) {
            std::cerr << "FAIL: HTTP/3 status line should be rejected as unsupported transport\n";
            ++failures;
        } else if (err.find("Unsupported HTTP status line version 'HTTP/3'") == std::string::npos) {
            std::cerr << "FAIL: expected explicit unsupported HTTP version rejection message\n";
            ++failures;
        } else {
            std::cerr << "PASS: unsupported HTTP/3 status line is rejected with explicit message\n";
        }

        if (browser::net::parse_http_status_line(
                "PRI * HTTP/2.0", http_version, status_code, reason, err)) {
            std::cerr << "FAIL: HTTP/2 preface line should be rejected until h2 transport is implemented\n";
            ++failures;
        } else if (err.find("HTTP/2 response preface received") == std::string::npos) {
            std::cerr << "FAIL: expected explicit HTTP/2 preface rejection message\n";
            ++failures;
        } else {
            std::cerr << "PASS: HTTP/2 preface line is rejected with explicit not-implemented message\n";
        }

        if (browser::net::parse_http_status_line(
                "PRI * HTTP/2.0   ", http_version, status_code, reason, err)) {
            std::cerr << "FAIL: HTTP/2 preface line with trailing whitespace should be rejected\n";
            ++failures;
        } else if (err.find("HTTP/2 response preface received") == std::string::npos) {
            std::cerr << "FAIL: expected explicit HTTP/2 preface rejection for trailing-whitespace variant\n";
            ++failures;
        } else {
            std::cerr << "PASS: HTTP/2 preface trailing-whitespace variant is rejected explicitly\n";
        }

        if (browser::net::parse_http_status_line(
                "PRI * HTTP/2.0\tSM", http_version, status_code, reason, err)) {
            std::cerr << "FAIL: HTTP/2 preface line with tab separator should be rejected\n";
            ++failures;
        } else if (err.find("HTTP/2 response preface received") == std::string::npos) {
            std::cerr << "FAIL: expected explicit HTTP/2 preface rejection for tab-separated variant\n";
            ++failures;
        } else {
            std::cerr << "PASS: HTTP/2 preface tab-separated variant is rejected explicitly\n";
        }
    }

    // Test 12: ALPN helper recognizes negotiated HTTP/2 transport protocol
    {
        if (!browser::net::is_http2_alpn_protocol("h2")) {
            std::cerr << "FAIL: expected h2 ALPN protocol to be treated as HTTP/2\n";
            ++failures;
        } else if (browser::net::is_http2_alpn_protocol("http/1.1")) {
            std::cerr << "FAIL: expected HTTP/1.1 ALPN protocol to not be treated as HTTP/2\n";
            ++failures;
        } else if (browser::net::is_http2_alpn_protocol("h2c")) {
            std::cerr << "FAIL: expected non-ALPN h2c token to not be treated as HTTP/2 ALPN\n";
            ++failures;
        } else {
            std::cerr << "PASS: ALPN HTTP/2 protocol detection works as expected\n";
        }
    }

    // Test 13: Upgrade helper recognizes HTTP/2 upgrade tokens
    {
        if (!browser::net::is_http2_upgrade_protocol("h2c")) {
            std::cerr << "FAIL: expected h2c upgrade token to be treated as HTTP/2 upgrade\n";
            ++failures;
        } else if (!browser::net::is_http2_upgrade_protocol("websocket, h2c")) {
            std::cerr << "FAIL: expected comma-separated upgrade token list to detect h2c\n";
            ++failures;
        } else if (!browser::net::is_http2_upgrade_protocol("\"h2\"")) {
            std::cerr << "FAIL: expected quoted h2 upgrade token to be treated as HTTP/2 upgrade\n";
            ++failures;
        } else if (!browser::net::is_http2_upgrade_protocol("'h2'")) {
            std::cerr << "FAIL: expected single-quoted h2 upgrade token to be treated as HTTP/2 upgrade\n";
            ++failures;
        } else if (!browser::net::is_http2_upgrade_protocol("\"\\\"h2\\\"\"")) {
            std::cerr << "FAIL: expected escaped quoted-string h2 upgrade token to be normalized and detected\n";
            ++failures;
        } else if (!browser::net::is_http2_upgrade_protocol("h2(comment)")) {
            std::cerr << "FAIL: expected h2 token with trailing comment to be treated as HTTP/2 upgrade\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_protocol("\"websocket,h2c\"")) {
            std::cerr << "FAIL: expected quoted comma-containing token to not be split as multiple upgrade tokens\n";
            ++failures;
        } else if (!browser::net::is_http2_upgrade_protocol("H2")) {
            std::cerr << "FAIL: expected case-insensitive h2 token detection\n";
            ++failures;
        } else if (!browser::net::is_http2_upgrade_request({{"\tUpgrade ", "h2"}})) {
            std::cerr << "FAIL: expected whitespace-padded Upgrade header name to be normalized and detected\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_protocol("websocket(comment, h2, note)")) {
            std::cerr << "FAIL: expected commas inside parenthesized comment text to not create synthetic h2 tokens\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_protocol("websocket\\,h2c")) {
            std::cerr << "FAIL: expected escaped comma to not split into synthetic h2 upgrade tokens\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_protocol("h2(comment")) {
            std::cerr << "FAIL: expected unterminated comment token to be rejected as malformed\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_protocol("websocket), h2")) {
            std::cerr << "FAIL: expected stray closing comment delimiter to be rejected as malformed\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_protocol(std::string("h2\x01", 3))) {
            std::cerr << "FAIL: expected control-character malformed token to be rejected\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_protocol(std::string("h2\x80", 3))) {
            std::cerr << "FAIL: expected non-ASCII malformed token to be rejected\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_protocol("h\\2")) {
            std::cerr << "FAIL: expected malformed bare backslash escape token to be rejected\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_protocol("h2;foo=\"bar")) {
            std::cerr << "FAIL: expected unterminated quoted parameter token to be rejected\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_protocol("websocket@, h2")) {
            std::cerr << "FAIL: expected invalid token character in Upgrade list to fail closed before h2 detection\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_protocol("websocket")) {
            std::cerr << "FAIL: expected non-h2 upgrade token to not be treated as HTTP/2\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_protocol("h2c-14")) {
            std::cerr << "FAIL: expected exact token matching for h2/h2c only\n";
            ++failures;
        } else {
            std::cerr << "PASS: HTTP/2 upgrade token detection works as expected\n";
        }
    }

    // Test 14: HTTP/2 upgrade response helper recognizes 101 and 426 upgrade-required responses
    {
        if (!browser::net::is_http2_upgrade_response(101, "h2")) {
            std::cerr << "FAIL: expected 101 + h2 upgrade token to be treated as HTTP/2 upgrade response\n";
            ++failures;
        } else if (!browser::net::is_http2_upgrade_response(426, "websocket, h2c")) {
            std::cerr << "FAIL: expected 426 + h2c token to be treated as HTTP/2 upgrade-required response\n";
            ++failures;
        } else if (!browser::net::is_http2_upgrade_response(101, "\"h2\"")) {
            std::cerr << "FAIL: expected quoted h2 token to be treated as HTTP/2 upgrade response\n";
            ++failures;
        } else if (!browser::net::is_http2_upgrade_response(426, "'h2c'")) {
            std::cerr << "FAIL: expected single-quoted h2c token to be treated as HTTP/2 upgrade response\n";
            ++failures;
        } else if (!browser::net::is_http2_upgrade_response(101, "\"\\\"h2\\\"\"")) {
            std::cerr << "FAIL: expected escaped quoted-string h2 token to be normalized in response detection\n";
            ++failures;
        } else if (!browser::net::is_http2_upgrade_response(101, "h2(comment)")) {
            std::cerr << "FAIL: expected h2 token with trailing comment to be treated as HTTP/2 upgrade response\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_response(101, "\"websocket,h2\"")) {
            std::cerr << "FAIL: expected quoted comma-containing upgrade token to not be split in response detection\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_response(426, "websocket(comment, h2, note)")) {
            std::cerr << "FAIL: expected commas inside parenthesized comment text to not create synthetic response h2 matches\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_response(101, "websocket\\,h2")) {
            std::cerr << "FAIL: expected escaped comma to not create synthetic response h2 upgrade matches\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_response(101, "h2(comment")) {
            std::cerr << "FAIL: expected unterminated comment token to be rejected in response detection\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_response(426, "websocket), h2c")) {
            std::cerr << "FAIL: expected stray closing comment delimiter to be rejected in response detection\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_response(101, std::string("h2\x01", 3))) {
            std::cerr << "FAIL: expected control-character malformed response token to be rejected\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_response(101, std::string("h2\x80", 3))) {
            std::cerr << "FAIL: expected non-ASCII malformed response token to be rejected\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_response(426, "h\\2c")) {
            std::cerr << "FAIL: expected malformed bare backslash escape response token to be rejected\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_response(101, "h2;foo=\"bar")) {
            std::cerr << "FAIL: expected unterminated quoted parameter response token to be rejected\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_response(426, "websocket@, h2c")) {
            std::cerr << "FAIL: expected invalid token character in response Upgrade list to fail closed before h2c detection\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_response(426, "websocket")) {
            std::cerr << "FAIL: expected 426 without h2/h2c token to not be treated as HTTP/2 upgrade response\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_response(200, "h2")) {
            std::cerr << "FAIL: expected non-upgrade HTTP status to not be treated as HTTP/2 upgrade response\n";
            ++failures;
        } else {
            std::cerr << "PASS: HTTP/2 upgrade response detection works as expected\n";
        }
    }

    // Test 15: HTTP/2 upgrade request helper recognizes outbound Upgrade: h2/h2c headers
    {
        std::map<std::string, std::string> headers;
        headers["Upgrade"] = "h2c";
        if (!browser::net::is_http2_upgrade_request(headers)) {
            std::cerr << "FAIL: expected Upgrade: h2c request header to be treated as HTTP/2 upgrade request\n";
            ++failures;
        } else if (!browser::net::is_http2_upgrade_request({{"Upgrade", "\"h2\""}})) {
            std::cerr << "FAIL: expected quoted Upgrade: h2 request header to be treated as HTTP/2 upgrade request\n";
            ++failures;
        } else if (!browser::net::is_http2_upgrade_request({{"Upgrade", "'h2c'"}})) {
            std::cerr << "FAIL: expected single-quoted Upgrade: h2c request header to be treated as HTTP/2 upgrade request\n";
            ++failures;
        } else if (!browser::net::is_http2_upgrade_request({{"Upgrade", "\"\\\"h2\\\"\""}})) {
            std::cerr << "FAIL: expected escaped quoted-string Upgrade: h2 request header to be normalized and detected\n";
            ++failures;
        } else if (!browser::net::is_http2_upgrade_request({{"Upgrade", "h2(comment)"}})) {
            std::cerr << "FAIL: expected Upgrade: h2(comment) request header to be treated as HTTP/2 upgrade request\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_request({{"Upgrade", "\"websocket,h2\""}})) {
            std::cerr << "FAIL: expected quoted comma-containing Upgrade token to not be split in request detection\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_request({{"upgrade", "websocket"}})) {
            std::cerr << "FAIL: expected non-h2 Upgrade header to not be treated as HTTP/2 upgrade request\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_request({{"upgrade", "websocket(comment, h2, note)"}})) {
            std::cerr << "FAIL: expected commas inside parenthesized comment text to not create synthetic request h2 matches\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_request({{"upgrade", "websocket\\,h2"}})) {
            std::cerr << "FAIL: expected escaped comma to not create synthetic request h2 matches\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_request({{"upgrade", "h2(comment"}})) {
            std::cerr << "FAIL: expected unterminated comment token to be rejected in request detection\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_request({{"upgrade", "websocket), h2"}})) {
            std::cerr << "FAIL: expected stray closing comment delimiter to be rejected in request detection\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_request({{"upgrade", std::string("h2\x01", 3)}})) {
            std::cerr << "FAIL: expected control-character malformed request token to be rejected\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_request({{"upgrade", std::string("h2\x80", 3)}})) {
            std::cerr << "FAIL: expected non-ASCII malformed request token to be rejected\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_request({{"upgrade", "h\\2"}})) {
            std::cerr << "FAIL: expected malformed bare backslash escape request token to be rejected\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_request({{"upgrade", "h2;foo=\"bar"}})) {
            std::cerr << "FAIL: expected unterminated quoted parameter request token to be rejected\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_request({{"upgrade", "websocket@, h2"}})) {
            std::cerr << "FAIL: expected invalid token character in request Upgrade list to fail closed before h2 detection\n";
            ++failures;
        } else if (browser::net::is_http2_upgrade_request({{"X-Custom", "h2"}})) {
            std::cerr << "FAIL: expected non-Upgrade header to not be treated as HTTP/2 upgrade request\n";
            ++failures;
        } else {
            std::cerr << "PASS: HTTP/2 upgrade request detection works as expected\n";
        }
    }

    // Test 16: HTTP2-Settings request header helper recognizes outbound h2c settings signal
    {
        std::map<std::string, std::string> headers;
        headers["HTTP2-Settings"] = "AAMAAABkAARAAAAAAAIAAAAA";
        if (!browser::net::is_http2_settings_request(headers)) {
            std::cerr << "FAIL: expected HTTP2-Settings header to be treated as HTTP/2 transport request signal\n";
            ++failures;
        } else {
            headers.clear();
            headers["http2-settings"] = "AAMAAABkAARAAAAAAAIAAAAA";
            if (!browser::net::is_http2_settings_request(headers)) {
                std::cerr << "FAIL: expected case-insensitive HTTP2-Settings detection\n";
                ++failures;
            } else if (!browser::net::is_http2_settings_request({{" HTTP2-Settings\t", "AAMAAABkAARAAAAAAAIAAAAA"}})) {
                std::cerr << "FAIL: expected whitespace-padded HTTP2-Settings name to be normalized and detected\n";
                ++failures;
            } else if (browser::net::is_http2_settings_request({{"HTTP2-Settings", ""}})) {
                std::cerr << "FAIL: expected empty HTTP2-Settings value to be rejected\n";
                ++failures;
            } else if (browser::net::is_http2_settings_request({{"HTTP2-Settings", "AAMA AABk"}})) {
                std::cerr << "FAIL: expected whitespace in HTTP2-Settings token68 value to be rejected\n";
                ++failures;
            } else if (browser::net::is_http2_settings_request({{"HTTP2-Settings", "AAMAAABk,token"}})) {
                std::cerr << "FAIL: expected comma-separated HTTP2-Settings value to be rejected\n";
                ++failures;
            } else if (browser::net::is_http2_settings_request({{"HTTP2-Settings", "==AA"}})) {
                std::cerr << "FAIL: expected leading padding in HTTP2-Settings value to be rejected\n";
                ++failures;
            } else if (browser::net::is_http2_settings_request({{"HTTP2-Settings", "AAMAAABk==="}})) {
                std::cerr << "FAIL: expected over-padded HTTP2-Settings token68 value to be rejected\n";
                ++failures;
            } else if (browser::net::is_http2_settings_request({{"HTTP2-Settings", "A"}})) {
                std::cerr << "FAIL: expected token68 length modulo 4 == 1 to be rejected\n";
                ++failures;
            } else if (browser::net::is_http2_settings_request({{"HTTP2-Settings", "AA="}})) {
                std::cerr << "FAIL: expected invalid single-padding token68 shape to be rejected\n";
                ++failures;
            } else if (browser::net::is_http2_settings_request({{"HTTP2-Settings", "AAA=="}})) {
                std::cerr << "FAIL: expected invalid double-padding token68 shape to be rejected\n";
                ++failures;
            } else if (browser::net::is_http2_settings_request({{"HTTP2-Settings", "AAA="}})) {
                std::cerr << "FAIL: expected non-SETTINGS-frame-length HTTP2-Settings payload (2 bytes) to be rejected\n";
                ++failures;
            } else if (browser::net::is_http2_settings_request({{"HTTP2-Settings", "AA=="}})) {
                std::cerr << "FAIL: expected non-SETTINGS-frame-length HTTP2-Settings payload (1 byte) to be rejected\n";
                ++failures;
            } else if (browser::net::is_http2_settings_request({{"HTTP2-Settings", "AAMAAABkAARAAAAAAAIAAA=="}})) {
                std::cerr << "FAIL: expected padded HTTP2-Settings payload with non-multiple-of-6 decoded length to be rejected\n";
                ++failures;
            } else if (!browser::net::is_http2_settings_request({{"HTTP2-Settings", "AAMAAABk"}})) {
                std::cerr << "FAIL: expected valid 6-byte SETTINGS payload token68 to be accepted\n";
                ++failures;
            } else if (browser::net::is_http2_settings_request({{"HTTP2-Settings", "AAMAA+Bk"}})) {
                std::cerr << "FAIL: expected non-base64url '+' in HTTP2-Settings value to be rejected\n";
                ++failures;
            } else if (browser::net::is_http2_settings_request({{"HTTP2-Settings", "AAMAA/Bk"}})) {
                std::cerr << "FAIL: expected non-base64url '/' in HTTP2-Settings value to be rejected\n";
                ++failures;
            } else if (browser::net::is_http2_settings_request({{"HTTP2-Settings", "AAMAA.Bk"}})) {
                std::cerr << "FAIL: expected non-base64url '.' in HTTP2-Settings value to be rejected\n";
                ++failures;
            } else if (browser::net::is_http2_settings_request({{"HTTP2-Settings", "AAMAA~Bk"}})) {
                std::cerr << "FAIL: expected non-base64url '~' in HTTP2-Settings value to be rejected\n";
                ++failures;
            } else if (browser::net::is_http2_settings_request({{"HTTP2-Settings", std::string("AA\x01", 3)}})) {
                std::cerr << "FAIL: expected control-character malformed HTTP2-Settings value to be rejected\n";
                ++failures;
            } else if (browser::net::is_http2_settings_request({{"HTTP2-Settings", std::string("AA\x80", 3)}})) {
                std::cerr << "FAIL: expected non-ASCII malformed HTTP2-Settings value to be rejected\n";
                ++failures;
            } else {
                headers.clear();
                headers["X-HTTP2-Settings"] = "token";
                if (browser::net::is_http2_settings_request(headers)) {
                    std::cerr << "FAIL: expected exact HTTP2-Settings header name matching\n";
                    ++failures;
                } else {
                    headers.clear();
                    headers["HTTP2-Settings"] = "AAMAAABk";
                    headers["http2-settings"] = "AAMAAABk";
                    if (browser::net::is_http2_settings_request(headers)) {
                        std::cerr << "FAIL: expected duplicate case-variant HTTP2-Settings headers to fail closed\n";
                        ++failures;
                    } else {
                        std::cerr << "PASS: HTTP2-Settings request header detection works as expected\n";
                    }
                }
            }
        }
    }

    // Test 17: HTTP/2 pseudo-header request helper recognizes outbound h2-only pseudo-headers
    {
        std::map<std::string, std::string> headers;
        headers[":authority"] = "example.com";
        if (!browser::net::is_http2_pseudo_header_request(headers)) {
            std::cerr << "FAIL: expected :authority pseudo-header to be treated as HTTP/2 transport signal\n";
            ++failures;
        } else {
            headers.clear();
            headers["X-Forwarded-For"] = "127.0.0.1";
            if (browser::net::is_http2_pseudo_header_request(headers)) {
                std::cerr << "FAIL: expected non-pseudo header to not be treated as HTTP/2 pseudo-header request\n";
                ++failures;
            } else if (!browser::net::is_http2_pseudo_header_request({{"\t:method ", "GET"}})) {
                std::cerr << "FAIL: expected whitespace-padded pseudo-header name to be normalized and detected\n";
                ++failures;
            } else {
                headers.clear();
                headers["authority"] = "example.com";
                if (browser::net::is_http2_pseudo_header_request(headers)) {
                    std::cerr << "FAIL: expected authority without leading colon to not be treated as pseudo-header\n";
                    ++failures;
                } else {
                    std::cerr << "PASS: HTTP/2 pseudo-header request detection works as expected\n";
                }
            }
        }
    }

    // Test 18: Transfer-Encoding helper matches chunked token exactly
    {
        if (!browser::net::is_chunked_transfer_encoding("chunked")) {
            std::cerr << "FAIL: expected chunked transfer-encoding token to be detected\n";
            ++failures;
        } else if (browser::net::is_chunked_transfer_encoding("gzip, chunked")) {
            std::cerr << "FAIL: expected unsupported transfer-coding chain to be rejected\n";
            ++failures;
        } else if (browser::net::is_chunked_transfer_encoding("GZIP,   CHUNKED  ")) {
            std::cerr << "FAIL: expected case-insensitive unsupported transfer-coding chain to be rejected\n";
            ++failures;
        } else if (browser::net::is_chunked_transfer_encoding("gzip")) {
            std::cerr << "FAIL: expected unsupported non-chunked transfer-coding to be rejected\n";
            ++failures;
        } else if (browser::net::is_chunked_transfer_encoding("notchunked")) {
            std::cerr << "FAIL: expected substring-only match to be rejected for chunked transfer-encoding\n";
            ++failures;
        } else if (browser::net::is_chunked_transfer_encoding("xchunked, gzip")) {
            std::cerr << "FAIL: expected prefixed token to not be treated as chunked transfer-encoding\n";
            ++failures;
        } else if (browser::net::is_chunked_transfer_encoding("chunked,")) {
            std::cerr << "FAIL: expected trailing-comma malformed transfer-encoding to be rejected\n";
            ++failures;
        } else if (browser::net::is_chunked_transfer_encoding(",chunked")) {
            std::cerr << "FAIL: expected leading-comma malformed transfer-encoding to be rejected\n";
            ++failures;
        } else if (browser::net::is_chunked_transfer_encoding("gzip,,chunked")) {
            std::cerr << "FAIL: expected empty-token malformed transfer-encoding to be rejected\n";
            ++failures;
        } else if (browser::net::is_chunked_transfer_encoding("\"chunked\"")) {
            std::cerr << "FAIL: expected quoted chunked token to be rejected\n";
            ++failures;
        } else if (browser::net::is_chunked_transfer_encoding("chunk\\ed")) {
            std::cerr << "FAIL: expected escaped malformed transfer-encoding token to be rejected\n";
            ++failures;
        } else if (browser::net::is_chunked_transfer_encoding(std::string("chunked\x01", 8))) {
            std::cerr << "FAIL: expected control-character transfer-encoding token to be rejected\n";
            ++failures;
        } else if (browser::net::is_chunked_transfer_encoding(std::string("chunked\x80", 8))) {
            std::cerr << "FAIL: expected non-ASCII transfer-encoding token to be rejected\n";
            ++failures;
        } else if (browser::net::is_chunked_transfer_encoding("chu\tnked")) {
            std::cerr << "FAIL: expected tab-corrupted transfer-encoding token to be rejected\n";
            ++failures;
        } else if (browser::net::is_chunked_transfer_encoding("chunked;foo=bar")) {
            std::cerr << "FAIL: expected chunked transfer-encoding with parameters to be rejected\n";
            ++failures;
        } else if (browser::net::is_chunked_transfer_encoding("chunked, gzip")) {
            std::cerr << "FAIL: expected non-final chunked transfer-encoding token to be rejected\n";
            ++failures;
        } else {
            std::cerr << "PASS: chunked transfer-encoding token detection works as expected\n";
        }
    }

    // Test 19: framing helper detects conflicting Transfer-Encoding and Content-Length headers
    {
        if (!browser::net::has_conflicting_message_framing_headers(
                {{"Transfer-Encoding", "chunked"}, {"Content-Length", "5"}})) {
            std::cerr << "FAIL: expected conflicting message framing headers to be detected\n";
            ++failures;
        } else if (!browser::net::has_conflicting_message_framing_headers(
                       {{" transfer-encoding\t", "chunked"}, {"\tcontent-length", "5"}})) {
            std::cerr << "FAIL: expected whitespace-padded framing header names to be normalized and detected\n";
            ++failures;
        } else if (browser::net::has_conflicting_message_framing_headers(
                       {{"Transfer-Encoding", "chunked"}})) {
            std::cerr << "FAIL: expected transfer-encoding-only framing to not be treated as conflicting\n";
            ++failures;
        } else if (browser::net::has_conflicting_message_framing_headers(
                       {{"Content-Length", "5"}})) {
            std::cerr << "FAIL: expected content-length-only framing to not be treated as conflicting\n";
            ++failures;
        } else if (browser::net::has_conflicting_message_framing_headers(
                       {{"X-Transfer-Encoding", "chunked"}, {"X-Content-Length", "5"}})) {
            std::cerr << "FAIL: expected non-exact framing header names to not be treated as conflicting\n";
            ++failures;
        } else {
            std::cerr << "PASS: framing conflict detection works as expected\n";
        }
    }

    // Test 20: Content-Length helper detects ambiguous multi-value framing
    {
        if (!browser::net::has_ambiguous_content_length_header("5,5")) {
            std::cerr << "FAIL: expected repeated Content-Length values to be treated as ambiguous\n";
            ++failures;
        } else if (!browser::net::has_ambiguous_content_length_header("5, 7")) {
            std::cerr << "FAIL: expected conflicting Content-Length values to be treated as ambiguous\n";
            ++failures;
        } else if (!browser::net::has_ambiguous_content_length_header("5,")) {
            std::cerr << "FAIL: expected trailing-comma Content-Length values to be treated as ambiguous\n";
            ++failures;
        } else if (!browser::net::has_ambiguous_content_length_header(",5")) {
            std::cerr << "FAIL: expected leading-comma Content-Length values to be treated as ambiguous\n";
            ++failures;
        } else if (!browser::net::has_ambiguous_content_length_header("5,abc")) {
            std::cerr << "FAIL: expected non-numeric Content-Length list token to be treated as ambiguous\n";
            ++failures;
        } else if (browser::net::has_ambiguous_content_length_header("5")) {
            std::cerr << "FAIL: expected single Content-Length value to not be treated as ambiguous\n";
            ++failures;
        } else {
            std::cerr << "PASS: ambiguous Content-Length detection works as expected\n";
        }
    }

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cerr << "\nAll request contract tests PASSED\n";
    return 0;
}
