#pragma once

#include "aedst/network/api_error.hpp"

#include <aedst/admin/v1/admin.pb.h>

#include <memory>

namespace aedst::network {
class ConnectClient;
} // namespace aedst::network

namespace aedst::feature::admin {

namespace adminv1 = ::aedst::admin::v1;

/// aedst.admin.v1.AdminService for the operator screen. Admin is a pure
/// read/compose surface with no client-side rules, so - like Features/Media - it
/// has no Domain layer: the generated messages are the model. Every RPC needs the
/// "admin" role; the server enforces it (permission_denied otherwise).
class AdminRepository {
public:
    AdminRepository() = default;
    AdminRepository(const AdminRepository&) = delete;
    AdminRepository& operator=(const AdminRepository&) = delete;
    AdminRepository(AdminRepository&&) = delete;
    AdminRepository& operator=(AdminRepository&&) = delete;
    virtual ~AdminRepository() = default;

    virtual void overview(network::ApiCompletion<adminv1::GetOverviewResponse> done) = 0;
    virtual void listUsers(const adminv1::ListUsersRequest& request,
                           network::ApiCompletion<adminv1::ListUsersResponse> done) = 0;
    virtual void listAuditLog(const adminv1::ListAuditLogRequest& request,
                              network::ApiCompletion<adminv1::ListAuditLogResponse> done) = 0;
    virtual void broadcast(const adminv1::BroadcastNotificationRequest& request,
                           network::ApiCompletion<adminv1::BroadcastNotificationResponse> done) = 0;
};

class ConnectAdminRepository final : public AdminRepository {
public:
    explicit ConnectAdminRepository(std::shared_ptr<network::ConnectClient> client);

    void overview(network::ApiCompletion<adminv1::GetOverviewResponse> done) override;
    void listUsers(const adminv1::ListUsersRequest& request,
                   network::ApiCompletion<adminv1::ListUsersResponse> done) override;
    void listAuditLog(const adminv1::ListAuditLogRequest& request,
                      network::ApiCompletion<adminv1::ListAuditLogResponse> done) override;
    void broadcast(const adminv1::BroadcastNotificationRequest& request,
                   network::ApiCompletion<adminv1::BroadcastNotificationResponse> done) override;

private:
    std::shared_ptr<network::ConnectClient> client_;
};

} // namespace aedst::feature::admin
