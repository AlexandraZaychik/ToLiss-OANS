#include "backend.hpp"
#include "toliss/backend.hpp"
#include "lr/backend.hpp"
#include "zibo/backend.hpp"

namespace oans::aircraft {
const Backend* detect() {
    // Explicit ACF studio identity takes precedence over generic registry names.
    const Backend* backends[]={&zibo::backend(),&lr::backend(),&toliss::backend()};
    for(const auto* backend:backends)if(backend->matches())return backend;
    return nullptr;
}
}
