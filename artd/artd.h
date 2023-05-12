#include "android-base/result.h"
#include "android/binder_auto_utils.h"

namespace art {
namespace artd {

class Artd : public aidl::com::android::server::art::BnArtd {
 public:
  ndk::ScopedAStatus isAlive(bool* _aidl_return) override;

  android::base::Result<void> Start();
};

}  // namespace artd
}  // namespace art

#endif  // ART_ARTD_ARTD_H_
