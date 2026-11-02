#include "backend.hpp"
#include "toliss/backend.hpp"
#include "lr/backend.hpp"

namespace oans::aircraft {
const Backend* detect() {
    // Explicit ACF studio identity takes precedence over generic registry names.
    const Backend* backends[]={&lr::backend(),&toliss::backend()};
    for(const auto* backend:backends)if(backend->matches())return backend;
    return nullptr;
}
}
