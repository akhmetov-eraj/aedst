#include "Data/connect_admin_repository.hpp"

#include "aedst/contract/methods.hpp"
#include "aedst/network/connect_client.hpp"

#include <utility>

namespace aedst::feature::admin {

ConnectAdminRepository::ConnectAdminRepository(std::shared_ptr<network::ConnectClient> client)
    : client_(std::move(client)) {}

void ConnectAdminRepository::overview(network::ApiCompletion<adminv1::GetOverviewResponse> done) {
    client_->unary(contract::kAdminGetOverview, adminv1::GetOverviewRequest{}, std::move(done));
}

void ConnectAdminRepository::listUsers(const adminv1::ListUsersRequest& request,
                                       network::ApiCompletion<adminv1::ListUsersResponse> done) {
    client_->unary(contract::kAdminListUsers, request, std::move(done));
}

void ConnectAdminRepository::listAuditLog(const adminv1::ListAuditLogRequest& request,
                                          network::ApiCompletion<adminv1::ListAuditLogResponse> done) {
    client_->unary(contract::kAdminListAuditLog, request, std::move(done));
}

void ConnectAdminRepository::broadcast(const adminv1::BroadcastNotificationRequest& request,
                                       network::ApiCompletion<adminv1::BroadcastNotificationResponse> done) {
    client_->unary(contract::kAdminBroadcastNotification, request, std::move(done));
}

} // namespace aedst::feature::admin
