// 管理已校验自定义资源条目的固定容量索引，不负责 Flash 读取或格式校验。
#pragma once

#include "custom_assets.h"

enum class CustomAssetCatalogStatus {
    kAdded = 0,
    kDuplicateMainGif,
    kDuplicateGallery,
    kDuplicateConfig,
    kIgnoredGalleryCapacity,
    kIgnoredUnsupportedType,
};

class CustomAssetCatalog {
public:
    static constexpr int kMaxGalleryImages = 24;

    void reset();
    CustomAssetCatalogStatus add(const CustomAssetEntry *entry);
    void sort_gallery_by_index();

    bool available() const;
    const CustomAssetEntry *main_gif() const;
    int gallery_count() const;
    const CustomAssetEntry *gallery_at(int index) const;
    const CustomAssetEntry *weather_city() const;
    const CustomAssetEntry *ota_manifest_url() const;

private:
    const CustomAssetEntry *main_gif_ = nullptr;
    const CustomAssetEntry *gallery_[kMaxGalleryImages] = {};
    const CustomAssetEntry *weather_city_ = nullptr;
    const CustomAssetEntry *ota_manifest_url_ = nullptr;
    int gallery_count_ = 0;
};
