#include "Products.h"
#include "../scene/Materials.h"

float productHeight(ProductShape s) {
    switch (s) {
        case P_CAN: return 0.12f;
        case P_BOTTLE: return 0.19f;
        case P_BAG: return 0.17f;
        case P_BAR: return 0.035f;
        case P_BOX: return 0.06f;
        case P_JUICE: return 0.11f;
    }
    return 0.1f;
}

void drawProduct(const Node& n, const Product& p) {
    Material main = Material(p.c1, 0.55f, 64.0f);
    Material acc = Material(p.c2, 0.4f, 48.0f);
    Material metal = Mat::chrome();
    switch (p.shape) {
        case P_CAN:
            n.cyl({0, 0.06f, 0}, {0.064f, 0.11f, 0.064f}, main);
            n.cyl({0, 0.06f, 0}, {0.0645f, 0.035f, 0.0645f}, acc);
            n.cyl({0, 0.117f, 0}, {0.058f, 0.008f, 0.058f}, metal);
            n.cyl({0, 0.003f, 0}, {0.058f, 0.006f, 0.058f}, metal);
            break;
        case P_BOTTLE: {
            Material plastic = Material(p.c1, 0.9f, 128.0f);
            n.cyl({0, 0.055f, 0}, {0.062f, 0.11f, 0.062f}, plastic);
            n.tcone({0, 0.13f, 0}, {0.062f, 0.04f, 0.062f}, plastic);
            n.cyl({0, 0.165f, 0}, {0.026f, 0.03f, 0.026f}, plastic);
            n.cyl({0, 0.185f, 0}, {0.03f, 0.016f, 0.03f}, acc);
            n.cyl({0, 0.06f, 0}, {0.0635f, 0.045f, 0.0635f}, Material(Vec3(0.95f), 0.2f, 16.0f));
            n.cyl({0, 0.06f, 0}, {0.064f, 0.012f, 0.064f}, acc);
            break;
        }
        case P_BAG:
            n.box({0, 0.085f, 0}, {0.13f, 0.15f, 0.04f}, main);
            n.sph({0, 0.085f, 0}, {0.125f, 0.13f, 0.06f}, main);
            n.box({0, 0.163f, 0}, {0.135f, 0.014f, 0.016f}, acc);
            n.box({0, 0.007f, 0}, {0.135f, 0.014f, 0.016f}, acc);
            n.cyl({0, 0.09f, 0.03f}, {0.05f, 0.004f, 0.05f}, acc, {90, 0, 0});
            break;
        case P_BAR:
            n.box({0, 0.0175f, 0}, {0.15f, 0.035f, 0.05f}, main);
            n.box({0, 0.0175f, 0}, {0.06f, 0.037f, 0.052f}, acc);
            break;
        case P_BOX:
            n.box({0, 0.03f, 0}, {0.14f, 0.06f, 0.07f}, main);
            n.box({0, 0.03f, 0.036f}, {0.14f, 0.02f, 0.002f}, acc);
            n.cyl({0.03f, 0.04f, 0.037f}, {0.025f, 0.002f, 0.025f}, Material(Vec3(0.8f, 0.6f, 0.35f)), {90, 0, 0});
            break;
        case P_JUICE:
            n.box({0, 0.05f, 0}, {0.065f, 0.10f, 0.045f}, main);
            n.box({0, 0.05f, 0.023f}, {0.06f, 0.04f, 0.001f}, acc);
            n.cyl({0.018f, 0.115f, 0}, {0.008f, 0.04f, 0.008f}, Material(Vec3(0.95f, 0.95f, 0.98f)), {0, 0, 12});
            break;
    }
}
