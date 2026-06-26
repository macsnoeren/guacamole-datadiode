#include "../include/approver.h"
#include <iostream>

ApprovalResult Approver::HandleRequest(const std::string &request_id) {
    // PoC operator stand-in: honour the runtime global approve/deny switch.
    if (!approve_.load(std::memory_order_relaxed)) {
        std::cout << "Approver: DENY request " << request_id << std::endl;
        return {false, "operator denied the request"};
    }

    // request_id is validated (is_valid_request_id) by the guard's receive loop
    // before this is called, so it is safe to log: a hex-only id can carry no
    // terminal escapes or forged log lines even when it arrived over UDP.
    std::cout << "Approver: APPROVE request " << request_id << std::endl;
    return {true, ""};
}
