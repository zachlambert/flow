#pragma once

#include <vector>
#include <optional>
#include <atomic>
#include <functional>
#include <mutex>
#include <future>
#include "flow/signal.h"

namespace flow {

template <typename Request_, typename Response_>
class ServiceClient {
public:
    typedef Request_ Request;
    typedef Response_ Response;

    ServiceClient(Engine& engine):
        engine(engine),
        in_response_(engine, std::bind(&ServiceClient::callback_response, this, std::placeholders::_1))
    {}

    Response sync_call(const Request& request)
    {
        callback = std::nullopt;
        response = std::promise<Response>();
        out_request_.write(request);
        return response.get_future().get();
    }

    typedef std::function<void(const Response&)> callback_t;
    void async_call(const Request& request, const callback_t& callback)
    {
        this->callback = callback;
        response = std::promise<Response>();
        out_request_.write(request);
    }

    Output<Request>& out_request() { return out_request_; }
    Input<Response>& in_response() { return in_response_; }

private:
    void callback_response(const Response& response)
    {
        this->response.set_value(response);
        if (callback) {
            engine.push_callback(std::bind(&ServiceClient::process, this));
        }
    }
    void process()
    {
        callback.value()(response.get_future().get());
    }

    Engine& engine;
    DirectOutput<Request> out_request_;
    DirectInput<Response> in_response_;

    std::optional<callback_t> callback;
    std::promise<Response> response;
};

template <typename Request_, typename Response_>
class ServiceServer {
public:
    typedef Request_ Request;
    typedef Response_ Response;

    typedef std::function<Response(const Request& request)> callback_t;
    ServiceServer(Engine& engine, const callback_t& callback):
        callback(callback),
        in_request_(engine, std::bind(&ServiceServer::callback_request, this, std::placeholders::_1))
    {}

    Output<Response>& out_response() { return out_response_; };
    Input<Request>& in_request() { return in_request_; }

private:
    void callback_request(const Request& request)
    {
        out_response_.write(callback(request));
    }

    DirectOutput<Response> out_response_;
    CallbackInput<Request> in_request_;

    callback_t callback;
};

template <typename Request, typename Response>
void connect(ServiceClient<Request, Response>& client, ServiceServer<Request, Response>& server)
{
    connect(client.out_request(), server.in_request());
    connect(server.out_response(), client.in_response());
}

} // namespace flow
