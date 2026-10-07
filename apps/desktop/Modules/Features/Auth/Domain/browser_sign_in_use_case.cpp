#include "Domain/browser_sign_in_use_case.hpp"

#include <utility>

namespace aedst::feature::auth {

BrowserSignInUseCase::BrowserSignInUseCase(std::shared_ptr<AuthRepository> repository)
    : repository_(std::move(repository)) {}

void BrowserSignInUseCase::execute(core::NoInput /*input*/, core::Completion<Session> done) {
    if (!repository_->supportsBrowserSignIn()) {
        done(core::fail(core::Error::invalidArgument("Browser sign-in is not available")));
        return;
    }
    repository_->signInWithBrowser(std::move(done));
}

void BrowserSignInUseCase::cancel() {
    repository_->cancelBrowserSignIn();
}

bool BrowserSignInUseCase::isAvailable() const {
    return repository_->supportsBrowserSignIn();
}

} // namespace aedst::feature::auth
