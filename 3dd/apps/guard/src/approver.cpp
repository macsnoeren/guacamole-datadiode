#include "../include/approver.h"
#include <iostream>

ApprovalResult Approver::HandleRequest(const std::string &request_id) {
    // PoC operator stand-in: honour the runtime global approve/deny switch.
    if (!approve_.load(std::memory_order_relaxed)) {
        std::cout << "Approver: DENY request " << request_id << std::endl;
        return {false, "operator denied the request"};
    }

    std::cout << "Approver: APPROVE request " << request_id << std::endl;
    return {true, ""};
}
