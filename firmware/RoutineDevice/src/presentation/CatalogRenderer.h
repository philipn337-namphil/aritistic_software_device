#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\presentation\\CatalogRenderer.h"
#pragma once
#include "../application/ProductState.h"
#include "../application/ProductView.h"

namespace routine::ui {
// Catalog page body and theme. Clock and confirmation overlays belong to the caller.
void renderCatalogPage(uint16_t* dst, const application::ProductState& state,
                       const application::ProductView& view);
}
