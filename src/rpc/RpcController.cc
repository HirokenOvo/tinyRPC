#include "RpcController.h"

RpcController::RpcController()
{
    failed_ = false;
    errMsg_ = "";
}

void RpcController::Reset()
{
    failed_ = false;
    errMsg_ = "";
}

bool RpcController::Failed() const { return failed_; }

std::string RpcController::ErrorText() const { return errMsg_; }

void RpcController::SetFailed(const std::string &reason)
{
    failed_ = true;
    errMsg_ = reason;
}

// FIXME
void RpcController::StartCancel()
{
}

bool RpcController::IsCanceled() const
{
    return false;
}

void RpcController::NotifyOnCancel(google::protobuf::Closure *callback)
{
}