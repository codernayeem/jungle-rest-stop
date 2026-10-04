// =============================================================================
//  Products.h  -  Items sold by the vending machines, each built from shapes:
//    can    = cylinder body + 2 metal rim cylinders + colour band cylinder
//    bottle = cylinder body + truncated-cone shoulder + neck + cap + label
//    bag    = box body + flattened sphere "puff" + 2 crimp strips
//    bar    = box wrapper + band box
//    box    = box + stripe (cookies / crackers)
//    juice  = box carton + straw cylinder
// =============================================================================
#pragma once
#include <string>
#include "../gfx/Draw.h"

enum ProductShape { P_CAN, P_BOTTLE, P_BAG, P_BAR, P_BOX, P_JUICE };

struct Product {
    std::string name;
    int price;            // Taka
    ProductShape shape;
    Vec3 c1, c2;          // main / accent colour
};

// Draw a product with its base centre at the node origin, facing +Z
void drawProduct(const Node& n, const Product& p);
// Height of the product (used to stack / drop it)
float productHeight(ProductShape s);
