#include "custom_asset_catalog.h"

#include <assert.h>

static CustomAssetEntry entry(uint16_t type, uint16_t index)
{
    CustomAssetEntry value = {};
    value.type = type;
    value.index = index;
    return value;
}

int main()
{
    CustomAssetCatalog catalog;
    catalog.reset();
    assert(!catalog.available());
    assert(catalog.main_gif() == nullptr);
    assert(catalog.gallery_count() == 0);
    assert(catalog.gallery_at(-1) == nullptr);
    assert(catalog.gallery_at(0) == nullptr);

    CustomAssetEntry gif = entry(kCustomAssetTypeMainGif, 0);
    CustomAssetEntry duplicate_gif = entry(kCustomAssetTypeMainGif, 0);
    assert(catalog.add(&gif) == CustomAssetCatalogStatus::kAdded);
    assert(catalog.add(&duplicate_gif) == CustomAssetCatalogStatus::kDuplicateMainGif);
    assert(catalog.main_gif() == &gif);

    CustomAssetEntry gallery_9 = entry(kCustomAssetTypeGalleryImage, 9);
    CustomAssetEntry gallery_2 = entry(kCustomAssetTypeGalleryImage, 2);
    CustomAssetEntry gallery_5 = entry(kCustomAssetTypeGalleryImage, 5);
    CustomAssetEntry duplicate_gallery_2 = entry(kCustomAssetTypeGalleryImage, 2);
    assert(catalog.add(&gallery_9) == CustomAssetCatalogStatus::kAdded);
    assert(catalog.add(&gallery_2) == CustomAssetCatalogStatus::kAdded);
    assert(catalog.add(&gallery_5) == CustomAssetCatalogStatus::kAdded);
    assert(catalog.add(&duplicate_gallery_2) == CustomAssetCatalogStatus::kDuplicateGallery);
    catalog.sort_gallery_by_index();
    assert(catalog.gallery_count() == 3);
    assert(catalog.gallery_at(0) == &gallery_2);
    assert(catalog.gallery_at(1) == &gallery_5);
    assert(catalog.gallery_at(2) == &gallery_9);
    assert(catalog.gallery_at(3) == nullptr);

    CustomAssetEntry city = entry(kCustomAssetTypeWeatherCity, 0);
    CustomAssetEntry duplicate_city = entry(kCustomAssetTypeWeatherCity, 0);
    CustomAssetEntry ota = entry(kCustomAssetTypeOtaManifestUrl, 0);
    CustomAssetEntry duplicate_ota = entry(kCustomAssetTypeOtaManifestUrl, 0);
    assert(catalog.add(&city) == CustomAssetCatalogStatus::kAdded);
    assert(catalog.add(&duplicate_city) == CustomAssetCatalogStatus::kDuplicateConfig);
    assert(catalog.add(&ota) == CustomAssetCatalogStatus::kAdded);
    assert(catalog.add(&duplicate_ota) == CustomAssetCatalogStatus::kDuplicateConfig);
    assert(catalog.weather_city() == &city);
    assert(catalog.ota_manifest_url() == &ota);
    assert(catalog.available());

    CustomAssetEntry unsupported = entry(99, 0);
    assert(catalog.add(nullptr) == CustomAssetCatalogStatus::kIgnoredUnsupportedType);
    assert(catalog.add(&unsupported) == CustomAssetCatalogStatus::kIgnoredUnsupportedType);

    catalog.reset();
    assert(!catalog.available());
    assert(catalog.main_gif() == nullptr);
    assert(catalog.weather_city() == nullptr);
    assert(catalog.ota_manifest_url() == nullptr);
    assert(catalog.gallery_count() == 0);

    CustomAssetEntry full_gallery[CustomAssetCatalog::kMaxGalleryImages + 1] = {};
    for (int i = 0; i < CustomAssetCatalog::kMaxGalleryImages; ++i) {
        full_gallery[i] = entry(kCustomAssetTypeGalleryImage, static_cast<uint16_t>(i));
        assert(catalog.add(&full_gallery[i]) == CustomAssetCatalogStatus::kAdded);
    }
    full_gallery[CustomAssetCatalog::kMaxGalleryImages] = entry(kCustomAssetTypeGalleryImage, 0);
    assert(catalog.add(&full_gallery[CustomAssetCatalog::kMaxGalleryImages]) ==
           CustomAssetCatalogStatus::kIgnoredGalleryCapacity);
    assert(catalog.gallery_count() == CustomAssetCatalog::kMaxGalleryImages);
    return 0;
}
