#include "aedst/media/media_remote_data_source.hpp"

#include "aedst/contract/methods.hpp"
#include "aedst/network/connect_client.hpp"

#include <utility>

namespace aedst::media {

ConnectMediaRemoteDataSource::ConnectMediaRemoteDataSource(std::shared_ptr<network::ConnectClient> client)
    : client_(std::move(client)) {}

void ConnectMediaRemoteDataSource::createUpload(const mediav1::CreateUploadRequest& request,
                                                network::ApiCompletion<mediav1::CreateUploadResponse> done) {
    client_->unary(contract::kMediaCreateUpload, request,
                   [done = std::move(done)](network::ApiResult<mediav1::CreateUploadResponse> result) {
                       if (result && (result->upload_url().empty() || result->media().id().empty())) {
                           done(std::unexpected(network::ApiError{.kind = network::ApiErrorKind::Decoding,
                                                                  .httpStatus = 200,
                                                                  .message = "Incomplete CreateUploadResponse"}));
                           return;
                       }
                       done(std::move(result));
                   });
}

void ConnectMediaRemoteDataSource::completeUpload(const mediav1::CompleteUploadRequest& request,
                                                  network::ApiCompletion<mediav1::CompleteUploadResponse> done) {
    client_->unary(contract::kMediaCompleteUpload, request, std::move(done));
}

void ConnectMediaRemoteDataSource::getMedia(const mediav1::GetMediaRequest& request,
                                            network::ApiCompletion<mediav1::GetMediaResponse> done) {
    client_->unary(contract::kMediaGetMedia, request, std::move(done));
}

void ConnectMediaRemoteDataSource::listMedia(const mediav1::ListMediaRequest& request,
                                             network::ApiCompletion<mediav1::ListMediaResponse> done) {
    client_->unary(contract::kMediaListMedia, request, std::move(done));
}

void ConnectMediaRemoteDataSource::deleteMedia(const mediav1::DeleteMediaRequest& request,
                                               network::ApiCompletion<void> done) {
    client_->unary(contract::kMediaDeleteMedia, request,
                   [done = std::move(done)](network::ApiResult<mediav1::DeleteMediaResponse> result) {
                       if (!result) {
                           done(std::unexpected(std::move(result.error())));
                           return;
                       }
                       done(network::ApiResult<void>{});
                   });
}

} // namespace aedst::media
