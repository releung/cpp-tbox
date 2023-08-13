/*
 *     .============.
 *    //  M A K E  / \
 *   //  C++ DEV  /   \
 *  //  E A S Y  /  \/ \
 * ++ ----------.  \/\  .
 *  \\     \     \ /\  /
 *   \\     \     \   /
 *    \\     \     \ /
 *     -============'
 *
 * Copyright (c) 2023 Hevake and contributors, all rights reserved.
 *
 * This file is part of cpp-tbox (https://github.com/cpp-main/cpp-tbox)
 * Use of this source code is governed by MIT license that can be found
 * in the LICENSE file in the root of the source tree. All contributing
 * project authors may be found in the CONTRIBUTORS.md file in the root
 * of the source tree.
 */
#include "rpc.h"

#include <tbox/base/log.h>
#include <tbox/base/json.hpp>
#include "proto.h"
#include "inner_types.h"

namespace tbox {
namespace jsonrpc {

Rpc::Rpc(event::Loop *loop)
    : loop_(loop)
    , request_timeout_(loop)
    , respond_timeout_(loop)
{
    request_timeout_.initialize(std::chrono::seconds(1), 30);
    respond_timeout_.initialize(std::chrono::seconds(1), 30);

    using namespace std::placeholders;
    request_timeout_.setCallback(std::bind(&Rpc::onRequestTimeout, this, _1));
    respond_timeout_.setCallback(std::bind(&Rpc::onRespondTimeout, this, _1));
}

Rpc::~Rpc()
{
    respond_timeout_.cleanup();
    request_timeout_.cleanup();
}

bool Rpc::initialize(Proto *proto)
{
    using namespace std::placeholders;

    proto->setRecvCallback(
        std::bind(&Rpc::onRecvRequest, this, _1, _2, _3),
        std::bind(&Rpc::onRecvRespond, this, _1, _2, _3)
    );
    proto_ = proto;

    return true;
}

void Rpc::cleanup()
{
    method_services_.clear();
    proto_->setRecvCallback(nullptr, nullptr);
    proto_ = nullptr;
}

void Rpc::request(const std::string &method, const Json &js_params, RequestCallback &&cb)
{
    if (cb) {
        int id = ++id_alloc_;
        proto_->sendRequest(id, method, js_params);
        request_callback_[id] = std::move(cb);
        request_timeout_.add(id);
    } else {
        proto_->sendRequest(0, method, js_params);
    }
}

void Rpc::registeService(const std::string &method, ServiceCallback &&cb)
{
    method_services_["method"] = std::move(cb);
}

void Rpc::respond(int id, int errcode, const Json &js_result)
{
    if (id == 0) {
        LogWarn("send id == 0 respond");
        return;
    }

    if (errcode == 0) {
        proto_->sendResult(id, js_result);
    } else {
        proto_->sendError(id, errcode);
    }

    tobe_respond_.erase(id);
}

void Rpc::onRecvRequest(int id, const std::string &method, const Json &js_params)
{
    auto iter = method_services_.find(method);
    if (iter != method_services_.end() && iter->second) {
        int errcode = 0;
        Json js_result;
        if (id != 0) {
            tobe_respond_.insert(id);
            if (iter->second(id, js_params, errcode, js_result)) {
                respond(id, errcode, js_result);
            } else {
                respond_timeout_.add(id);
            }
        } else {
            iter->second(id, js_params, errcode, js_result);
        }
    } else {
        proto_->sendError(id, ErrorCode::kMethodNotFound);
    }
}

void Rpc::onRecvRespond(int id, int errcode, const Json &js_result)
{
    auto iter = request_callback_.find(id);
    if (iter != request_callback_.end()) {
        if (iter->second)
            iter->second(errcode, js_result);
        request_callback_.erase(iter);
    }
}

void Rpc::onRequestTimeout(int id)
{
    auto iter = request_callback_.find(id);
    if (iter != request_callback_.end()) {
        if (iter->second)
            iter->second(ErrorCode::kRequestTimeout, Json());
        request_callback_.erase(iter);
    }
}

void Rpc::onRespondTimeout(int id)
{
    auto iter = tobe_respond_.find(id);
    if (iter != tobe_respond_.end()) {
        LogWarn("respond timeout"); //! 仅仅是提示作用
        tobe_respond_.erase(iter);
    }
}

}
}
