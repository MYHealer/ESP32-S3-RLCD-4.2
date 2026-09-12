// 实现自定义资源条目的固定容量索引和确定性图库排序。
#include "custom_asset_catalog.h"

#include <string.h>

void CustomAssetCatalog::reset()
{
    main_gif_ = nullptr;
    memset(gallery_, 0, sizeof(gallery_));
    weather_city_ = nullptr;
    ota_manifest_url_ = nullptr;
    gallery_count_ = 0;
}

CustomAssetCatalogStatus CustomAssetCatalog::add(const CustomAssetEntry *entry)
{
    if (!entry) {
        return CustomAssetCatalogStatus::kIgnoredUnsupportedType;
    }
    if (entry->type == kCustomAssetTypeMainGif) {
        if (main_gif_) {
            return CustomAssetCatalogStatus::kDuplicateMainGif;
        }
        main_gif_ = entry;
        return CustomAssetCatalogStatus::kAdded;
    }
    if (entry->type == kCustomAssetTypeGalleryImage) {
        // Preserve the established fixed-capacity behavior. Shape validation limits
        // valid gallery indexes to this same range before entries reach the catalog.
        if (gallery_count_ >= kMaxGalleryImages) {
            return CustomAssetCatalogStatus::kIgnoredGalleryCapacity;
        }
        for (int i = 0; i < gallery_count_; ++i) {
            if (gallery_[i] && gallery_[i]->index == entry->index) {
                return CustomAssetCatalogStatus::kDuplicateGallery;
            }
        }
        gallery_[gallery_count_++] = entry;
        return CustomAssetCatalogStatus::kAdded;
    }
    if (entry->type == kCustomAssetTypeWeatherCity) {
        if (weather_city_) {
            return CustomAssetCatalogStatus::kDuplicateConfig;
        }
        weather_city_ = entry;
        return CustomAssetCatalogStatus::kAdded;
    }
    if (entry->type == kCustomAssetTypeOtaManifestUrl) {
        if (ota_manifest_url_) {
            return CustomAssetCatalogStatus::kDuplicateConfig;
        }
        ota_manifest_url_ = entry;
        return CustomAssetCatalogStatus::kAdded;
    }
    return CustomAssetCatalogStatus::kIgnoredUnsupportedType;
}

void CustomAssetCatalog::sort_gallery_by_index()
{
    for (int i = 0; i < gallery_count_ - 1; ++i) {
        for (int j = i + 1; j < gallery_count_; ++j) {
            if (gallery_[j]->index < gallery_[i]->index) {
                const CustomAssetEntry *entry = gallery_[i];
                gallery_[i] = gallery_[j];
                gallery_[j] = entry;
            }
        }
    }
}

bool CustomAssetCatalog::available() const
{
    return main_gif_ || gallery_count_ > 0 || weather_city_ || ota_manifest_url_;
}

const CustomAssetEntry *CustomAssetCatalog::main_gif() const
{
    return main_gif_;
}

int CustomAssetCatalog::gallery_count() const
{
    return gallery_count_;
}

const CustomAssetEntry *CustomAssetCatalog::gallery_at(int index) const
{
    return index >= 0 && index < gallery_count_ ? gallery_[index] : nullptr;
}

const CustomAssetEntry *CustomAssetCatalog::weather_city() const
{
    return weather_city_;
}

const CustomAssetEntry *CustomAssetCatalog::ota_manifest_url() const
{
    return ota_manifest_url_;
}
